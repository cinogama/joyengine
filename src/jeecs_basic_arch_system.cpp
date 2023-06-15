#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <optional>

#ifdef NDEBUG
#   define DEBUG_ARCH_LOG(...) ((void)0)
#   define DEBUG_ARCH_LOG_WARN(...) ((void)0)
#else
#   define DEBUG_ARCH_LOG(...) jeecs::debug::loginfo( __VA_ARGS__ );
#   define DEBUG_ARCH_LOG_WARN(...) jeecs::debug::logwarn( __VA_ARGS__ );
#endif

#if defined(__cpp_lib_execution) && defined(NDEBUG)
#   define ParallelForeach(...) std::for_each( std::execution::par_unseq, __VA_ARGS__ )
#else
#   define ParallelForeach std::for_each
#endif

#define CHECK(A,B)((A-B>=-0.0001))
#define jeoffsetof(T, M) ((::size_t)&reinterpret_cast<char const volatile&>((((T*)0)->M)))

/*
* 欢迎来到罪恶和灾难之地！这里是ArchSystem，整个引擎最黑暗扭曲的阴暗之地！
* 所有的并行、组件、实体（如果存在的话）、世界、整个上下文，都处于这个混沌
* 沙盘里微小的一部分，但同时也是ArchSystem的全部。
* 
* 在ECS（Entity Component System）中，实体虽然被摆在最开头，但是实际上是不存
* 在的，它只是一组组件集合的概念，这也是JoyEngine的TranslationSystem实现得扭
* 曲复杂的根源性问题——实体本身不存在了，也就没有办法简单地通过挂一个指针引
* 用的方式指向另一个。实体的父子层级关系就此崩塌。
* 
* 为了实现ECS，ArchSystem被设计用于保存组件的数据——为了缓存友好，数据应该
* 尽可能高效而紧密地排布。好在ECS的工作方式决定了，拥有相同组件的实体们通常
* 会被一次性批量处理。因此ArchSystem只需要让拥有相同组件的实体们的组件们连续
* 排布（不同组件的空间保证8字节对齐）即可。
* 
* 曾有设想，如果一个人被按照原样、重建其身体的每一个原子。然后将原本的身体消灭，
* 那么这两个人是同一个人吗？在使用ECS时，这样的问题就变成的切实存在的了：一旦
* 一个实体的组件被添加或删除，它就会在某个时机从一个ArchType迁移到另一个ArchType
* 一切都会被回收，只有数据被保留。
* 
* 一个ArchType由一系列ArchChunk组成，ArchChunk保持有一片内存空间。写下这些的时候
* 这个空间的长度是 CHUNK_SIZE = 64KB，一个Chunk能储存若干个实体——具体的数量
* 取决于实体的大小——或者说组件们的大小。一个ArchType只被用于储存拥有某一组件
* 组的组件们——我知道这样说话很拗口，但确实如此；ArchType被ArchManager统一管理。
* 并属于某个世界。世界销毁时一切也将被一起回收。
* 
* 世界是组件和系统的集合，世界之间的工作周期是独立的，每个世界会按照自己的工作
* 节奏，在独立的线程中运作。而所有的世界最终属于这个宇宙（Universe）：它是
* 引擎的全部上下文的总和。
* 
* 实际上，Universe应该能够被创建复数个，Universe之间亦是互相独立的。尽管引擎自带
* 的图形系统对此兼容性不佳——但确实可以，JoyEngine已经默许开发者将数据保存在系
* 统的实例中，尽管在原教旨主义的ECS下这不该发生，但是我只想说，尽量远离静态生命
* 周期——哪怕把数据放在系统上也好。
* 
*                                                               ——混乱制造者
*                                                                   mr_cino
*/

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
                assert(jeoffsetof(jeecs_impl::arch_type::arch_chunk, _m_chunk_buffer) == 0);

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
            assert(_m_entity_size != 0);
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
        static void free_chunk_info(jeecs::dependence::arch_chunks_info* archinfo) noexcept
        {
            delete[] archinfo->m_component_sizes;
            delete[]archinfo->m_component_offsets;

            delete archinfo;
        }
        inline jeecs::dependence::arch_chunks_info* create_chunk_info(const jeecs::dependence* depend) const noexcept
        {
            jeecs::dependence::arch_chunks_info* info = new jeecs::dependence::arch_chunks_info;
            info->m_arch = const_cast<arch_type*>(this);
            info->m_entity_count = get_entity_count_per_chunk();

            info->m_component_count = depend->m_requirements.size();
            info->m_component_sizes = new size_t[info->m_component_count];
            info->m_component_offsets = new size_t[info->m_component_count];

            for (size_t reqid = 0; reqid < info->m_component_count; ++reqid)
            {
                auto* arch_typeinfo = get_arch_type_info_by_type_id(depend->m_requirements[reqid].m_type);

                if (arch_typeinfo)
                {
                    info->m_component_sizes[reqid] = arch_typeinfo->m_typeinfo->m_chunk_size;
                    info->m_component_offsets[reqid] = arch_typeinfo->m_begin_offset_in_chunk;
                }
                else
                {
                    assert(depend->m_requirements[reqid].m_require == jeecs::requirement::ANYOF
                        || depend->m_requirements[reqid].m_require == jeecs::requirement::MAYNOT
                        || depend->m_requirements[reqid].m_require == jeecs::requirement::EXCEPT);
                    info->m_component_sizes[reqid] = info->m_component_offsets[reqid] = 0;
                }
            }

            return info;
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

        arch_type* find_or_add_arch(const types_set& _types) noexcept
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
        arch_type::entity create_an_entity_with_component(const types_set& _types) noexcept
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

        inline void update_dependence_archinfo(jeecs::dependence* dependence) const noexcept
        {
            types_set contain_set, anyof_set, except_set /*, maynot_set*/;
            for (auto& requirement : dependence->m_requirements)
            {
                switch (requirement.m_require)
                {
                case jeecs::requirement::type::CONTAIN:
                    contain_set.insert(requirement.m_type); break;
                case jeecs::requirement::type::MAYNOT:
                    /*maynot_set.insert(requirement.m_type);*/ break;
                case jeecs::requirement::type::ANYOF:
                    anyof_set.insert(requirement.m_type); break;
                case jeecs::requirement::type::EXCEPT:
                    except_set.insert(requirement.m_type); break;
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

            dependence->m_archs.clear();
            do
            {
                std::shared_lock sg1(_m_arch_types_mapping_mx);
                for (auto& [typeset, arch] : _m_arch_types_mapping)
                {
                    if (contain(typeset, contain_set)
                        && contain_any(typeset, anyof_set)
                        && except(typeset, except_set))
                    {
                        // Current arch is matched!
                        dependence->m_archs.push_back(arch->create_chunk_info(dependence));
                    }
                }
            } while (0);
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

    class ecs_universe;

    struct ecs_job
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_job);

        using job_for_worlds_t = je_job_for_worlds_t;
        using job_call_once_t = je_job_call_once_t;

        enum job_type
        {
            CALL_ONCE,
            FOR_WORLDS,
        };
        job_type m_job_type;

        union
        {
            job_for_worlds_t m_for_worlds_job;
            job_call_once_t m_call_once_job;
        };

        ecs_universe* m_universe;
        void* m_custom_data;
        void(*m_free_function)(void*);
        std::mutex m_time_guard;
        double m_next_execute_time;

        ecs_job(ecs_universe* universe, job_for_worlds_t _job, void* custom_data, void(*free_function)(void*))
            : m_for_worlds_job(_job)
            , m_job_type(job_type::FOR_WORLDS)
            , m_next_execute_time(0.)
            , m_universe(universe)
            , m_custom_data(custom_data)
            , m_free_function(free_function)
        {
            assert(_job != nullptr);
        }
        ecs_job(ecs_universe* universe, job_call_once_t _job, void* custom_data, void(*free_function)(void*))
            : m_call_once_job(_job)
            , m_job_type(job_type::CALL_ONCE)
            , m_next_execute_time(0.)
            , m_universe(universe)
            , m_custom_data(custom_data)
            , m_free_function(free_function)
        {
            assert(_job != nullptr);
        }
        ~ecs_job()
        {
            if (m_free_function != nullptr)
                m_free_function(m_custom_data);
        }

        inline void set_next_execute_time(double nextTime) noexcept;
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
            JECS_DISABLE_MOVE_AND_COPY(_world_command_buffer);
            struct typed_system
            {
                const jeecs::typing::type_info* m_typeinfo;
                jeecs::game_system* m_add_system_instance; // if m_add_system_instance == nullptr, remove spcify sys

                typed_system* last;

                typed_system(const jeecs::typing::type_info* id, jeecs::game_system* addr)
                    : m_typeinfo(id)
                    , m_add_system_instance(addr)
                {
                    // Do nothing else
                }
            };

            jeecs::basic::atomic_list<typed_system> m_adding_or_removing_components;
            bool m_destroy_world;

            _world_command_buffer() = default;
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

        void close_world(ecs_world* w)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p The destroy world operation has been committed to the command buffer.", w);
            _find_or_create_buffer_for(w).m_destroy_world = true;
        }

        void add_system_instance(ecs_world* w, const jeecs::typing::type_info* type, jeecs::game_system* sys_instance)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p want to add system(%p) named '%s', operation has been committed to the command buffer.",
                w, sys_instance, type->m_typename);

            assert(sys_instance);

            _find_or_create_buffer_for(w).m_adding_or_removing_components.add_one(
                new _world_command_buffer::typed_system(type, sys_instance)
            );
        }

        void remove_system_instance(ecs_world* w, const jeecs::typing::type_info* type)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p want to remove system named '%s', operation has been committed to the command buffer.",
                w, type->m_typename);

            _find_or_create_buffer_for(w).m_adding_or_removing_components.add_one(
                new _world_command_buffer::typed_system(type, nullptr)
            );
        }

    public:
        void update();
    };

    class ecs_world
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_world);
    public:
        struct storage_system
        {
            jeecs::game_system* m_system_instance;
            double m_next_pre_update_time;  // USED FOR DEBUG
            double m_next_update_time;      // USED FOR DEBUG
            double m_next_late_update_time; // USED FOR DEBUG
            double m_next_commit_update_time;
            double m_execute_interval;

            storage_system& set_system_instance(jeecs::game_system* sys)noexcept
            {
                m_system_instance = sys;
                m_next_pre_update_time
                    = m_next_update_time
                    = m_next_late_update_time
                    = m_next_commit_update_time
                    = 0.;
                m_execute_interval = sys->delta_dtime();
                return *this;
            }
        };
        using system_container_t = std::unordered_map<const jeecs::typing::type_info*, storage_system>;
        using system_delay_container_t = std::unordered_multimap<const jeecs::typing::type_info*, storage_system>;
        using system_removing_container_t = std::unordered_map<const jeecs::typing::type_info*, size_t>;
    private:
        ecs_universe* _m_universe;

        command_buffer _m_command_buffer;
        arch_manager _m_arch_manager;

        std::string _m_name;

        std::atomic_bool _m_destroying_flag = false;
        std::atomic_size_t _m_archmgr_updated_version = 100;

        system_container_t m_systems;
        system_delay_container_t m_delay_appending_systems;
        system_removing_container_t m_delay_removing_systems;
    private:
        inline static std::shared_mutex _m_alive_worlds_mx;
        inline static std::unordered_set<ecs_world*> _m_alive_worlds;

    public:
        ecs_world(ecs_universe* universe)
            :_m_universe(universe)
            , _m_name("anonymous")
            , _m_arch_manager(this)
        {
            std::lock_guard g1(_m_alive_worlds_mx);
            _m_alive_worlds.insert(this);
        }
        ~ecs_world()
        {
            assert(is_valid(this));

            std::lock_guard g1(_m_alive_worlds_mx);
            _m_alive_worlds.erase(this);
        }
        static bool is_valid(ecs_world* world) noexcept
        {
            std::shared_lock sg1(_m_alive_worlds_mx);
            return _m_alive_worlds.find(world) != _m_alive_worlds.end();
        }

        system_container_t& get_system_instances() noexcept
        {
            return m_systems;
        }

        static void _destroy_system_instance(const jeecs::typing::type_info* type, jeecs::game_system* sys)noexcept
        {
            type->destruct(sys);
            je_mem_free(sys);
        }

        void append_system_instance_delay(const jeecs::typing::type_info* type, jeecs::game_system* sys)noexcept
        {
            m_delay_appending_systems.insert(std::make_pair(type, storage_system().set_system_instance(sys)));
        }
        void remove_system_instance_delay(const jeecs::typing::type_info* type)
        {
            auto fnd = m_delay_appending_systems.find(type);
            if (fnd == m_delay_appending_systems.end())
            {
                // Not found in delay appending system. try add removing flag
                ++m_delay_removing_systems[type];
            }
            else
            {
                jeecs::debug::logwarn("Current system(%p) named '%s' ready to append later, but in same frame it has been request to remove, canceled.",
                    fnd->second.m_system_instance, type->m_typename);
                _destroy_system_instance(type, fnd->second.m_system_instance);
                m_delay_appending_systems.erase(fnd);
            }

        }

        void append_system_instance(const jeecs::typing::type_info* type, jeecs::game_system* sys) noexcept
        {
            if (m_delay_removing_systems[type])
            {
                --m_delay_removing_systems[type];
                jeecs::debug::logwarn("Current system(%p) named '%s' ready to append, but in same frame it has been request to remove, canceled.",
                    sys, type->m_typename);

                _destroy_system_instance(type, sys);
            }
            else if (m_systems.find(type) != m_systems.end())
            {
                jeecs::debug::logwarn("Current system(%p) named '%s' already contained in world(%p), try add it(%p) later.",
                    m_systems[type], type->m_typename, this, sys);

                append_system_instance_delay(type, sys);
            }
            else
                m_systems[type].set_system_instance(sys);
        }
        void remove_system_instance(const jeecs::typing::type_info* type) noexcept
        {
            if (m_systems.find(type) == m_systems.end())
            {
                // System not contained in alive-systems list, try remove delay-systems
                remove_system_instance_delay(type);
            }
            else
            {
                _destroy_system_instance(type, m_systems[type].m_system_instance);
                m_systems.erase(m_systems.find(type));
            }
        }

        arch_manager& _get_arch_mgr() noexcept
        {
            // NOTE: This function used for editor
            return _m_arch_manager;
        }

        const arch_manager& _get_arch_mgr() const noexcept
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

    public:
        size_t archtype_mgr_updated_version()const noexcept
        {
            return _m_archmgr_updated_version;
        }

        void update_dependence_archinfo(jeecs::dependence* require)const noexcept
        {
            _get_arch_mgr().update_dependence_archinfo(require);
        }

        bool update()
        {
            if (!is_destroying())
            {
                if (!m_delay_appending_systems.empty())
                {
                    // append delay systems?
                    for (auto& delay_appending_system : m_delay_appending_systems)
                    {
                        if (m_systems.find(delay_appending_system.first) == m_systems.end())
                            // Append it as normal.
                            append_system_instance(delay_appending_system.first, delay_appending_system.second.m_system_instance);
                        else
                        {
                            jeecs::debug::logerr("Trying to append system(%p) with type of '%s', but the system has same type already appended.",
                                delay_appending_system.second.m_system_instance, delay_appending_system.first->m_typename);
                            _destroy_system_instance(delay_appending_system.first, delay_appending_system.second.m_system_instance);
                        }
                    }
                    m_delay_appending_systems.clear();
                }
                if (!m_delay_removing_systems.empty())
                {
                    for (auto& delay_removing_system : m_delay_removing_systems)
                    {
                        if (delay_removing_system.second)
                        {
                            jeecs::debug::logerr("Trying to remove system with type of '%s', but the specified type of system does not exists.",
                                delay_removing_system.first->m_typename);
                        }
                    }
                    m_delay_removing_systems.clear();
                }
            }
            else
            {
                // Remove all system from world.
                std::vector<const jeecs::typing::type_info*> _removing_sys_types;
                _removing_sys_types.reserve(m_systems.size());

                for (auto& sys : m_systems)
                    _removing_sys_types.push_back(sys.first);

                for (auto type : _removing_sys_types)
                    remove_system_instance(type);

                // Find all entity to close.
                _m_arch_manager.close_all_entity(this);

                // After this round, we should do a round of command buffer update, then close this.     
                _m_command_buffer.update();

                // Return false and world will be closed by universe-loop.
                return false;
            }

            // Complete command buffer:
            _m_command_buffer.update();
            if (_m_arch_manager._arch_modified())
                ++_m_archmgr_updated_version;

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

    double default_job_for_execute_sys_update_for_worlds(void* _ecs_world, void*_);

    void command_buffer::update()
    {
        // 太可怕了，这个函数实在是太可怕了……
        // 丑陋而混乱的实现！简直是灾难的起点！
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

                            if (new_chunk_types.find(current_typed_component->m_typeinfo->m_id) != new_chunk_types.end())
                            {
                                // Origin chunk has same component, the old one will be replaced by the new one.
                                // Give warning here!
                                jeecs::debug::logwarn("Try adding component '%s' to entity, but here is already have a same one.",
                                    current_typed_component->m_typeinfo->m_typename);
                            }

                            auto& addr_place = append_component_type_addr_set[current_typed_component->m_typeinfo->m_id];
                            if (addr_place)
                            {
                                // This type of component already in list, destruct/free it and give warning
                                jeecs::debug::logwarn("Try adding same component named '%s' to entity in same frame.",
                                    current_typed_component->m_typeinfo->m_typename);
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

                auto* append_or_remove_system = _buf_in_world.second.m_adding_or_removing_components.pick_all();
                while (append_or_remove_system)
                {
                    auto* cur_append_or_remove_system = append_or_remove_system;
                    append_or_remove_system = append_or_remove_system->last;

                    if (cur_append_or_remove_system->m_add_system_instance)
                    {
                        // add
                        world->append_system_instance(
                            cur_append_or_remove_system->m_typeinfo,
                            cur_append_or_remove_system->m_add_system_instance);
                    }
                    else
                    {
                        // remove
                        world->remove_system_instance(
                            cur_append_or_remove_system->m_typeinfo);
                    }


                    delete cur_append_or_remove_system;
                }
            });

        // Finish! clear buffer.
        _m_world_command_buffer.clear();
    }

    // ecs_universe
    class ecs_universe
    {
        std::vector<ecs_world*> _m_world_list;

        std::mutex _m_removing_worlds_appending_mx;

        std::thread _m_universe_update_thread;
        std::atomic_flag _m_universe_update_thread_stop_flag = {};

        // Used for store shared jobs instance.
        std::vector<ecs_job*> _m_shared_pre_jobs;
        std::vector<ecs_job*> _m_shared_jobs;
        std::vector<ecs_job*> _m_shared_after_jobs;

        std::mutex _m_next_execute_interval_mx;
        volatile double _m_current_time = 0.;
        double _m_next_execute_interval = 0.5;

        struct universe_action
        {
            enum action_type
            {
                ADD_WORLD,

                ADD_PRE_JOB_FOR_WORLDS,
                ADD_PRE_JOB_CALL_ONCE,
                ADD_NORMAL_JOB_FOR_WORLDS,
                ADD_NORMAL_JOB_CALL_ONCE,
                ADD_AFTER_JOB_FOR_WORLDS,
                ADD_AFTER_JOB_CALL_ONCE,

                REMOVE_PRE_JOB_FOR_WORLDS,
                REMOVE_PRE_JOB_CALL_ONCE,
                REMOVE_NORMAL_JOB_FOR_WORLDS,
                REMOVE_NORMAL_JOB_CALL_ONCE,
                REMOVE_AFTER_JOB_FOR_WORLDS,
                REMOVE_AFTER_JOB_CALL_ONCE,
            };

            action_type m_type;

            void* m_custom_data;
            void(*m_free_function)(void*);

            union
            {
                ecs_world* m_adding_world;
                ecs_job::job_call_once_t m_call_once_job;
                ecs_job::job_for_worlds_t m_for_worlds_job;
            };
            universe_action* last;
        };

        jeecs::basic::atomic_list<universe_action> _m_universe_actions;

        void set_next_execute_interval(double interval)
        {
            std::lock_guard g1(_m_next_execute_interval_mx);
            if (interval > 0 && interval < _m_next_execute_interval)
                _m_next_execute_interval = interval;
        }

        void append_universe_action(universe_action* act) noexcept
        {
            _m_universe_actions.add_one(act);
        }

    public:
        inline double current_time() const noexcept
        {
            return _m_current_time;
        }
        inline double next_execute_time_allign(double exec_intv)const noexcept
        {
            const double align_base = 1.0 / 600.0;
            return align_base * ceil((current_time() + exec_intv) / align_base);
        }
        void update_universe_action_and_worlds()noexcept
        {
            std::vector<ecs_world*> _m_removing_worlds;

            // Update all worlds, if world is closing, add it to _m_removing_worlds.
            ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                [this, &_m_removing_worlds](ecs_world* world) {
                    if (!world->update())
                    {
                        std::lock_guard g1(_m_removing_worlds_appending_mx);

                        // Ready remove the world from list;
                        _m_removing_worlds.push_back(world);
                    }
                });

            // Remove all closed worlds
            for (ecs_world* removed_world : _m_removing_worlds)
            {
                auto fnd = std::find(_m_world_list.begin(), _m_world_list.end(), removed_world);
                if (fnd != _m_world_list.end())
                    _m_world_list.erase(fnd);
                else
                    // Current world is not found in world_list, that should not happend.
                    assert(false);

                DEBUG_ARCH_LOG("World(%p) has been destroyed.", removed_world);
                delete removed_world;
            }

            // After world update, some universe job might need to be removed.
            // Update them here.
            auto* universe_act = _m_universe_actions.pick_all();
            while (universe_act)
            {
                auto* cur_action = universe_act;
                universe_act = universe_act->last;

                switch (cur_action->m_type)
                {
                case universe_action::action_type::ADD_WORLD:
                    _m_world_list.push_back(cur_action->m_adding_world);
                    break;

                case universe_action::action_type::ADD_PRE_JOB_FOR_WORLDS:
                    _m_shared_pre_jobs.push_back(new ecs_job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;
                case universe_action::action_type::ADD_PRE_JOB_CALL_ONCE:
                    _m_shared_pre_jobs.push_back(new ecs_job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;
                case universe_action::action_type::ADD_NORMAL_JOB_FOR_WORLDS:
                    _m_shared_jobs.push_back(new ecs_job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;
                case universe_action::action_type::ADD_NORMAL_JOB_CALL_ONCE:
                    _m_shared_jobs.push_back(new ecs_job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;
                case universe_action::action_type::ADD_AFTER_JOB_FOR_WORLDS:
                    _m_shared_after_jobs.push_back(new ecs_job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;
                case universe_action::action_type::ADD_AFTER_JOB_CALL_ONCE:
                    _m_shared_after_jobs.push_back(new ecs_job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                    break;

                case universe_action::action_type::REMOVE_PRE_JOB_FOR_WORLDS:
                {
                    auto fnd = std::find_if(_m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS
                                && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                        });

                    if (fnd == _m_shared_pre_jobs.end())
                        jeecs::debug::logerr("Cannot find pre-job(%p) in universe(%p), remove failed.",
                            cur_action->m_for_worlds_job, this);
                    else
                    {
                        if (cur_action->m_free_function != nullptr)
                            cur_action->m_free_function((*fnd)->m_custom_data);

                        delete* fnd;
                        _m_shared_pre_jobs.erase(fnd);
                    }
                    break;
                }
                case universe_action::action_type::REMOVE_PRE_JOB_CALL_ONCE:
                {
                    auto fnd = std::find_if(_m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE
                                && _job->m_call_once_job == cur_action->m_call_once_job;
                        });

                    if (fnd == _m_shared_pre_jobs.end())
                        jeecs::debug::logerr("Cannot find pre-callonce-job(%p) in universe(%p), remove failed.",
                            cur_action->m_call_once_job, this);
                    else
                    {
                        delete* fnd;
                        _m_shared_pre_jobs.erase(fnd);
                    }

                    break;
                }
                case universe_action::action_type::REMOVE_NORMAL_JOB_FOR_WORLDS:
                {
                    auto fnd = std::find_if(_m_shared_jobs.begin(), _m_shared_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS
                                && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                        });

                    if (fnd == _m_shared_jobs.end())
                        jeecs::debug::logerr("Cannot find job(%p) in universe(%p), remove failed.",
                            cur_action->m_for_worlds_job, this);
                    else
                    {
                        delete* fnd;
                        _m_shared_jobs.erase(fnd);
                    }
                    break;
                }
                case universe_action::action_type::REMOVE_NORMAL_JOB_CALL_ONCE:
                {
                    auto fnd = std::find_if(_m_shared_jobs.begin(), _m_shared_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE
                                && _job->m_call_once_job == cur_action->m_call_once_job;
                        });

                    if (fnd == _m_shared_jobs.end())
                        jeecs::debug::logerr("Cannot find callonce-job(%p) in universe(%p), remove failed.",
                            cur_action->m_call_once_job, this);
                    else
                    {
                        delete* fnd;
                        _m_shared_jobs.erase(fnd);
                    }
                    break;
                }
                case universe_action::action_type::REMOVE_AFTER_JOB_FOR_WORLDS:
                {
                    auto fnd = std::find_if(_m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS
                                && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                        });

                    if (fnd == _m_shared_after_jobs.end())
                        jeecs::debug::logerr("Cannot find after-job(%p) in universe(%p), remove failed.",
                            cur_action->m_for_worlds_job, this);
                    else
                    {
                        delete* fnd;
                        _m_shared_after_jobs.erase(fnd);
                    }
                    break;
                }
                case universe_action::action_type::REMOVE_AFTER_JOB_CALL_ONCE:
                {
                    auto fnd = std::find_if(_m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                        [cur_action](ecs_job* _job) {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE
                                && _job->m_call_once_job == cur_action->m_call_once_job;
                        });

                    if (fnd == _m_shared_after_jobs.end())
                        jeecs::debug::logerr("Cannot find after-callonce-job(%p) in universe(%p), remove failed.",
                            cur_action->m_call_once_job, this);
                    else
                    {
                        delete* fnd;
                        _m_shared_after_jobs.erase(fnd);
                    }
                    break;
                }
                default:
                    break;
                }

                delete cur_action;
            }

        }
        void update() noexcept
        {
            _m_current_time += _m_next_execute_interval;
            _m_next_execute_interval = 1.0;

            je_clock_sleep_until(_m_current_time);
            if (je_clock_time() - _m_current_time >= 2.0)
                _m_current_time = je_clock_time();
            // Sleep end!
            
            // New frame begin here!!!!

            // Walk through all jobs:
            // 1. Do pre jobs.
            ParallelForeach(
                _m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                [this](ecs_job* shared_job) {
                    if (CHECK(_m_current_time, shared_job->m_next_execute_time))
                    {
                        if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                            ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                                [this, shared_job](ecs_world* world) {
                                    shared_job->set_next_execute_time(
                                        next_execute_time_allign(shared_job->m_for_worlds_job(world, shared_job->m_custom_data)));
                                });
                        else
                            shared_job->set_next_execute_time(
                                next_execute_time_allign(shared_job->m_call_once_job(shared_job->m_custom_data)));
                    }
                    set_next_execute_interval(shared_job->m_next_execute_time - current_time());
                });

            // 2. update actions & worlds
            update_universe_action_and_worlds();

            // 3. Do normal jobs.
            ParallelForeach(
                _m_shared_jobs.begin(), _m_shared_jobs.end(),
                [this](ecs_job* shared_job) {
                    if (CHECK(_m_current_time, shared_job->m_next_execute_time))
                    {
                        if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                            ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                                [this, shared_job](ecs_world* world) {
                                    shared_job->set_next_execute_time(
                                        next_execute_time_allign(shared_job->m_for_worlds_job(world, shared_job->m_custom_data)));
                                });
                        else
                            shared_job->set_next_execute_time(
                                next_execute_time_allign(shared_job->m_call_once_job(shared_job->m_custom_data)));
                    }
                    set_next_execute_interval(shared_job->m_next_execute_time - current_time());
                });

            // 4. Do after jobs.
            ParallelForeach(
                _m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                [this](ecs_job* shared_job) {
                    if (CHECK(_m_current_time, shared_job->m_next_execute_time))
                    {
                        if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                            ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                                [this, shared_job](ecs_world* world) {
                                    shared_job->set_next_execute_time(
                                        next_execute_time_allign(shared_job->m_for_worlds_job(world, shared_job->m_custom_data)));
                                });
                        else
                            shared_job->set_next_execute_time(
                                next_execute_time_allign(shared_job->m_call_once_job(shared_job->m_custom_data)));
                    }
                    set_next_execute_interval(shared_job->m_next_execute_time - current_time());
                });
        }
    public:
        ecs_universe()
        {
            DEBUG_ARCH_LOG("Ready to create ecs_universe: %p.", this);

            // Append default jobs for updating systems.
            je_ecs_universe_register_for_worlds_job(this, default_job_for_execute_sys_update_for_worlds, nullptr, nullptr);

            _m_universe_update_thread_stop_flag.test_and_set();
            _m_universe_update_thread = std::move(std::thread(
                [this]() {
                    while (true)
                    {
                        update();
                        if (_m_world_list.empty() && !_m_universe_update_thread_stop_flag.test_and_set())
                            // If there is no world alive, and exit flag is setten, exit this thread.
                            break;
                    }

                    // Make sure universe action empty.
                    update();
                    assert(_m_universe_actions.peek() == nullptr);

                    // Free all ecs_jobs.
                    for (ecs_job* pre_job : _m_shared_pre_jobs)
                        delete pre_job;
                    for (ecs_job* job : _m_shared_jobs)
                        delete job;
                    for (ecs_job* after_job : _m_shared_after_jobs)
                        delete after_job;

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

        inline void register_pre_call_once_job(je_job_call_once_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_PRE_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_pre_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_PRE_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_call_once_job(je_job_call_once_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_NORMAL_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_NORMAL_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_after_call_once_job(je_job_call_once_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_AFTER_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_after_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void(*free_function)(void*))noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_AFTER_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }


        inline void unregister_pre_call_once_job(je_job_call_once_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_PRE_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_pre_for_worlds_job(je_job_for_worlds_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_PRE_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }
        inline void unregister_call_once_job(je_job_call_once_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_for_worlds_job(je_job_for_worlds_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }
        inline void unregister_after_call_once_job(je_job_call_once_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_after_for_worlds_job(je_job_for_worlds_t job)noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }

        ~ecs_universe()
        {
            DEBUG_ARCH_LOG("Universe: %p closing.", this);

            stop_universe_loop();
            if (_m_universe_update_thread.joinable())
                _m_universe_update_thread.join();

            DEBUG_ARCH_LOG("Universe: %p closed.", this);
        }
    public:
        void stop_universe_loop() noexcept
        {
            // Only invoke in game thread!
            for (auto* world : _m_world_list)
                je_ecs_world_destroy(world);

            _m_universe_update_thread_stop_flag.clear();
        }

        ecs_world* create_world()
        {
            DEBUG_ARCH_LOG("Universe: %p want to create a world.", this);

            jeecs_impl::ecs_world* new_world = new jeecs_impl::ecs_world(this);

            universe_action* add_world_action = new universe_action;
            add_world_action->m_type = universe_action::ADD_WORLD;
            add_world_action->m_adding_world = new_world;

            append_universe_action(add_world_action);

            return new_world;
        }
        std::vector<ecs_world*> _get_all_worlds()
        {
            // NOTE: This function is designed for editor
            return _m_world_list;
        }
    };

    inline void ecs_job::set_next_execute_time(double nextTime)noexcept
    {
        std::lock_guard g1(m_time_guard);
        if (CHECK(m_universe->current_time(), m_next_execute_time)
            || (nextTime > 0 && nextTime < m_next_execute_time))
            m_next_execute_time = nextTime;
    }

    double default_job_for_execute_sys_update_for_worlds(void* _ecs_world, void* _)
    {
        ecs_world* cur_world = (ecs_world*)_ecs_world;

        ecs_world::system_container_t& active_systems = cur_world->get_system_instances();

        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [cur_world](ecs_world::system_container_t::value_type& val)
            {
                auto& system_info = val.second;
                const double current_time = cur_world->get_universe()->current_time();

                // Make sure pre-update can happend as same frame as update & late-update
                if (CHECK(current_time, system_info.m_next_commit_update_time))
                {
                    val.first->pre_update(system_info.m_system_instance);
                    system_info.m_next_pre_update_time = current_time + system_info.m_execute_interval;
                }
            }
        );
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [cur_world](ecs_world::system_container_t::value_type& val)
            {
                auto& system_info = val.second;
                const double current_time = cur_world->get_universe()->current_time();

                // Make sure pre-update can happend as same frame as update & late-update
                if (CHECK(current_time, system_info.m_next_commit_update_time))
                {
                    val.first->update(system_info.m_system_instance);
                    system_info.m_next_update_time = current_time + system_info.m_execute_interval;

                    assert(system_info.m_next_pre_update_time == system_info.m_next_update_time);
                }
            }
        );
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [cur_world](ecs_world::system_container_t::value_type& val)
            {
                auto& system_info = val.second;
                const double current_time = cur_world->get_universe()->current_time();
                if (CHECK(current_time, system_info.m_next_commit_update_time))
                {
                    val.first->late_update(system_info.m_system_instance);
                    system_info.m_next_late_update_time = current_time + system_info.m_execute_interval;

                    assert(system_info.m_next_pre_update_time == system_info.m_next_update_time
                        && system_info.m_next_update_time == system_info.m_next_late_update_time);
                }
            }
        );
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [cur_world](ecs_world::system_container_t::value_type& val)
            {
                auto& system_info = val.second;
                const double current_time = cur_world->get_universe()->current_time();
                if (CHECK(current_time, system_info.m_next_commit_update_time))
                {
                    val.first->commit_update(system_info.m_system_instance);
                    system_info.m_next_commit_update_time = current_time + system_info.m_execute_interval;

                    assert(system_info.m_next_pre_update_time == system_info.m_next_update_time
                        && system_info.m_next_update_time == system_info.m_next_late_update_time
                        && system_info.m_next_late_update_time == system_info.m_next_commit_update_time);
                }
            }
        );

        double next_time_to_exec_system = 1.0f;

        for (auto& cur_system : active_systems)
        {
            double waittime = (cur_system.second.m_next_update_time - cur_world->get_universe()->current_time());
            if (waittime < next_time_to_exec_system)
                next_time_to_exec_system = waittime;
        }
        return next_time_to_exec_system;
    }
}

void* je_ecs_universe_create()
{
    return jeecs::basic::create_new<jeecs_impl::ecs_universe>();
}

void je_ecs_universe_register_exit_callback(void* universe, void(*callback)(void*), void* arg)
{
    ((jeecs_impl::ecs_universe*)universe)->register_exit_callback(
        [callback, arg]()
        {
            callback(arg);
        });
}

void je_ecs_universe_loop(void* ecs_universe)
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

jeecs::game_system* je_ecs_world_add_system_instance(void* world, const jeecs::typing::type_info* type)
{
    jeecs::game_system* sys = (jeecs::game_system*)je_mem_alloc(type->m_size);
    type->construct(sys, world);

    ((jeecs_impl::ecs_world*)world)->get_command_buffer().add_system_instance((jeecs_impl::ecs_world*)world, type, sys);

    return sys;
}

jeecs::game_system* je_ecs_world_get_system_instance(void* world, const jeecs::typing::type_info* type)
{
    auto& syss = ((jeecs_impl::ecs_world*)world)->get_system_instances();

    auto fnd = syss.find(type);
    if (fnd == syss.end())
        return nullptr;
    return fnd->second.m_system_instance;
}

void je_ecs_world_remove_system_instance(void* world, const jeecs::typing::type_info* type)
{
    ((jeecs_impl::ecs_world*)world)->get_command_buffer().remove_system_instance((jeecs_impl::ecs_world*)world, type);
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
        out_entity->_set_arch_chunk_info(entity._m_in_chunk, entity._m_id, entity._m_version);
}

size_t je_ecs_world_archmgr_updated_version(void* world)
{
    return ((jeecs_impl::ecs_world*)world)->archtype_mgr_updated_version();
}

void je_ecs_world_update_dependences_archinfo(void* world, jeecs::dependence* dependence)
{
    ((jeecs_impl::ecs_world*)world)->update_dependence_archinfo(dependence);
}

void je_ecs_clear_dependence_archinfos(jeecs::dependence* dependence)
{
    for (auto* archinfo : dependence->m_archs)
        jeecs_impl::arch_type::free_chunk_info(archinfo);
    dependence->m_archs.clear();
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

bool je_ecs_world_is_valid(void* world)
{
    return jeecs_impl::ecs_world::is_valid((jeecs_impl::ecs_world*)world);
}

void* je_ecs_world_of_entity(const jeecs::game_entity* entity)
{
    auto* chunk = (jeecs_impl::arch_type::arch_chunk*)entity->_m_in_chunk;
    return chunk->get_arch_type()->get_arch_mgr()->get_world();
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

const jeecs::typing::type_info** jedbg_get_all_components_from_entity(const jeecs::game_entity* _entity)
{
    auto* cur_chunk = (const jeecs_impl::arch_type::arch_chunk*)_entity->_m_in_chunk;
    auto& cur_arch_type_infos = cur_chunk->get_arch_type()->get_type_infos();

    const jeecs::typing::type_info** outresult = (const jeecs::typing::type_info**)je_mem_alloc(
        sizeof(const jeecs::typing::type_info*) * (cur_arch_type_infos.size() + 1));

    auto write_result = outresult;
    for (auto* typeinfo : cur_arch_type_infos)
    {
        *(write_result++) = typeinfo;
    }
    *write_result = nullptr;

    // Sort by id.
    std::sort(outresult, write_result,
        [](const jeecs::typing::type_info* a, const jeecs::typing::type_info* b) {
            return a->m_id < b->m_id;
        });

    return outresult;
}

static jeecs::typing::uid_t _editor_entity_uid;

void jedbg_set_editing_entity_uid(const jeecs::typing::uid_t& uid)
{
    _editor_entity_uid = uid;
}

const jeecs::typing::type_info** jedbg_get_all_system_attached_in_world(void* _world)
{
    jeecs_impl::ecs_world* world = (jeecs_impl::ecs_world*)_world;
    auto& syss = world->get_system_instances();

    std::vector<const jeecs::typing::type_info*> attached_result;
    attached_result.reserve(syss.size());

    for (auto& attached_sys : syss)
        attached_result.push_back(attached_sys.first);

    size_t bufsz = attached_result.size() * sizeof(const jeecs::typing::type_info*);
    const jeecs::typing::type_info** result = (const jeecs::typing::type_info**)je_mem_alloc(bufsz + sizeof(const jeecs::typing::type_info*));

    memcpy(result, attached_result.data(), bufsz);
    result[attached_result.size()] = nullptr;

    return result;

}

jeecs::typing::uid_t jedbg_get_editing_entity_uid()
{
    return _editor_entity_uid;
}

void je_ecs_universe_register_pre_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_pre_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_pre_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_pre_call_once_job(job, data, freefunc);
}
void je_ecs_universe_register_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_call_once_job(job, data, freefunc);
}
void je_ecs_universe_register_after_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_after_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_after_call_once_job(void* universe, je_job_call_once_t job, void* data, void(*freefunc)(void*))
{
    ((jeecs_impl::ecs_universe*)universe)->register_after_call_once_job(job, data, freefunc);
}

void je_ecs_universe_unregister_pre_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_pre_for_worlds_job(job);
}
void je_ecs_universe_unregister_pre_call_once_job(void* universe, je_job_call_once_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_pre_call_once_job(job);
}
void je_ecs_universe_unregister_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_for_worlds_job(job);
}
void je_ecs_universe_unregister_call_once_job(void* universe, je_job_call_once_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_call_once_job(job);
}
void je_ecs_universe_unregister_after_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_after_for_worlds_job(job);
}
void je_ecs_universe_unregister_after_call_once_job(void* universe, je_job_call_once_t job)
{
    ((jeecs_impl::ecs_universe*)universe)->unregister_after_call_once_job(job);
}
