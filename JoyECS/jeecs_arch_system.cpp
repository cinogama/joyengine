#define JE_IMPL
#include "jeecs.hpp"

namespace jeecs_impl
{
    using entity_id_in_chunk_t = size_t;
    using types_set = std::set<jeecs::typing::typeid_t>;
    using version_t = size_t;

    constexpr entity_id_in_chunk_t INVALID_ENTITY_ID = SIZE_MAX;
    constexpr size_t ALLIGN_BASE = alignof(std::max_align_t);
    constexpr size_t CHUNK_SIZE = 64 * 1024; // 64K

    class command_buffer;
    class arch_manager;

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

            using byte_t = uint8_t;
            static_assert(sizeof(byte_t) == 1, "sizeof(uint8_t) should be 1.");

            byte_t _m_chunk_buffer[CHUNK_SIZE];
            const types_set& _m_types;
            const archtypes_map& _m_arch_typeinfo_mapping;
            const size_t _m_entity_count;
            const size_t _m_entity_size;
            struct _entity_meta
            {
                std::atomic_flag m_in_used = {};
                version_t m_version = 0;

                enum class entity_stat
                {
                    UNAVAILABLE,    // Entity is destroied or just not ready,
                    READY,          // Entity is OK, and just work as normal.
                };
                entity_stat m_stat = entity_stat::UNAVAILABLE;
            };

            _entity_meta* _m_entities_meta;
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
                _m_entities_meta = jeecs::basic::create_new_n<_entity_meta>(_m_entity_count);
            }
            ~arch_chunk()
            {
                // All entity in chunk should be free.
                assert(_m_free_count == _m_entity_count);
                jeecs::basic::destroy_free_n(_m_entities_meta, _m_entity_count);
            }

        public:
            // ATTENTION: move_component_to WILL INVOKE DESTRUCT FUNCTION OF from_component
            inline void move_component_to(entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid, void* from_component)const
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                void* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);
                arch_typeinfo.m_typeinfo->move(component_addr, from_component);
                arch_typeinfo.m_typeinfo->destruct(from_component);
            }

            bool alloc_entity_id(entity_id_in_chunk_t* out_id, version_t* out_version)
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
            inline void* get_component_addr(entity_id_in_chunk_t _eid, size_t _chunksize, size_t _offset)const noexcept
            {
                return (void*)(_m_chunk_buffer + _offset + _eid * _chunksize);
            }
            inline bool is_entity_valid(entity_id_in_chunk_t eid, version_t eversion) const noexcept
            {
                if (_m_entities_meta[eid].m_version != eversion)
                    return false;
                return true;
            }
            inline void* get_component_addr_with_typeid(entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid) const noexcept
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping.at(tid);
                return get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

            }
            inline void destruct_component_addr_with_typeid(entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid) const noexcept
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
        private:
            // Following function only invoke by command_buffer
            friend class command_buffer;

            void command_active_entity(entity_id_in_chunk_t eid) noexcept
            {
                _m_entities_meta[eid].m_stat = _entity_meta::entity_stat::READY;
            }

            void command_close_entity(entity_id_in_chunk_t eid) noexcept
            {
                // TODO: MULTI-THREAD PROBLEM
                _m_entities_meta[eid].m_stat = _entity_meta::entity_stat::UNAVAILABLE;
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
            entity_id_in_chunk_t   _m_id;
            version_t              _m_version;

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

        void alloc_entity(arch_chunk** out_chunk, entity_id_in_chunk_t* out_eid, version_t* out_eversion)
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
            entity_id_in_chunk_t entity_id;
            version_t            entity_version;

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

    };

    struct ecs_system_function
    {
        enum dependence_type : uint8_t
        {
            // Operation
            READ_FROM_LAST_FRAME,
            WRITE,
            READ_AFTER_WRITE,

            // Constraint
            EXCEPT,
            CONTAIN,
            ANY,
        };
        enum sequence
        {
            CAN_HAPPEND_SAME_TIME,
            WRITE_CONFLICT,
            ONLY_HAPPEND_BEFORE,
            ONLY_HAPPEND_AFTER,
            UNABLE_DETERMINE,
        };
        using dependences_t = std::unordered_map<jeecs::typing::typeid_t, dependence_type>;
        
    public:
        dependences_t m_dependence_list;
        std::vector<arch_type*> m_arch_types;

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

                    if ((depend_type == dependence_type::EXCEPT
                        || aim_require_type == dependence_type::EXCEPT)
                        && depend_type != aim_require_type)
                    {
                        // These two set will not meet at same time, just skip them
                        return sequence::CAN_HAPPEND_SAME_TIME;
                    }
                    else if (aim_require_type == dependence_type::EXCEPT)
                    {
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == dependence_type::WRITE)
                    {
                        if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                        else if (depend_type == dependence_type::WRITE)
                        {
                            write_conflict = true;
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        }
                        else if (depend_type == dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == dependence_type::READ_FROM_LAST_FRAME)
                    {
                        if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else if (depend_type == dependence_type::WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                        else if (depend_type == dependence_type::READ_AFTER_WRITE)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    }
                    else if (aim_require_type == dependence_type::READ_AFTER_WRITE)
                    {
                        if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                            result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                        else if (depend_type == dependence_type::WRITE)
                            result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                        else if (depend_type == dependence_type::READ_AFTER_WRITE)
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
        std::shared_mutex _m_arch_types_mapping_mx;

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

        std::shared_mutex _m_entity_command_buffer_mx;
        std::map<arch_type::entity, _entity_command_buffer> _m_entity_command_buffer;

        _entity_command_buffer& _find_or_create_buffer_for(const arch_type::entity& e)
        {
            do
            {
                std::shared_lock sg1(_m_entity_command_buffer_mx);
                auto fnd = _m_entity_command_buffer.find(e);
                if (fnd != _m_entity_command_buffer.end())
                    return fnd->second;

            } while (0);

            std::lock_guard g1(_m_entity_command_buffer_mx);
            return _m_entity_command_buffer[e];
        }
    public:
        command_buffer() = default;

        void init_new_entity(const arch_type::entity& e)
        {
            _find_or_create_buffer_for(e);
        }

        void remove_entity(const arch_type::entity& e)
        {
            _find_or_create_buffer_for(e).m_entity_removed_flag = true;
        }

        void* append_component(const arch_type::entity& e, const jeecs::typing::type_info* component_type)
        {
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
            _find_or_create_buffer_for(e).m_removed_components.add_one(
                jeecs::basic::create_new<_entity_command_buffer::typed_component>(component_type, nullptr)
            );
        }

    public:
        void update()
        {
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
                                entity_id_in_chunk_t entity_id;
                                version_t entity_version;

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
        }
    };

    class ecs_world
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_world);

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
                    if (seq == ecs_system_function::sequence::ONLY_HAPPEND_BEFORE)
                    {
                        // registed_system can only happend before (*insert_place), so mark here to insert
                        --insert_place;
                        break;
                    }
                    else if (seq == ecs_system_function::sequence::UNABLE_DETERMINE)
                    {
                        // conflict between registed_system and (*insert_place), report error and break
                        jeecs::debug::log_error("[error] sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
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
                        jeecs::debug::log_error("[error] sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
                    }
                    else if (seq == ecs_system_function::sequence::ONLY_HAPPEND_AFTER)
                    {
                        // registed_system should happend at insert_place, but here require happend after (*check_place),
                        // that is impossiable, report error.
                        jeecs::debug::log_error("[error] sequence conflict between system(%p) and system(%p).", registed_system, (*insert_place));
                    }
                }
                dependence_system_chain.insert(insert_place, registed_system);
            }

            return dependence_system_chain;
        }
    public:
        ecs_world() = default;

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
            jeecs::debug::log_info("This world has %zu system(s) to work.\n", _m_registed_system.size());

            size_t count = 1;
            for (auto& systems : _m_execute_seq)
            {
                jeecs::debug::log_info("seq %zu\n", count++);
                for (auto* sys : systems)
                {
                    jeecs::debug::log_info("    system %p\n", sys);
                    for (auto& [dep_id, dep_type] : sys->m_dependence_list)
                    {
                        const char* wtype = "";
                        switch (dep_type)
                        {
                        case ecs_system_function::dependence_type::EXCEPT:
                            wtype = "EXCEPT"; break;
                        case ecs_system_function::dependence_type::ANY:
                            wtype = "ANY"; break;
                        case ecs_system_function::dependence_type::CONTAIN:
                            wtype = "CONTAIN"; break;
                        case ecs_system_function::dependence_type::READ_FROM_LAST_FRAME:
                            wtype = "READ(LF)"; break;
                        case ecs_system_function::dependence_type::READ_AFTER_WRITE:
                            wtype = "READ(AF)"; break;
                        case ecs_system_function::dependence_type::WRITE:
                            wtype = "WRITE"; break;
                        default:
                            assert(false);
                        }
                        jeecs::debug::log_info("        %s %s\n", wtype, jeecs::typing::type_info::of(dep_id)->m_typename);
                    }
                }
            }

        }

        // TEST
        void regist_system(ecs_system_function* sys)
        {
            _m_system_modified.clear();
            _m_registed_system.push_back(sys);
        }

        inline bool _system_modified()noexcept
        {
            return !_m_system_modified.test_and_set();
        }
    public:
        void update()
        {
            // If system added/removed update dependence relationship.
            if (!_system_modified())
                build_dependence_graph();

            // If arch changed, update system's data from..
            if (_m_arch_manager._arch_modified())
            {

            }
        }
    };

}