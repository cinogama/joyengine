#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <thread>
#include <condition_variable>
#include <unordered_map>

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
    public:
        using types_list = std::vector<const jeecs::typing::type_info*>;
        using archtypes_map = std::unordered_map<jeecs::typing::typeid_t, arch_type_info>;
    private:
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
                assert(((size_t) & reinterpret_cast<char const volatile&>((
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
                auto fnd = _m_arch_typeinfo_mapping.find(tid);
                if (fnd == _m_arch_typeinfo_mapping.end())
                    return nullptr;
                const arch_type_info& arch_typeinfo = fnd->second;
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
            inline size_t get_entity_count_in_chunk() const noexcept
            {
                return _m_entity_count;
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
            template<typename CT = void>
            inline CT* get_component(jeecs::typing::typeid_t tid) const
            {
                if (_m_in_chunk->is_entity_valid(_m_id, _m_version))
                    return (CT*)_m_in_chunk->get_component_addr_with_typeid(_m_id, tid);
                return nullptr;
            }

            template<typename CT>
            inline CT* get_component() const
            {
                return get_component<CT>(jeecs::typing::type_info::id<CT>());
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
            auto fnd = _m_arch_typeinfo_mapping.find(tid);
            if (fnd != _m_arch_typeinfo_mapping.end())
                return &fnd->second;
            return nullptr;
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

        inline const types_set& get_types()const noexcept
        {
            return _m_types_set;
        }

        inline const types_list& get_type_infos()const noexcept
        {
            return _m_arch_typeinfo;
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
        using dependences_t = std::unordered_multimap<jeecs::typing::typeid_t, jeecs::game_system_function::dependence_type>;

    public:
        dependences_t m_dependence_list;
        std::vector<arch_type*> m_arch_types;
        jeecs::game_system_function* m_game_system_function;

    public:
        ecs_system_function(jeecs::game_system_function* gsf)
            :m_game_system_function(gsf)
        {
            for (size_t dindex = 0; dindex < gsf->m_dependence_count; dindex++)
                m_dependence_list.insert(std::make_pair(gsf->m_dependence[dindex].m_tid, gsf->m_dependence[dindex].m_depend));
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

                    m_game_system_function->m_archs = nullptr;
                    m_game_system_function->m_arch_count = 0;
                }
            }
            m_game_system_function->m_attached_flag.clear();
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

            bool has_excepted_component = false;
            bool cannot_happend_in_same_time = false;

            for (auto [depend_tid, depend_type] : m_dependence_list)
            {
                bool is_normal_component_id = !(jeecs::typing::NOT_TYPEID_FLAG & depend_tid);

                // Check dependence
                if (has_excepted_component && is_normal_component_id)
                    continue;

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
                        if (!cannot_happend_in_same_time)
                            has_excepted_component = true;
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
                        {
                            if (!is_normal_component_id) { has_excepted_component = false; cannot_happend_in_same_time = true; }
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        }
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                    {
                        if (depend_type == jeecs::game_system_function::dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else if (depend_type == jeecs::game_system_function::dependence_type::WRITE)
                        {
                            if (!is_normal_component_id) { has_excepted_component = false; cannot_happend_in_same_time = true; }
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        }
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
                        {
                            if (!is_normal_component_id) { has_excepted_component = false; cannot_happend_in_same_time = true; }
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                        }
                        else if (depend_type == jeecs::game_system_function::dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                }
            }

            if (has_excepted_component)
                return sequence::CAN_HAPPEND_SAME_TIME;

            if (write_conflict && result == sequence::CAN_HAPPEND_SAME_TIME)
                return sequence::WRITE_CONFLICT;

            return result;
        }


    };

    class arch_manager
    {
        JECS_DISABLE_MOVE_AND_COPY(arch_manager);

        using arch_map_t = std::map<types_set, arch_type*>;

        ecs_world* _m_world;
        arch_map_t _m_arch_types_mapping;
        mutable std::shared_mutex _m_arch_types_mapping_mx;

        std::atomic_flag _m_arch_modified = {};

    public:
        arch_manager(ecs_world* world) :_m_world(world)
        {

        }
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

            types_set need_set, any_set, except_set, mayhave_set;
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
                        if (mayhave_set.find(depend.first) == mayhave_set.end())
                            need_set.insert(depend.first); break;
                    case jeecs::game_system_function::dependence_type::MAY_NOT_HAVE:
                        // Remove 'MAYHAVE' component need-requirement from need-set; 
                        mayhave_set.insert(depend.first);
                        need_set.erase(depend.first);
                        break;
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
                            if (arch_typeinfo)
                            {
                                ainfo.m_component_sizes[cindex] = arch_typeinfo->m_typeinfo->m_chunk_size;
                                ainfo.m_component_mem_begin_offsets[cindex] = arch_typeinfo->m_begin_offset_in_chunk;
                            }
                            else
                                ainfo.m_component_sizes[cindex] = ainfo.m_component_mem_begin_offsets[cindex] = 0;
                            // end of one component datas
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

        inline std::vector<arch_type*> _get_all_arch_types() const noexcept
        {
            // THIS FUNCTION ONLY FOR EDITOR
            std::vector<arch_type*> arch_list;
            std::shared_lock sg1(_m_arch_types_mapping_mx);
            for (auto& pair : _m_arch_types_mapping)
                arch_list.push_back(pair.second);

            return arch_list;
        }

        inline ecs_world* get_world() const noexcept
        {
            return _m_world;
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

        // ATTENTION: 
        /*
        if (e.get_component<...>())
            e.get_component<...>();

        THIS OPERATION IS NOT THREAD SAFE.
        * If need to get component and add it if this component,
        * There is NO-WAY! for now!!!
        */
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

            auto* func_instance = jeecs::basic::create_new<jeecs_impl::ecs_system_function>(game_system_function);

            _find_or_create_buffer_for(w).m_append_system.add_one(
                jeecs::basic::create_new<_world_command_buffer::system_list_node>(game_system_function, func_instance)
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
        std::vector<ecs_system_function*> _m_non_registed_system;
        std::list<std::list<ecs_system_function*>> _m_execute_seq;
        std::atomic_flag _m_system_modified = {};

        std::string _m_name;

        std::atomic_bool _m_destroying_flag = false;

        inline bool _system_modified()noexcept
        {
            return !_m_system_modified.test_and_set();
        }

    public:
        ecs_world(ecs_universe* universe)
            :_m_universe(universe)
            , _m_name("anonymous")
            , _m_arch_manager(this)
        {

        }

        arch_manager& _get_arch_mgr() noexcept
        {
            // NOTE: This function used for editor
            return _m_arch_manager;
        }

        const std::string& _name() const noexcept
        {
            // NOTE: This function used for editor
            return _m_name;
        }
        const std::string& _name(const std::string& new_name) noexcept
        {
            // NOTE: This function used for editor
            return _m_name = new_name;
        }

        void build_dependence_graph()
        {
            std::list<std::list<ecs_system_function*>> output_layer;

            // 1. Create dependence relationship
            std::unordered_map<ecs_system_function*, std::unordered_set<ecs_system_function*>>
                depend_map;
            std::unordered_map<ecs_system_function*, std::unordered_set<ecs_system_function*>>
                inv_depend_map;

            for (auto cur_sys = _m_registed_system.begin();
                cur_sys != _m_registed_system.end();
                ++cur_sys)
            {
                for (auto cmp_sys = cur_sys + 1;
                    cmp_sys != _m_registed_system.end();
                    ++cmp_sys)
                {
                    auto dep = (*cur_sys)->check_dependence((*cmp_sys)->m_dependence_list);
                    if (dep == ecs_system_function::sequence::ONLY_HAPPEND_AFTER)
                    {
                        // cur_sys depends on cmp_sys
                        depend_map[*cmp_sys].insert(*cur_sys);
                        inv_depend_map[*cur_sys].insert(*cmp_sys);
                    }
                    else if (dep == ecs_system_function::sequence::ONLY_HAPPEND_BEFORE)
                    {
                        //  cmp_sys depends on cur_sys
                        depend_map[*cur_sys].insert(*cmp_sys);
                        inv_depend_map[*cmp_sys].insert(*cur_sys);
                    }
                    else if (dep == ecs_system_function::sequence::UNABLE_DETERMINE)
                        // error, give warning
                        jeecs::debug::log_error("sequence conflict between system(%p) and system(%p).",
                            cmp_sys, (*cur_sys));
                }
            }

            // ok we got a depend map, calc depend chain
            std::vector<ecs_system_function*> current_syss = _m_registed_system;
            size_t not_ready_system_count = current_syss.size();

            while (not_ready_system_count)
            {
                output_layer.push_back({});
                auto& current_layer = output_layer.back();

                for (auto& cur_sys : current_syss)
                {
                    bool can_execute_now = true;
                    if (cur_sys)
                    {
                        for (auto dep_sys : inv_depend_map[cur_sys])
                        {
                            if (std::find(current_syss.begin(), current_syss.end(), dep_sys) != current_syss.end())
                            {
                                // Depend sys not ready, break
                                can_execute_now = false;
                                break;
                            }
                        }
                        if (can_execute_now)
                            current_layer.push_back(cur_sys);
                    }
                }

                if (current_layer.empty())
                {
                    // Loop depends, give error
                    jeecs::debug::log_error("World: %p cannot generate system dependences, maybe interdependent?");
                    // Insert all system to current layer

                    for (auto& cur_sys : current_syss)
                        if (cur_sys)
                            current_layer.push_back(cur_sys);
                }

                assert(current_layer.size() <= not_ready_system_count);
                not_ready_system_count -= current_layer.size();
                for (auto ready_systems : current_layer)
                {
                    auto fnd = std::find(current_syss.begin(), current_syss.end(), ready_systems);
                    assert(fnd != current_syss.end());
                    *fnd = nullptr;
                }
            }

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
                        case jeecs::game_system_function::dependence_type::MAY_NOT_HAVE:
                            wtype = "MAY_NOT_HAVE"; break;
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

        void register_system_next_frame(ecs_system_function* sys)
        {
            _m_system_modified.clear();
            _m_non_registed_system.push_back(sys);
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

            // Try remove the system from non_registed_system set;
            for (auto i = _m_non_registed_system.begin(); i != _m_non_registed_system.end(); i++)
            {
                if ((*i)->m_game_system_function == sys)
                {
                    jeecs::basic::destroy_free(*i);
                    _m_non_registed_system.erase(i);
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
                    // Try add non_registed system to world
                    std::vector<ecs_system_function*> non_registed_systems;
                    non_registed_systems.swap(_m_non_registed_system);
                    for (auto* non_registed_sys : non_registed_systems)
                    {
                        if (non_registed_sys->m_game_system_function->m_attached_flag.test_and_set())
                            // Still not ready!
                            this->register_system_next_frame(non_registed_sys);
                        else
                            this->register_system(non_registed_sys);
                    }

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

                //  auto current = je_clock_time();

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

                //auto endwork = je_clock_time();
                //jeecs::debug::log_warn("A round of system scheduler: %f", endwork - current);
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


                // Return false and world will be closed by universe-loop.
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
            [this](std::pair<ecs_world* const, _world_command_buffer>& _buf_in_world)
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
                        if (current_append_system->m_ecs_system_function->m_game_system_function->m_attached_flag.test_and_set())
                        {
                            // Current system has been attached to another world, retry in next frame
                            DEBUG_ARCH_LOG_WARN("System: %p, has been attached by another world, try add it next frame.",
                                current_append_system->m_system_function);

                            world->register_system_next_frame(current_append_system->m_ecs_system_function);
                        }
                        else
                        {
                            DEBUG_ARCH_LOG("System: %p, added to world: %p.",
                                current_append_system->m_system_function, world);

                            world->register_system(current_append_system->m_ecs_system_function);
                        }
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
            const jeecs::typing::type_info* m_system_typeinfo;
            ecs_world* m_attached_world; // only used in shared_system
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

                    jeecs::basic::destroy_free(world);
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

                    // invoke callback
                    auto* callback_func_node = m_exit_callback_list.pick_all();
                    while (callback_func_node)
                    {
                        auto* current_callback = callback_func_node;
                        callback_func_node = callback_func_node->last;

                        current_callback->m_method();
                        jeecs::basic::destroy_free(current_callback);
                    }
                }
            ));

            while (_m_pause_universe_update_for_world)
                std::this_thread::yield();

            DEBUG_ARCH_LOG("Universe: %p created.", this);
        }

        struct callback_function_node
        {
            callback_function_node* last;
            std::function<void(void)> m_method;
        };
        jeecs::basic::atomic_list<callback_function_node> m_exit_callback_list;

        inline void register_exit_callback(const std::function<void(void)>& function)noexcept
        {
            callback_function_node* node = jeecs::basic::create_new<callback_function_node>();
            node->m_method = function;
            m_exit_callback_list.add_one(node);
        }

        ~ecs_universe()
        {
            DEBUG_ARCH_LOG("Universe: %p closing.", this);

            stop_universe_loop();
            if (_m_universe_update_thread.joinable())
                _m_universe_update_thread.join();

            unstore_system_for_world(nullptr);

            assert(_m_stored_systems.empty());

            DEBUG_ARCH_LOG("Universe: %p closed.", this);
        }
    public:
        void stop_universe_loop() noexcept
        {
            do
            {
                std::lock_guard g1(_m_world_list_mx);
                for (auto* world : _m_world_list)
                {
                    je_ecs_world_destroy(world);
                }

            } while (0);

            _m_universe_update_thread_stop_flag.clear();
        }

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
        std::vector<ecs_world*> _get_all_worlds()
        {
            // NOTE: This function is designed for editor
            std::lock_guard g1(_m_world_list_mx);
            return _m_world_list;
        }

        void unstore_system_for_world(ecs_world* world)
        {
            std::vector<stored_system_instance> removing_sys_instances;
            do
            {
                std::lock_guard g1(_m_stored_systems_mx);
                auto fnd = _m_stored_systems.find(world);

                if (fnd != _m_stored_systems.end())
                {
                    removing_sys_instances = std::move(fnd->second);
                    _m_stored_systems.erase(fnd);
                }
            } while (0);

            for (auto& stored_system : removing_sys_instances)
            {
                stored_system.m_system_typeinfo->destruct(stored_system.m_system_instance);
                je_mem_free(stored_system.m_system_instance);
            }
        }

        void store_system_for_world(ecs_world* world, const jeecs::typing::type_info* system_type, jeecs::game_system* system_instacne)
        {
            std::lock_guard g1(_m_stored_systems_mx);
            _m_stored_systems[world].push_back(stored_system_instance{ system_instacne,system_type, world });
        }

        void attach_shared_system_to_world(ecs_world* world, const jeecs::typing::type_info* system_type)
        {
            std::lock_guard g1(_m_stored_systems_mx);
            stored_system_instance* fnd = nullptr;
            for (auto& shared_system : _m_stored_systems[nullptr])
            {
                if (shared_system.m_system_typeinfo == system_type)
                {
                    fnd = &shared_system;
                    break;
                }
            }

            if (fnd)
            {
                if (fnd->m_attached_world)
                {
                    if (fnd->m_attached_world == world)
                    {
                        // FUCK YOU!
                        DEBUG_ARCH_LOG_WARN("Shared system has been attached to world: %p, skip.", world);
                        return;
                    }

                    // Disattach from old world
                    auto* func_chain = fnd->m_system_instance->get_registed_function_chain();

                    while (func_chain)
                    {
                        fnd->m_attached_world->get_command_buffer().remove_system(fnd->m_attached_world, func_chain);
                        func_chain = func_chain->last;
                    }
                }

                // NOTE: Attach system in next universe update.
                if (world)
                {
                    // Attach to new world.
                    auto* func_chain = fnd->m_system_instance->get_registed_function_chain();

                    while (func_chain)
                    {
                        world->get_command_buffer().append_system(world, func_chain);
                        func_chain = func_chain->last;
                    }
                }
                fnd->m_attached_world = world;
            }
            else
                jeecs::debug::log_error("There is no shared-system: '%s' in universe:%p.",
                    system_type->m_typename, this);
        }

        ecs_world* _shared_system_attached_world(const jeecs::typing::type_info* system_type)
        {
            // This function only worked for editor
            std::lock_guard g1(_m_stored_systems_mx);
            stored_system_instance* fnd = nullptr;
            for (auto& shared_system : _m_stored_systems[nullptr])
            {
                if (shared_system.m_system_typeinfo == system_type)
                {
                    fnd = &shared_system;
                    break;
                }
            }

            if (fnd)
                return fnd->m_attached_world;

            return nullptr;
        }
    };
}

void* je_ecs_universe_create()
{
    return jeecs::basic::create_new<jeecs_impl::ecs_universe>();
}

void je_universe_loop(void* ecs_universe)
{
    std::condition_variable exit_cv;
    std::mutex exit_mx;
    std::atomic_flag exit_flag = {};
    exit_flag.test_and_set();

    ((jeecs_impl::ecs_universe*)ecs_universe)->register_exit_callback([&]() {
        std::lock_guard g1(exit_mx);
        exit_flag.clear();
        exit_cv.notify_all();
        });

    do
    {
        std::unique_lock uq1(exit_mx);
        exit_cv.wait(uq1, [&]()->bool {return !exit_flag.test_and_set(); });
    } while (0);
}

void je_ecs_universe_destroy(void* ecs_universe)
{
    jeecs::basic::destroy_free((jeecs_impl::ecs_universe*)ecs_universe);
}

void je_ecs_universe_stop(void* ecs_universe)
{
    ((jeecs_impl::ecs_universe*)ecs_universe)->stop_universe_loop();
}

void* je_ecs_universe_instance_system(
    void* universe,
    void* aim_world,
    const jeecs::typing::type_info* system_type)
{
    void* instance = je_mem_alloc(system_type->m_size);

    if (aim_world)
        system_type->construct(instance, aim_world);
    else
        system_type->construct(instance, universe);

    ((jeecs_impl::ecs_universe*)universe)->store_system_for_world(
        (jeecs_impl::ecs_world*)aim_world, system_type, (jeecs::game_system*)instance);

    return instance;
}

void je_ecs_universe_attach_shared_system_to(
    void* universe,
    void* aim_world,
    const jeecs::typing::type_info* system_type
)
{
    ((jeecs_impl::ecs_universe*)universe)->attach_shared_system_to_world(
        (jeecs_impl::ecs_world*)aim_world, system_type);
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
        out_entity->_set_arch_chunk_info(entity._m_in_chunk, entity._m_id, entity._m_version);
    }
}

void* je_ecs_world_entity_add_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info)
{
    return ((jeecs_impl::ecs_world*)world)->get_command_buffer().append_component(
        *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity)),
        component_info
    );
}

void je_ecs_world_entity_remove_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info)
{
    ((jeecs_impl::ecs_world*)world)->get_command_buffer().remove_component(
        *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity)),
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

void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info)
{
    auto& ecs_entity = *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity));
    auto* component = ecs_entity.get_component(component_info->m_id);
    return component;
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

void* je_ecs_world_of_entity(const jeecs::game_entity* entity)
{
    auto* chunk = (jeecs_impl::arch_type::arch_chunk*)entity->_m_in_chunk;
    return chunk->get_arch_type()->get_arch_mgr()->get_world();
}

bool je_ecs_world_validate_entity(const jeecs::game_entity* entity)
{
    auto* chunk = (jeecs_impl::arch_type::arch_chunk*)entity->_m_in_chunk;
    return chunk->is_entity_valid(entity->_m_id, entity->_m_version);
}

//////////////////// FOLLOWING IS DEBUG EDITOR API ////////////////////
void** jedbg_get_all_worlds_in_universe(void* _universe)
{
    jeecs_impl::ecs_universe* universe = (jeecs_impl::ecs_universe*)_universe;
    auto result = std::move(universe->_get_all_worlds());

    void** out_result = (void**)je_mem_alloc(sizeof(void*) * (result.size() + 1));

    void** write_place = out_result;
    for (auto* worlds : result)
        *(write_place++) = (void*)worlds;
    *write_place = nullptr;

    return out_result;
}

const char* jedbg_get_world_name(void* _world)
{
    return ((jeecs_impl::ecs_world*)_world)->_name().c_str();
}

void jedbg_set_world_name(void* _world, const char* name)
{
    ((jeecs_impl::ecs_world*)_world)->_name(name);
}

void* jedbg_get_shared_system_attached_world(void* _universe, const jeecs::typing::type_info* tinfo)
{
    return ((jeecs_impl::ecs_universe*)_universe)->_shared_system_attached_world(tinfo);
}

void jedbg_free_entity(jeecs::game_entity* _entity_list)
{
    jeecs::basic::destroy_free(_entity_list);
}

jeecs::game_entity** jedbg_get_all_entities_in_world(void* _world)
{
    jeecs_impl::ecs_world* world = (jeecs_impl::ecs_world*)_world;

    std::vector<jeecs::game_entity*> out_entities;

    auto&& archs = world->_get_arch_mgr()._get_all_arch_types();
    for (auto& arch : archs)
    {
        auto* chunk = arch->get_head_chunk();
        while (chunk)
        {
            size_t entity_count_in_chunk = chunk->get_entity_count_in_chunk();
            auto* entity_meta_arr = chunk->get_entity_meta();
            for (size_t i = 0; i < entity_count_in_chunk; ++i)
            {
                if (entity_meta_arr[i].m_stat == jeecs::game_entity::entity_stat::READY)
                {
                    jeecs::game_entity* entity = jeecs::basic::create_new<jeecs::game_entity>();
                    entity->_set_arch_chunk_info(chunk, i, entity_meta_arr[i].m_version);

                    out_entities.push_back(entity);
                }
            }
            chunk = chunk->last;
        }
    }
    jeecs::game_entity** out_result = (jeecs::game_entity**)je_mem_alloc(sizeof(jeecs::game_entity*) * (out_entities.size() + 1));
    memcpy(out_result, out_entities.data(), out_entities.size() * sizeof(jeecs::game_entity*));
    out_result[out_entities.size()] = nullptr;

    return out_result;
}

const jeecs::typing::type_info** jedbg_get_all_components_from_entity(jeecs::game_entity* _entity)
{
    auto* cur_chunk = (jeecs_impl::arch_type::arch_chunk*)_entity->_m_in_chunk;
    auto& cur_arch_type_infos = cur_chunk->get_arch_type()->get_type_infos();

    const jeecs::typing::type_info** outresult = (const jeecs::typing::type_info**)je_mem_alloc(
        sizeof(const jeecs::typing::type_info*) * cur_arch_type_infos.size() + 1);
    
    auto write_result = outresult;
    for (auto* typeinfo : cur_arch_type_infos)
    {
        *(write_result++) = typeinfo;
    }
    *write_result = nullptr;

    // Sort by id.
    std::sort(outresult, write_result, 
        [](const jeecs::typing::type_info* a, const jeecs::typing::type_info*b) {
            return a->m_id < b->m_id;
        });

    return outresult;
}

static void* _editor_universe;

void jedbg_set_editor_universe(void* universe_handle)
{
    _editor_universe = universe_handle;
}

void* jedbg_get_editor_universe()
{
    return _editor_universe;
}