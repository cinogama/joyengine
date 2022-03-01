#define JE_IMPL
#include "jeecs.hpp"

#include <list>
#include <thread>

#ifdef NDEBUG
#   define DEBUG_ARCH_LOG(...) ((void)0)
#   define DEBUG_ARCH_LOG_WARN(...) ((void)0)
#else
#   define DEBUG_ARCH_LOG(...) jeecs::debug::log_info( __VA_ARGS__ );
#   define DEBUG_ARCH_LOG_WARN(...) jeecs::debug::log_warn( __VA_ARGS__ );
#endif

namespace jeecs_impl
{
    using types_set = std::set<jeecs::typing::typeid_t>;

    constexpr jeecs::typing::entity_id_in_chunk_t INVALID_ENTITY_ID = SIZE_MAX;
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64K

    class command_buffer;
    class arch_manager;
    class ecs_world;

    inline bool is_system_component_depends(jeecs::typing::typeid_t id)
    {
        if (jeecs::typing::type_info::of(id) == nullptr)
            return true;
        return false;
    }

    class arch_type
    {
        JECS_DISABLE_MOVE_AND_COPY(arch_type);
        // ahahahah, arch_type is coming!

        struct arch_type_info
        {
            const jeecs::typing::type_info* m_typeinfo;
            size_t m_begin_offset_in_chunk;
        };

        using types_list = std::vector<const jeecs::typing::type_info*>;
        using archtypes_map = std::unordered_map<jeecs::typing::typeid_t, arch_type_info>;

        const types_list      _m_arch_typeinfo;
        const types_set       _m_types_set;
        const archtypes_map   _m_arch_typeinfo_mapping;
        const size_t    _m_entity_size;
        const size_t    _m_entity_count_per_chunk;

        arch_manager* _m_arch_manager;

    public:
        class arch_chunk
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_chunk);
        public:
            struct entity_meta
            {
                std::atomic_flag m_in_used = {};
                jeecs::typing::version_t m_version = 0;

                jeecs::game_entity::entity_stat m_stat = jeecs::game_entity::entity_stat::UNAVAILABLE;
            };

        private:

            using byte_t = uint8_t;
            static_assert(sizeof(byte_t) == 1, "sizeof(uint8_t) should be 1.");

            byte_t _m_chunk_buffer[CHUNK_SIZE];

            const types_set& _m_types;
            const archtypes_map& _m_arch_typeinfo_mapping;
            const size_t _m_entity_count;
            const size_t _m_entity_size;

            entity_meta* _m_entities_meta;
            std::atomic_size_t _m_free_count;
            arch_type* _m_arch_type;
        public:
            arch_chunk* last; // for atomic_list;
        public:
            arch_chunk(arch_type* _arch_type)
                : _m_entity_count(_arch_type->_m_entity_count_per_chunk)
                , _m_entity_size(_arch_type->_m_entity_size)
                , _m_free_count(_arch_type->_m_entity_count_per_chunk)
                , _m_arch_typeinfo_mapping(_arch_type->_m_arch_typeinfo_mapping)
                , _m_types(_arch_type->_m_types_set)
                , _m_arch_type(_arch_type)
            {
                static_assert(
                    ((size_t) & reinterpret_cast<char const volatile&>((
                        ((jeecs_impl::arch_type::arch_chunk*)0)->_m_chunk_buffer))) == 0);

                _m_entities_meta = jeecs::basic::create_new_n<entity_meta>(_m_entity_count);
            }
            ~arch_chunk()
            {
                // All entity in chunk should be free.
                assert(_m_free_count == _m_entity_count);
                jeecs::basic::destroy_free_n(_m_entities_meta, _m_entity_count);
            }

        public:
            // ATTENTION: move_component_to WILL INVOKE DESTRUCT FUNCTION OF from_component
            inline void move_component_to(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid, void* from_component)const
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                void* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);
                arch_typeinfo.m_typeinfo->move(component_addr, from_component);
                arch_typeinfo.m_typeinfo->destruct(from_component);
            }

            bool alloc_entity_id(jeecs::typing::entity_id_in_chunk_t* out_id, jeecs::typing::version_t* out_version)
            {
                size_t free_entity_count = _m_free_count;
                while (free_entity_count)
                {
                    if (_m_free_count.compare_exchange_weak(free_entity_count, free_entity_count - 1))
                    {
                        // OK There is a usable place for entity
                        for (size_t id = 0; id < _m_entity_count; id++)
                        {
                            if (!_m_entities_meta[id].m_in_used.test_and_set())
                            {
                                *out_id = id;
                                *out_version = ++(_m_entities_meta[id].m_version);
                                return true;
                            }
                        }
                        assert(false); // entity count is ok, but there is no free place. that should not happend.
                    }
                    free_entity_count = _m_free_count; // Fail to compare, update the count and retry.
                }
                return false;
            }
            inline void* get_component_addr(jeecs::typing::entity_id_in_chunk_t _eid, size_t _chunksize, size_t _offset)const noexcept
            {
                return (void*)(_m_chunk_buffer + _offset + _eid * _chunksize);
            }
            inline bool is_entity_valid(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::version_t eversion) const noexcept
            {
                if (_m_entities_meta[eid].m_version != eversion)
                    return false;
                return true;
            }
            inline void* get_component_addr_with_typeid(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid) const noexcept
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                return get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

            }
            inline void destruct_component_addr_with_typeid(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid) const noexcept
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                auto* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

                arch_typeinfo.m_typeinfo->destruct(component_addr);

            }
            inline const types_set& types()const noexcept
            {
                return _m_types;
            }
            inline arch_type* get_arch_type()const noexcept
            {
                return _m_arch_type;
            }
            inline const entity_meta* get_entity_meta()const noexcept
            {
                return _m_entities_meta;
            }
            inline void close_all_entity(ecs_world* by_world)
            {
                for (jeecs::typing::entity_id_in_chunk_t eidx = 0; eidx < _m_entity_count; eidx++)
                {
                    if (_m_entities_meta[eidx].m_in_used.test_and_set())
                    {
                        jeecs::game_entity gentity;
                        gentity._m_id = eidx;
                        gentity._m_in_chunk = this;
                        gentity._m_version = _m_entities_meta[eidx].m_version;

                        je_ecs_world_destroy_entity(by_world, &gentity);
                    }
                }
            }

        private:
            // Following function only invoke by command_buffer
            friend class command_buffer;

            void command_active_entity(jeecs::typing::entity_id_in_chunk_t eid) noexcept
            {
                _m_entities_meta[eid].m_stat = jeecs::game_entity::entity_stat::READY;
            }

            void command_close_entity(jeecs::typing::entity_id_in_chunk_t eid) noexcept
            {
                // TODO: MULTI-THREAD PROBLEM
                _m_entities_meta[eid].m_stat = jeecs::game_entity::entity_stat::UNAVAILABLE;
                ++_m_entities_meta[eid].m_version;
                _m_entities_meta[eid].m_in_used.clear();

                ++_m_free_count;
                ++_m_arch_type->_m_free_count;
            }
        };

    private:

        std::atomic_size_t _m_free_count;
        jeecs::basic::atomic_list<arch_chunk> _m_chunks;

    public:
        struct entity
        {
            arch_chunk* _m_in_chunk;
            jeecs::typing::entity_id_in_chunk_t   _m_id;
            jeecs::typing::version_t              _m_version;

            // Do not invoke this function if possiable, you should get component by arch_type & system.
            template<typename CT>
            inline CT* get_component() const
            {
                assert(_m_in_chunk->is_entity_valid(_m_id, _m_version));
                return (CT*)_m_in_chunk->get_component_addr_with_typeid(_m_id, jeecs::typing::type_info::id<CT>());
            }

            inline arch_chunk* chunk()const noexcept
            {
                return _m_in_chunk;
            }

            inline bool operator < (const entity& another) const noexcept
            {
                if (_m_in_chunk < another._m_in_chunk)
                    return true;
                if (_m_in_chunk > another._m_in_chunk)
                    return false;
                if (_m_id < another._m_id)
                    return true;
                return false;
            }

            inline bool valid() const noexcept
            {
                return _m_in_chunk->is_entity_valid(_m_id, _m_version);
            }
        };

    public:
        arch_type(arch_manager* _arch_manager, const types_set& _types_set)
            : _m_entity_size(0)
            , _m_free_count(0)
            , _m_entity_count_per_chunk(0)
            , _m_types_set(_types_set)
            , _m_arch_manager(_arch_manager)
        {
            static_assert(offsetof(jeecs::game_entity, _m_in_chunk)
                == offsetof(entity, _m_in_chunk));
            static_assert(offsetof(jeecs::game_entity, _m_id)
                == offsetof(entity, _m_id));
            static_assert(offsetof(jeecs::game_entity, _m_version)
                == offsetof(entity, _m_version));

            for (jeecs::typing::typeid_t tid : _types_set)
                const_cast<types_list&>(_m_arch_typeinfo).push_back(jeecs::typing::type_info::of(tid));

            std::sort(const_cast<types_list&>(_m_arch_typeinfo).begin(),
                const_cast<types_list&>(_m_arch_typeinfo).end(),
                [](const jeecs::typing::type_info* a, const jeecs::typing::type_info* b) {
                    return a->m_size < b->m_size;
                });

            const_cast<size_t&>(_m_entity_size) = 0;
            for (auto* typeinfo : _m_arch_typeinfo)
            {
                const_cast<size_t&>(_m_entity_size) += typeinfo->m_chunk_size;
            }

            const_cast<size_t&>(_m_entity_count_per_chunk) = CHUNK_SIZE / _m_entity_size;

            size_t mem_offset = 0;
            for (auto* typeinfo : _m_arch_typeinfo)
            {
                const_cast<archtypes_map&>(_m_arch_typeinfo_mapping)[typeinfo->m_id]
                    = arch_type_info{ typeinfo, mem_offset };
                mem_offset += typeinfo->m_chunk_size * _m_entity_count_per_chunk;
            }
        }

        ~arch_type()
        {
            arch_chunk* chunk = _m_chunks.pick_all();
            while (chunk)
            {
                auto* next_chunk = chunk->last;
                jeecs::basic::destroy_free(chunk);

                chunk = next_chunk;
            }
        }

        arch_chunk* _create_new_chunk()
        {
            arch_chunk* new_chunk = jeecs::basic::create_new<arch_chunk>(this);
            _m_chunks.add_one(new_chunk);
            _m_free_count += _m_entity_count_per_chunk;
            return new_chunk;
        }

        void alloc_entity(arch_chunk** out_chunk, jeecs::typing::entity_id_in_chunk_t* out_eid, jeecs::typing::version_t* out_eversion)
        {
            while (true)
            {
                size_t free_entity_count = _m_free_count;
                while (free_entity_count)
                {
                    if (_m_free_count.compare_exchange_weak(free_entity_count, free_entity_count - 1))
                    {
                        // OK There is a usable place for entity
                        arch_chunk* peek_chunk = _m_chunks.peek();
                        while (peek_chunk)
                        {
                            if (peek_chunk->alloc_entity_id(out_eid, out_eversion))
                            {
                                *out_chunk = peek_chunk;
                                return;
                            }
                            peek_chunk = peek_chunk->last;
                        }

                        assert(false); // entity count is ok, but there is no free place. that should not happend.
                    }
                    free_entity_count = _m_free_count; // Fail to compare, update the count and retry.
                }
                _create_new_chunk();
            }
        }

        entity instance_entity()
        {
            arch_chunk* chunk;
            jeecs::typing::entity_id_in_chunk_t entity_id;
            jeecs::typing::version_t            entity_version;

            alloc_entity(&chunk, &entity_id, &entity_version);
            for (auto& arch_typeinfo : _m_arch_typeinfo_mapping)
            {
                void* component_addr = chunk->get_component_addr(entity_id,
                    arch_typeinfo.second.m_typeinfo->m_chunk_size,
                    arch_typeinfo.second.m_begin_offset_in_chunk);

                arch_typeinfo.second.m_typeinfo->construct(component_addr);
            }

            return entity{ chunk ,entity_id, entity_version };
        }

        arch_manager* get_arch_mgr()const noexcept
        {
            return _m_arch_manager;
        }

        arch_chunk* get_head_chunk() const noexcept
        {
            return _m_chunks.peek();
        }

        size_t get_entity_count_per_chunk() const noexcept
        {
            return _m_entity_count_per_chunk;
        }

        const arch_type_info* get_arch_type_info_by_type_id(jeecs::typing::typeid_t tid) const
        {
            return &_m_arch_typeinfo_mapping.at(tid);
        }

        inline void close_all_entity(ecs_world* by_world)
        {
            auto* chunk = get_head_chunk();
            while (chunk)
            {
                chunk->close_all_entity(by_world);

                chunk = chunk->last;
            }
        }
    };

    struct ecs_system_function
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_system_function);

        enum sequence
        {
            CAN_HAPPEND_SAME_TIME,
            WRITE_CONFLICT,
            ONLY_HAPPEND_BEFORE,
            ONLY_HAPPEND_AFTER,
            UNABLE_DETERMINE,
        };
        using dependences_t = std::unordered_map<jeecs::typing::typeid_t, jeecs::game_system_function::dependence_type>;

    public:
        dependences_t m_dependence_list;
        std::vector<arch_type*> m_arch_types;
        jeecs::game_system_function* m_game_system_function;

    public:
        ecs_system_function(jeecs::game_system_function* gsf)
            :m_game_system_function(gsf)
        {
            for (size_t dindex = 0; dindex < gsf->m_dependence_count; dindex++)
                m_dependence_list[gsf->m_dependence[dindex].m_tid] = gsf->m_dependence[dindex].m_depend;
        }
        ~ecs_system_function()
        {
            if (m_game_system_function->m_rw_component_count)
            {
                if (m_game_system_function->m_archs
                    && m_game_system_function->m_arch_count)
                {
                    // Free old arch infos
                    for (size_t aindex = 0; aindex < m_game_system_function->m_arch_count; aindex++)
                    {
                        auto& ainfo = m_game_system_function->m_archs[aindex];
                        je_mem_free(ainfo.m_component_sizes);
                        je_mem_free(ainfo.m_component_mem_begin_offsets);
                    }

                    je_mem_free(m_game_system_function->m_archs);
                }
            }
            jeecs::game_system_function::destory(m_game_system_function);
        }
    public:

        sequence check_dependence(const dependences_t& depend) const noexcept
        {
            bool write_conflict = false;
            sequence result = sequence::CAN_HAPPEND_SAME_TIME;

            auto decided_depend_seq = [&](sequence seq) {
                if (seq == sequence::UNABLE_DETERMINE || result == sequence::UNABLE_DETERMINE)
                    return sequence::UNABLE_DETERMINE;
                if (result == sequence::CAN_HAPPEND_SAME_TIME)
                    return seq;
                if (result == sequence::ONLY_HAPPEND_BEFORE)
                {
                    if (seq == sequence::ONLY_HAPPEND_AFTER)
                        return sequence::UNABLE_DETERMINE;
                    return sequence::ONLY_HAPPEND_BEFORE;
                }
                if (result == sequence::ONLY_HAPPEND_AFTER)
                {
                    if (seq == sequence::ONLY_HAPPEND_BEFORE)
                        return sequence::UNABLE_DETERMINE;
                    return sequence::ONLY_HAPPEND_AFTER;
                }
                return sequence::UNABLE_DETERMINE;
            };

            for (auto [depend_tid, depend_type] : m_dependence_list)
            {
                // Check dependence
                auto fnd = depend.find(depend_tid);

                if (fnd == depend.end())
                {
                    // NO REQUIRE, JUST GOON..
                }
                else
                {
                    auto aim_require_type = fnd->second;

                    if ((depend_type == jeecs::game_system_function::dependence_type::EXCEPT
                        || aim_require_type == jeecs::game_system_function::dependence_type::EXCEPT)
                        && depend_type != aim_require_type)
                    {
                        // These two set will not meet at same time, just skip them
                        return sequence::CAN_HAPPEND_SAME_TIME;
                    }
                    else if (aim_require_type == jeecs::game_system_function::dependence_type::EXCEPT)
                    {
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == jeecs::game_system_function::dependence_type::WRITE)
                    {
                        if (depend_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                        else if (depend_type == jeecs::game_system_function::dependence_type::WRITE)
                        {
                            write_conflict = true;
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        }
                        else if (depend_type == jeecs::game_system_function::dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                    {
                        if (depend_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else if (depend_type == jeecs::game_system_function::dependence_type::WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        else if (depend_type == jeecs::game_system_function::dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == jeecs::game_system_function::dependence_type::READ_AFTER_WRITE)
                    {
                        if (depend_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else if (depend_type == jeecs::game_system_function::dependence_type::WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                        else if (depend_type == jeecs::game_system_function::dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                }
            }

            if (write_conflict && result == sequence::CAN_HAPPEND_SAME_TIME)
                return sequence::WRITE_CONFLICT;

            return result;
        }


    };

    class arch_manager
    {
        JECS_DISABLE_MOVE_AND_COPY(arch_manager);

        using arch_map_t = std::map<types_set, arch_type*>;

        arch_map_t _m_arch_types_mapping;
        mutable std::shared_mutex _m_arch_types_mapping_mx;

        std::atomic_flag _m_arch_modified = {};

    public:
        arch_manager() = default;
        ~arch_manager()
        {
            for (auto& [types, archtype] : _m_arch_types_mapping)
                jeecs::basic::destroy_free(archtype);
        }

        arch_type* find_or_add_arch(const types_set& _types)
        {
            do
            {
                std::shared_lock sg1(_m_arch_types_mapping_mx);
                auto fnd = _m_arch_types_mapping.find(_types);
                if (fnd != _m_arch_types_mapping.end())
                {
                    return fnd->second;
                }
            } while (0);

            std::lock_guard g1(_m_arch_types_mapping_mx);
            arch_type*& atype = _m_arch_types_mapping[_types];
            if (nullptr == atype)
                atype = jeecs::basic::create_new<arch_type>(this, _types);
            _m_arch_modified.clear();
            return atype;
        }
        arch_type::entity create_an_entity_with_component(const types_set& _types)
        {
            assert(!_types.empty());
            return find_or_add_arch(_types)->instance_entity();
        }
    public:
        // Only invoke by update..
        inline bool _arch_modified() noexcept
        {
            return !_m_arch_modified.test_and_set();
        }

        inline void _update_system_func_arch(ecs_system_function* modify_sys_func) const
        {
            // TODO: OPTMIZE..

            types_set need_set, any_set, except_set;
            for (auto& depend : modify_sys_func->m_dependence_list)
            {
                if (is_system_component_depends(depend.first))
                {
                    // Do nothing.
                }
                else
                {
                    switch (depend.second)
                    {
                    case jeecs::game_system_function::dependence_type::ANY:
                        any_set.insert(depend.first); break;
                    case jeecs::game_system_function::dependence_type::EXCEPT:
                        except_set.insert(depend.first); break;
                    case jeecs::game_system_function::dependence_type::CONTAIN:
                    case jeecs::game_system_function::dependence_type::READ_AFTER_WRITE:
                    case jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME:
                    case jeecs::game_system_function::dependence_type::WRITE:
                        need_set.insert(depend.first); break;
                    default:
                        assert(false); //  Unknown type
                    }
                }
            }

            static auto contain = [](const types_set& a, const types_set& b)
            {
                for (auto type_id : b)
                    if (a.find(type_id) == a.end())
                        return false;
                return true;
            };
            static auto contain_any = [](const types_set& a, const types_set& b)
            {
                for (auto type_id : b)
                    if (a.find(type_id) != a.end())
                        return true;
                return b.empty();
            };
            static auto except = [](const types_set& a, const types_set& b)
            {
                for (auto type_id : b)
                    if (a.find(type_id) != a.end())
                        return false;
                return true;
            };

            // OK Get fetched arch_types;
            modify_sys_func->m_arch_types.clear();

            do
            {
                std::shared_lock sg1(_m_arch_types_mapping_mx);
                for (auto& [typeset, arch] : _m_arch_types_mapping)
                {
                    if (contain(typeset, need_set)
                        && contain_any(typeset, any_set)
                        && except(typeset, except_set))
                    {
                        modify_sys_func->m_arch_types.push_back(arch);
                    }
                }
            } while (0);

            // Update game_system..
            if (modify_sys_func->m_game_system_function->m_rw_component_count)
            {
                if (modify_sys_func->m_game_system_function->m_archs
                    && modify_sys_func->m_game_system_function->m_arch_count)
                {
                    // Free old arch infos
                    for (size_t aindex = 0; aindex < modify_sys_func->m_game_system_function->m_arch_count; aindex++)
                    {
                        auto& ainfo = modify_sys_func->m_game_system_function->m_archs[aindex];
                        je_mem_free(ainfo.m_component_sizes);
                        je_mem_free(ainfo.m_component_mem_begin_offsets);
                    }

                    je_mem_free(modify_sys_func->m_game_system_function->m_archs);
                }

                modify_sys_func->m_game_system_function->m_arch_count = modify_sys_func->m_arch_types.size();
                if (modify_sys_func->m_game_system_function->m_arch_count)
                {
                    modify_sys_func->m_game_system_function->m_archs = (jeecs::game_system_function::arch_index_info*)je_mem_alloc(
                        modify_sys_func->m_game_system_function->m_arch_count * sizeof(jeecs::game_system_function::arch_index_info));

                    for (size_t aindex = 0; aindex < modify_sys_func->m_game_system_function->m_arch_count; aindex++)
                    {
                        auto& ainfo = modify_sys_func->m_game_system_function->m_archs[aindex];
                        ainfo.m_archtype = modify_sys_func->m_arch_types[aindex];
                        ainfo.m_entity_count_per_arch_chunk = modify_sys_func->m_arch_types[aindex]->get_entity_count_per_chunk();

                        ainfo.m_component_sizes = (size_t*)je_mem_alloc(modify_sys_func->m_game_system_function->m_rw_component_count * sizeof(size_t));
                        ainfo.m_component_mem_begin_offsets = (size_t*)je_mem_alloc(modify_sys_func->m_game_system_function->m_rw_component_count * sizeof(size_t));
                        for (size_t cindex = 0; cindex < modify_sys_func->m_game_system_function->m_rw_component_count; cindex++)
                        {
                            auto* arch_typeinfo = modify_sys_func->m_arch_types[aindex]->get_arch_type_info_by_type_id(
                                modify_sys_func->m_game_system_function->m_dependence[cindex].m_tid
                            );

                            ainfo.m_component_sizes[cindex] = arch_typeinfo->m_typeinfo->m_chunk_size;
                            ainfo.m_component_mem_begin_offsets[cindex] = arch_typeinfo->m_begin_offset_in_chunk;
                        }
                    }
                }

            }
        }

        inline void close_all_entity(ecs_world* by_world)
        {
            std::shared_lock sg1(_m_arch_types_mapping_mx);
            for (auto& [types, archtype] : _m_arch_types_mapping)
            {
                archtype->close_all_entity(by_world);
            }
        }
    };

    class command_buffer
    {
        // command_buffer used to store operations happend in a entity.
        JECS_DISABLE_MOVE_AND_COPY(command_buffer);

        struct _entity_command_buffer
        {
            JECS_DISABLE_MOVE_AND_COPY(_entity_command_buffer);

            struct typed_component
            {
                const jeecs::typing::type_info* m_typeinfo;
                void* m_component_addr;

                typed_component* last;

                typed_component(const jeecs::typing::type_info* id, void* addr)
                    : m_typeinfo(id)
                    , m_component_addr(addr)
                {
                    // Do nothing else
                }
            };

            jeecs::basic::atomic_list<typed_component> m_removed_components;
            jeecs::basic::atomic_list<typed_component> m_append_components;
            bool                                       m_entity_removed_flag;

            _entity_command_buffer() = default;
        };

        struct _world_command_buffer
        {
            struct system_list_node
            {
                jeecs::game_system_function* m_system_function;
                ecs_system_function* m_ecs_system_function;
                system_list_node* last;

                system_list_node(
                    jeecs::game_system_function* system_function,
                    ecs_system_function* ecs_system_function)
                    : m_system_function(system_function)
                    , m_ecs_system_function(ecs_system_function)
                {
                    // Do nothing else
                }
            };

            jeecs::basic::atomic_list<system_list_node> m_append_system;
            jeecs::basic::atomic_list<system_list_node> m_removed_system;
            bool                                        m_destroy_world;
        };

        std::shared_mutex _m_command_buffer_mx;
        std::map<arch_type::entity, _entity_command_buffer> _m_entity_command_buffer;
        std::map<ecs_world*, _world_command_buffer> _m_world_command_buffer;

        _entity_command_buffer& _find_or_create_buffer_for(const arch_type::entity& e)
        {
            do
            {
                std::shared_lock sg1(_m_command_buffer_mx);
                auto fnd = _m_entity_command_buffer.find(e);
                if (fnd != _m_entity_command_buffer.end())
                    return fnd->second;

            } while (0);

            std::lock_guard g1(_m_command_buffer_mx);
            return _m_entity_command_buffer[e];
        }
        _world_command_buffer& _find_or_create_buffer_for(ecs_world* w)
        {
            do
            {
                std::shared_lock sg1(_m_command_buffer_mx);
                auto fnd = _m_world_command_buffer.find(w);
                if (fnd != _m_world_command_buffer.end())
                    return fnd->second;

            } while (0);

            std::lock_guard g1(_m_command_buffer_mx);
            return _m_world_command_buffer[w];
        }

        std::shared_mutex _m_command_executer_guard_mx;

    public:
        command_buffer() = default;
        ~command_buffer()
        {
            assert(_m_entity_command_buffer.empty() && _m_world_command_buffer.empty());
        }

        void init_new_entity(const arch_type::entity& e)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for(e);
        }

        void remove_entity(const arch_type::entity& e)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for(e).m_entity_removed_flag = true;
        }

        void* append_component(const arch_type::entity& e, const jeecs::typing::type_info* component_type)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            // Instance component
            void* created_component = je_mem_alloc(component_type->m_size);
            component_type->construct(created_component);

            _find_or_create_buffer_for(e).m_append_components.add_one(
                jeecs::basic::create_new<_entity_command_buffer::typed_component>(component_type, created_component)
            );

            return created_component;
        }

        void remove_component(const arch_type::entity& e, const jeecs::typing::type_info* component_type)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for(e).m_removed_components.add_one(
                jeecs::basic::create_new<_entity_command_buffer::typed_component>(component_type, nullptr)
            );
        }

        void append_system(ecs_world* w, jeecs::game_system_function* game_system_function)
        {
            DEBUG_ARCH_LOG("World: %p append system:%p operation has been committed to the command buffer.",
                w, game_system_function);

            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for(w).m_append_system.add_one(
                jeecs::basic::create_new<_world_command_buffer::system_list_node>(game_system_function,
                    jeecs::basic::create_new<jeecs_impl::ecs_system_function>(game_system_function))
            );
        }

        void remove_system(ecs_world* w, jeecs::game_system_function* game_system_function)
        {
            DEBUG_ARCH_LOG("World: %p remove system:%p operation has been committed to the command buffer.",
                w, game_system_function);

            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for(w).m_removed_system.add_one(
                jeecs::basic::create_new<_world_command_buffer::system_list_node>(game_system_function,
                    nullptr)
            );
        }

        void close_world(ecs_world* w)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p The destroy world operation has been committed to the command buffer.", w);
            _find_or_create_buffer_for(w).m_destroy_world = true;
        }

    public:
        void update();
    };

    class ecs_universe;

    class ecs_world
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_world);

        ecs_universe* _m_universe;

        command_buffer _m_command_buffer;
        arch_manager _m_arch_manager;
        std::vector<ecs_system_function*> _m_registed_system;
        std::list<std::list<ecs_system_function*>> _m_execute_seq;
        std::atomic_flag _m_system_modified = {};

        std::list<ecs_system_function*> _generate_dependence_chain() const
        {
            std::list<ecs_system_function*>dependence_system_chain;

            // Walk throw all registered system. build dependence chain.
            for (ecs_system_function* registed_system : _m_registed_system)
            {
                auto insert_place = dependence_system_chain.cbegin();
                for (; insert_place != dependence_system_chain.cend(); ++insert_place)
                {
                    auto seq = registed_system->check_dependence((*insert_place)->m_dependence_list);
                    if (seq == ecs_system_function::sequence::ONLY_HAPPEND_BEFORE )
                    {
                        // registed_system can only happend before (*insert_place), so mark here to insert
                        break;
                    }
                    else if (seq == ecs_system_function::sequence::UNABLE_DETERMINE)
                    {
                        // conflict between registed_system and (*insert_place), report error and break
                        jeecs::debug::log_error("sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
                        break;
                    }
                }
                // OK, Then continue check...
                for (auto check_place = insert_place; check_place != dependence_system_chain.cend(); ++check_place)
                {
                    auto seq = registed_system->check_dependence((*check_place)->m_dependence_list);
                    if (seq == ecs_system_function::sequence::UNABLE_DETERMINE)
                    {
                        // conflict between registed_system and (*check_place), report error and break
                        jeecs::debug::log_error("sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
                    }
                    else if (seq == ecs_system_function::sequence::ONLY_HAPPEND_AFTER)
                    {
                        // registed_system should happend at insert_place, but here require happend after (*check_place),
                        // that is impossiable, report error.
                        jeecs::debug::log_error("sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
                    }
                }
                dependence_system_chain.insert(insert_place, registed_system);
            }

            return dependence_system_chain;
        }

        std::atomic_bool _m_destroying_flag = false;

        inline bool _system_modified()noexcept
        {
            return !_m_system_modified.test_and_set();
        }

    public:
        ecs_world(ecs_universe* universe)
            :_m_universe(universe)
        {

        }

        void build_dependence_graph()
        {
            auto&& dependence_system_chain = _generate_dependence_chain();

            std::unordered_set<ecs_system_function*> write_conflict_set;
            std::list<std::list<ecs_system_function*>> output_layer;

            // Begin to merge chain to layer..

            for (auto* esystem : dependence_system_chain)
            {
                if (output_layer.size())
                {
                    bool write_conflict_flag = false;

                    for (auto* last_layer_system : output_layer.back())
                    {
                        ecs_system_function::sequence seq = esystem->check_dependence(last_layer_system->m_dependence_list);
                        switch (seq)
                        {
                        case ecs_system_function::CAN_HAPPEND_SAME_TIME:
                            // Do nothing
                            break;
                        case ecs_system_function::ONLY_HAPPEND_AFTER:
                            // Create new layer to place this system;
                            goto jmp_here_to_insert_next_system;
                            break;
                        case ecs_system_function::sequence::WRITE_CONFLICT:
                            write_conflict_flag = true;
                            break;
                        case ecs_system_function::ONLY_HAPPEND_BEFORE:
                        case ecs_system_function::UNABLE_DETERMINE:
                            break;
                        default:
                            assert(false);
                            break;
                        }
                    }

                    // OK, Can happend at same time,

                    if (write_conflict_flag)
                        write_conflict_set.insert(esystem);
                    else
                        // Insert this system to wc_set, when this seq end, make layers
                        output_layer.back().push_back(esystem);
                }
                else
                {
                    // Only new layer can be here
                jmp_here_to_insert_next_system:;

                    std::list<std::list<ecs_system_function*>> write_conflict_lists;
                    for (auto* esys : write_conflict_set)
                    {
                        bool has_insert_to_layer = false;
                        for (auto& layers : write_conflict_lists)
                        {
                            bool can_not_happend_at_same_time = false;
                            for (auto* lsys : layers)
                            {
                                auto seq = esys->check_dependence(lsys->m_dependence_list);
                                if (seq != ecs_system_function::sequence::CAN_HAPPEND_SAME_TIME)
                                {
                                    can_not_happend_at_same_time = true;
                                    break;
                                }
                            }
                            if (!can_not_happend_at_same_time)
                            {
                                has_insert_to_layer = true;
                                layers.push_back(esys);
                                break;
                            }
                        }

                        if (!has_insert_to_layer)
                            write_conflict_lists.push_back({ esys });
                    }

                    write_conflict_set.clear();
                    output_layer.insert(output_layer.end(), write_conflict_lists.begin(), write_conflict_lists.end());

                    output_layer.push_back({ esystem });
                }

            }

            std::list<std::list<ecs_system_function*>> write_conflict_lists;
            for (auto* esys : write_conflict_set)
            {
                bool has_insert_to_layer = false;
                for (auto& layers : write_conflict_lists)
                {
                    bool can_not_happend_at_same_time = false;
                    for (auto* lsys : layers)
                    {
                        auto seq = esys->check_dependence(lsys->m_dependence_list);
                        if (seq != ecs_system_function::sequence::CAN_HAPPEND_SAME_TIME)
                        {
                            can_not_happend_at_same_time = true;
                            break;
                        }
                    }
                    if (!can_not_happend_at_same_time)
                    {
                        has_insert_to_layer = true;
                        layers.push_back(esys);
                        break;
                    }
                }

                if (!has_insert_to_layer)
                    write_conflict_lists.push_back({ esys });
            }
            write_conflict_set.clear();
            output_layer.insert(output_layer.end(), write_conflict_lists.begin(), write_conflict_lists.end());

            _m_execute_seq = std::move(output_layer);
        }

        void display_execute_seq()
        {
            jeecs::debug::log_info("This world has %zu system(s) to work.", _m_registed_system.size());

            size_t count = 1;
            for (auto& systems : _m_execute_seq)
            {
                jeecs::debug::log_info("seq %zu", count++);
                for (auto* sys : systems)
                {
                    jeecs::debug::log_info("    system %p", sys);
                    for (auto& [dep_id, dep_type] : sys->m_dependence_list)
                    {
                        const char* wtype = "";
                        switch (dep_type)
                        {
                        case jeecs::game_system_function::dependence_type::EXCEPT:
                            wtype = "EXCEPT"; break;
                        case jeecs::game_system_function::dependence_type::ANY:
                            wtype = "ANY"; break;
                        case jeecs::game_system_function::dependence_type::CONTAIN:
                            wtype = "CONTAIN"; break;
                        case jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME:
                            wtype = "READ(LF)"; break;
                        case jeecs::game_system_function::dependence_type::READ_AFTER_WRITE:
                            wtype = "READ(AF)"; break;
                        case jeecs::game_system_function::dependence_type::WRITE:
                            wtype = "WRITE"; break;
                        default:
                            assert(false);
                        }

                        if (is_system_component_depends(dep_id))
                            jeecs::debug::log_info("        %s SYSTEM_COMPONENT: %p", wtype, dep_id);
                        else
                            jeecs::debug::log_info("        %s %s", wtype, jeecs::typing::type_info::of(dep_id)->m_typename);
                    }
                }
            }

        }

        // TEST
        void register_system(ecs_system_function* sys)
        {
            _m_system_modified.clear();
            _m_registed_system.push_back(sys);
        }

        void unregister_system(jeecs::game_system_function* sys)
        {
            _m_system_modified.clear();

            for (auto i = _m_registed_system.begin(); i != _m_registed_system.end(); i++)
            {
                if ((*i)->m_game_system_function == sys)
                {
                    jeecs::basic::destroy_free(*i);
                    _m_registed_system.erase(i);
                    break;
                }
            }
        }

    public:
        bool update()
        {
            if (!is_destroying())
            {
                // If system added/removed, update dependence relationship.
                bool system_changed_flag = false;
                if (_system_modified())
                {
                    build_dependence_graph();
                    system_changed_flag = true;

                    display_execute_seq();
                }
                // If arch changed, update system's data from..
                if (_m_arch_manager._arch_modified() || system_changed_flag)
                {
                    for (auto* sys_func : _m_registed_system)
                        _m_arch_manager._update_system_func_arch(sys_func);
                }

                // Ok, execute chain:
                for (auto& seq : _m_execute_seq)
                {
                    std::for_each(
#ifdef __cpp_lib_execution
                        std::execution::par_unseq,
#endif
                        seq.begin(), seq.end(),
                        [](ecs_system_function* func)
                        {
                            func->m_game_system_function->update();
                        }
                    );
                }
            }
            else
            {
                // Find all entity to close.
                _m_arch_manager.close_all_entity(this);

                // Find all system to close.
                for (auto* sys : _m_registed_system)
                    _m_command_buffer.remove_system(this, sys->m_game_system_function);

                // After this round, we should do a round of command buffer update, then close this.     
                _m_command_buffer.update();


                jeecs::basic::destroy_free(this);
                return false;
            }

            // Complete command buffer:
            _m_command_buffer.update();

            return true;
        }

        inline arch_type::entity create_entity_with_component(const types_set& types)
        {
            auto&& entity = _m_arch_manager.create_an_entity_with_component(types);
            _m_command_buffer.init_new_entity(entity);
            return entity;
        }

        inline command_buffer& get_command_buffer() noexcept
        {
            return _m_command_buffer;
        }

        inline bool is_destroying()const noexcept
        {
            return _m_destroying_flag;
        }

        inline void ready_to_destroy() noexcept
        {
            _m_destroying_flag = true;
        }

        inline ecs_universe* get_universe() const noexcept
        {
            return _m_universe;
        }
    };

    void command_buffer::update()
    {
        std::lock_guard g1(_m_command_executer_guard_mx);

        // Update all operate in this buffer
        std::for_each(
#ifdef __cpp_lib_execution
            std::execution::par_unseq,
#endif
            _m_entity_command_buffer.begin(), _m_entity_command_buffer.end(),
            [](std::pair<const arch_type::entity, _entity_command_buffer>& _buf_in_entity)
            {
                //  If entity not valid, skip component append&remove&active, but need 
                // free temp components.
                arch_type::entity current_entity = _buf_in_entity.first;

                if (current_entity.valid())
                {
                    if (_buf_in_entity.second.m_entity_removed_flag)
                    {
                        // Remove all new component;
                        auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                        while (append_typed_components)
                        {
                            // Free template component
                            auto current_typed_component = append_typed_components;
                            append_typed_components = append_typed_components->last;

                            current_typed_component->m_typeinfo
                                ->m_destructor(current_typed_component->m_component_addr);
                            je_mem_free(current_typed_component->m_component_addr);

                            jeecs::basic::destroy_free(current_typed_component);
                        }

                        auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                        while (removed_typed_components)
                        {
                            auto current_typed_component = removed_typed_components;
                            removed_typed_components = removed_typed_components->last;

                            jeecs::basic::destroy_free(current_typed_component);
                        }

                        // Remove all component
                        types_set origin_chunk_types = current_entity.chunk()->types();
                        for (jeecs::typing::typeid_t type_id : origin_chunk_types)
                        {
                            current_entity.chunk()->destruct_component_addr_with_typeid(
                                current_entity._m_id,
                                type_id
                            );
                        }

                        // OK, Mark old entity chunk is freed, 
                        current_entity.chunk()->command_close_entity(current_entity._m_id);
                    }
                    else
                    {
                        // 1. Mark entity as active..
                        current_entity.chunk()->command_active_entity(current_entity._m_id);

                        types_set new_chunk_types = current_entity.chunk()->types();

                        // 2. Destroy removed component..
                        auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                        while (removed_typed_components)
                        {
                            auto current_typed_component = removed_typed_components;
                            removed_typed_components = removed_typed_components->last;

                            assert(current_typed_component->m_component_addr == nullptr);
                            if (new_chunk_types.erase(current_typed_component->m_typeinfo->m_id))
                            {
                                current_entity.chunk()
                                    ->destruct_component_addr_with_typeid(current_entity._m_id,
                                        current_typed_component->m_typeinfo->m_id);
                            }

                            jeecs::basic::destroy_free(current_typed_component);
                        }

                        // 3. Prepare append component..(component may be repeated, so we using last one and give warning)
                        std::unordered_map<jeecs::typing::typeid_t, void*> append_component_type_addr_set;
                        auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                        while (append_typed_components)
                        {
                            // Free template component
                            auto current_typed_component = append_typed_components;
                            append_typed_components = append_typed_components->last;

                            assert(new_chunk_types.find(current_typed_component->m_typeinfo->m_id) == new_chunk_types.end());

                            auto& addr_place = append_component_type_addr_set[current_typed_component->m_typeinfo->m_id];
                            if (addr_place)
                            {
                                // This type of component already in list, destruct/free it and give warning
                                // TODO: WARNING~
                                current_typed_component->m_typeinfo->m_destructor(addr_place);
                                je_mem_free(addr_place);
                            }
                            addr_place = current_typed_component->m_component_addr;
                            new_chunk_types.insert(current_typed_component->m_typeinfo->m_id);

                            jeecs::basic::destroy_free(current_typed_component);
                        }

                        // 5. Almost done! get new arch type:
                        auto* current_arch_type = current_entity.chunk()->get_arch_type();
                        auto* new_arch_type = current_arch_type->get_arch_mgr()->find_or_add_arch(new_chunk_types);

                        if (new_arch_type == current_arch_type)
                        {
                            // New & old arch is same, rebuilt in place.
                            for (auto [type_id, component_addr] : append_component_type_addr_set)
                            {
                                current_entity.chunk()->move_component_to(current_entity._m_id, type_id, component_addr);
                                je_mem_free(component_addr);
                            }
                        }
                        else
                        {
                            arch_type::arch_chunk* chunk;
                            jeecs::typing::entity_id_in_chunk_t entity_id;
                            jeecs::typing::version_t entity_version;

                            new_arch_type->alloc_entity(&chunk, &entity_id, &entity_version);
                            // Entity alloced, move component to here..

                            for (jeecs::typing::typeid_t type_id : new_chunk_types)
                            {
                                auto fnd = append_component_type_addr_set.find(type_id);
                                if (fnd == append_component_type_addr_set.end())
                                {
                                    // 1. Move old component
                                    chunk->move_component_to(entity_id, type_id,
                                        current_entity.chunk()->get_component_addr_with_typeid(
                                            current_entity._m_id, type_id));
                                }
                                else
                                {
                                    // 2. Move new component
                                    chunk->move_component_to(entity_id, type_id, fnd->second);
                                    je_mem_free(fnd->second);
                                }
                            }

                            // OK, Mark old entity chunk is freed, 
                            current_entity.chunk()->command_close_entity(current_entity._m_id);

                            // Active new one
                            chunk->command_active_entity(entity_id);
                        }

                    }// End component modify
                }
                else
                {
                    auto* append_typed_components = _buf_in_entity.second.m_append_components.pick_all();
                    while (append_typed_components)
                    {
                        // Free template component
                        auto current_typed_component = append_typed_components;
                        append_typed_components = append_typed_components->last;

                        current_typed_component->m_typeinfo
                            ->destruct(current_typed_component->m_component_addr);
                        je_mem_free(current_typed_component->m_component_addr);

                        jeecs::basic::destroy_free(current_typed_component);
                    }

                    auto* removed_typed_components = _buf_in_entity.second.m_removed_components.pick_all();
                    while (removed_typed_components)
                    {
                        auto current_typed_component = removed_typed_components;
                        removed_typed_components = removed_typed_components->last;

                        jeecs::basic::destroy_free(current_typed_component);
                    }
                }
            });

        // Finish! clear buffer.
        _m_entity_command_buffer.clear();

        /////////////////////////////////////////////////////////////////////////////////////

        std::for_each(
#ifdef __cpp_lib_execution
            std::execution::par_unseq,
#endif
            _m_world_command_buffer.begin(), _m_world_command_buffer.end(),
            [](std::pair<ecs_world* const, _world_command_buffer>& _buf_in_world)
            {
                //  If entity not valid, skip component append&remove&active, but need 
                // free temp components.
                ecs_world* world = _buf_in_world.first;

                if (_buf_in_world.second.m_destroy_world)
                    world->ready_to_destroy();

                auto* append_system = _buf_in_world.second.m_append_system.pick_all();
                while (append_system)
                {
                    auto current_append_system = append_system;
                    append_system = append_system->last;
                    if (world->is_destroying())
                    {
                        DEBUG_ARCH_LOG_WARN("System: %p, is trying add to world: %p, but this world is destroying.",
                            current_append_system->m_system_function, world);
                        jeecs::basic::destroy_free(current_append_system->m_ecs_system_function);
                    }
                    else
                    {
                        DEBUG_ARCH_LOG("System: %p, added to world: %p.",
                            current_append_system->m_system_function, world);
                        world->register_system(current_append_system->m_ecs_system_function);
                    }

                    jeecs::basic::destroy_free(current_append_system);
                }

                auto* removed_system = _buf_in_world.second.m_removed_system.pick_all();
                while (removed_system)
                {
                    auto current_removed_system = removed_system;
                    removed_system = removed_system->last;

                    DEBUG_ARCH_LOG("System: %p, removed from world: %p.",
                        current_removed_system->m_system_function, world);
                    world->unregister_system(current_removed_system->m_system_function);

                    jeecs::basic::destroy_free(current_removed_system);
                }

            });

        // Finish! clear buffer.
        _m_world_command_buffer.clear();
    }

    // ecs_universe
    class ecs_universe
    {
        std::recursive_mutex _m_world_list_mx;
        std::vector<ecs_world*> _m_world_list;

        std::vector<ecs_world*> _m_reading_world_list;
        std::thread _m_universe_update_thread;
        std::atomic_flag _m_universe_update_thread_stop_flag = {};
        std::atomic_bool _m_pause_universe_update_for_world = true;

        struct stored_system_instance
        {
            jeecs::game_system* m_system_instance;
            void(*m_system_destructor)(jeecs::game_system*);
        };

        std::recursive_mutex _m_stored_systems_mx;
        std::unordered_map<ecs_world*, std::vector<stored_system_instance>> _m_stored_systems;

    public:
        size_t update()
        {
            _m_pause_universe_update_for_world = false;

            do
            {
                std::lock_guard g1(_m_world_list_mx);
                _m_reading_world_list = _m_world_list;
            } while (0);

            size_t executing_world_count = _m_reading_world_list.size();

            std::for_each(
#ifdef __cpp_lib_execution
                std::execution::par_unseq,
#endif
                _m_reading_world_list.begin(), _m_reading_world_list.end(),
                [this](ecs_world* world)
                {
                    DEBUG_ARCH_LOG("World %p: updating...", world);

                    double current_time = je_clock_time();

                    do
                    {
                        if (_m_pause_universe_update_for_world)
                        {
                            DEBUG_ARCH_LOG("World %p: stop update for ecs_universe world list modify.", world);
                            return;
                        }
                        if (je_clock_time() > 1.0 + current_time)
                            current_time = je_clock_time();

                        je_clock_sleep_until(current_time += 0.0166'6667);

                    } while (world->update());

                    DEBUG_ARCH_LOG("World %p: destroied.", world);

                    unstore_system_for_world(world);

                    // Execute here means world has been destroied, remove it from list;
                    std::lock_guard g1(_m_world_list_mx);
                    if (auto fnd = std::find(_m_world_list.begin(), _m_world_list.end(), world);
                        fnd != _m_world_list.end())
                        _m_world_list.erase(fnd);
                    else
                        assert(false);
                }
            );

            return executing_world_count;
        }
    public:
        ecs_universe()
        {
            DEBUG_ARCH_LOG("Ready to create ecs_universe: %p.", this);

            _m_universe_update_thread_stop_flag.test_and_set();
            _m_universe_update_thread = std::move(std::thread(
                [this]() {
                    while (true)
                    {
                        if (0 == this->update())
                        {
                            // If there is no world alive, and exit flag is setten, exit this thread.
                            if (!_m_universe_update_thread_stop_flag.test_and_set())
                                break;

                            je_clock_sleep_for(0.1);
                        }
                    }
                }
            ));

            while (_m_pause_universe_update_for_world)
                std::this_thread::yield();

            DEBUG_ARCH_LOG("Universe: %p created.", this);
        }
        ~ecs_universe()
        {
            DEBUG_ARCH_LOG("Universe: %p closing.", this);

            do
            {
                std::lock_guard g1(_m_world_list_mx);
                for (auto* world : _m_world_list)
                {
                    je_ecs_world_destroy(world);
                }

            } while (0);

            _m_universe_update_thread_stop_flag.clear();
            _m_universe_update_thread.join();

            unstore_system_for_world(nullptr);

            assert(_m_stored_systems.empty());

            DEBUG_ARCH_LOG("Universe: %p closed.", this);
        }
    public:
        ecs_world* create_world()
        {
            DEBUG_ARCH_LOG("Universe: %p want to create a world.", this);

            std::lock_guard g1(_m_world_list_mx);

            auto* world = jeecs::basic::create_new<jeecs_impl::ecs_world>(this);

            _m_world_list.push_back(world);
            _m_pause_universe_update_for_world = true;

            DEBUG_ARCH_LOG("Universe: %p create a world: %p.", this, world);

            return world;
        }

        void unstore_system_for_world(ecs_world* world)
        {
            std::lock_guard g1(_m_stored_systems_mx);
            auto fnd = _m_stored_systems.find(world);
            if (fnd != _m_stored_systems.end())
            {
                for (auto& stored_system : fnd->second)
                    stored_system.m_system_destructor(stored_system.m_system_instance);

                _m_stored_systems.erase(fnd);
            }
        }

        void store_system_for_world(ecs_world* world, jeecs::game_system* system_instacne, void(*gsystem_destructor)(jeecs::game_system*))
        {
            std::lock_guard g1(_m_stored_systems_mx);
            _m_stored_systems[world].push_back(stored_system_instance{ system_instacne,gsystem_destructor });
        }
    };
}

void* je_ecs_universe_create()
{
    return jeecs::basic::create_new<jeecs_impl::ecs_universe>();
}

void je_ecs_universe_destroy(void* ecs_universe)
{
    jeecs::basic::destroy_free((jeecs_impl::ecs_universe*)ecs_universe);
}

void je_ecs_universe_store_world_system_instance(
    void* ecs_universe,
    void* world,
    jeecs::game_system* gsystem_instance,
    void(*gsystem_destructor)(jeecs::game_system*)
)
{
    ((jeecs_impl::ecs_universe*)ecs_universe)->store_system_for_world(
        (jeecs_impl::ecs_world*)world, gsystem_instance, gsystem_destructor);
}

void* je_arch_get_chunk(void* archtype)
{
    return ((jeecs_impl::arch_type*)archtype)->get_head_chunk();
}

void* je_arch_next_chunk(void* chunk)
{
    return ((jeecs_impl::arch_type::arch_chunk*)chunk)->last;
}

void* je_ecs_world_create(void* in_universe)
{
    return ((jeecs_impl::ecs_universe*)in_universe)->create_world();// jeecs::basic::create_new<jeecs_impl::ecs_world>();
}

void je_ecs_world_destroy(void* world)
{
    ((jeecs_impl::ecs_world*)world)->get_command_buffer().close_world((jeecs_impl::ecs_world*)world);
}

void je_ecs_world_register_system_func(void* world, jeecs::game_system_function* game_system_function)
{
    jeecs_impl::ecs_world* ecsworld = (jeecs_impl::ecs_world*)world;
    ecsworld->get_command_buffer().append_system(ecsworld, game_system_function);
}

void je_ecs_world_unregister_system_func(void* world, jeecs::game_system_function* game_system_function)
{
    jeecs_impl::ecs_world* ecsworld = (jeecs_impl::ecs_world*)world;
    ecsworld->get_command_buffer().remove_system(ecsworld, game_system_function);
}

bool je_ecs_world_update(void* world)
{
    return ((jeecs_impl::ecs_world*)world)->update();
}

void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    jeecs::typing::typeid_t* component_ids)
{
    jeecs_impl::types_set types;
    while (*component_ids != jeecs::typing::INVALID_TYPE_ID)
        types.insert(*(component_ids++));

    auto&& entity = ((jeecs_impl::ecs_world*)world)->create_entity_with_component(types);

    if (out_entity)
    {
        out_entity->_m_in_chunk = entity._m_in_chunk;
        out_entity->_m_id = entity._m_id;
        out_entity->_m_version = entity._m_version;
    }
}

void* je_ecs_world_entity_add_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info)
{
    return ((jeecs_impl::ecs_world*)world)->get_command_buffer().append_component(
        reinterpret_cast<const jeecs_impl::arch_type::entity&>(*entity),
        component_info
    );
}

void je_ecs_world_entity_remove_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info)
{
    ((jeecs_impl::ecs_world*)world)->get_command_buffer().remove_component(
        reinterpret_cast<const jeecs_impl::arch_type::entity&>(*entity),
        component_info
    );
}

size_t je_arch_entity_meta_size()
{
    return sizeof(jeecs_impl::arch_type::arch_chunk::entity_meta);
}

size_t je_arch_entity_meta_state_offset()
{
    return offsetof(jeecs_impl::arch_type::arch_chunk::entity_meta, m_stat);
}

size_t je_arch_entity_meta_version_offset()
{
    return offsetof(jeecs_impl::arch_type::arch_chunk::entity_meta, m_version);
}

const void* je_arch_entity_meta_addr_in_chunk(void* chunk)
{
    return ((jeecs_impl::arch_type::arch_chunk*)chunk)->get_entity_meta();
}

void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity)
{
    jeecs_impl::arch_type::entity closing_entity;
    closing_entity._m_id = entity->_m_id;
    closing_entity._m_in_chunk = (jeecs_impl::arch_type::arch_chunk*)entity->_m_in_chunk;
    closing_entity._m_version = entity->_m_version;

    ((jeecs_impl::ecs_world*)world)->get_command_buffer().remove_entity(closing_entity);
}

void* je_ecs_world_in_universe(void* world)
{
    return  ((jeecs_impl::ecs_world*)world)->get_universe();
}