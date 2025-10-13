#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <list>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <optional>

#ifdef NDEBUG
#define DEBUG_ARCH_LOG(...) ((void)0)
#define DEBUG_ARCH_LOG_WARN(...) ((void)0)
#else
#define DEBUG_ARCH_LOG(...) jeecs::debug::loginfo(__VA_ARGS__)
#define DEBUG_ARCH_LOG_WARN(...) jeecs::debug::logwarn(__VA_ARGS__)
#endif

#if defined(__cpp_lib_execution) && defined(NDEBUG) && JE4_CURRENT_PLATFORM != JE4_PLATFORM_WEBGL
#define ParallelForeach(...) std::for_each(std::execution::par_unseq, __VA_ARGS__)
#else
#define ParallelForeach std::for_each
#endif

#define CHECK(A, B) ((A - B >= -0.0001))
#define jeoffsetof(T, M) ((::size_t)&reinterpret_cast<char const volatile &>((((T *)0)->M)))

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
 * 一个ArchType由一系列ArchChunk组成，ArchChunk保持有一片内存空间。这个空间的
 * 长度是 CHUNK_SIZE = 16KB，一个Chunk能储存若干个实体——具体的数量取决于实体的
 * 大小——或者说组件们的大小。一个ArchType只被用于储存拥有某一组件组的组件们——
 * 我知道这样说话很拗口，但确实如此；ArchType被ArchManager统一管理。并属于某个
 * 世界。世界销毁时一切也将被一起回收。
 *
 * 世界是组件和系统的集合，世界之间的工作周期是独立的，每个世界会按照自己的工作
 * 节奏，在独立的线程中运作。而所有的世界最终属于这个宇宙（Universe）：它是
 * 引擎的全部上下文的总和。
 *                                                                   mr_cino
 */

namespace jeecs_impl
{
    using types_set = std::set<jeecs::typing::typeid_t>;

    constexpr size_t CHUNK_SIZE = 16 * 1024; // 16K

    class command_buffer;
    class arch_manager;
    class ecs_world;

    inline bool is_system_component_depends(jeecs::typing::typeid_t id)
    {
        if (jeecs::typing::type_info::of(id) == nullptr)
            return true;
        return false;
    }

    template <typename T>
    class mcmp_lockfree_fixed_loop_queue
    {
        JECS_DISABLE_MOVE_AND_COPY(mcmp_lockfree_fixed_loop_queue);

        T* const _m_queue_buffer;
        const size_t _m_queue_size;

        std::atomic_size_t _m_head;
        std::atomic_size_t _m_tail_for_write;
        std::atomic_size_t _m_tail_for_reading;

    public:
        mcmp_lockfree_fixed_loop_queue(size_t queue_size)
            : _m_queue_buffer(new T[queue_size + 1])
            , _m_queue_size(queue_size + 1)
            , _m_head(0)
            , _m_tail_for_write(0)
            , _m_tail_for_reading(0)
        {
            assert(queue_size != 0);
        }
        ~mcmp_lockfree_fixed_loop_queue()
        {
            delete[] _m_queue_buffer;
        }

        void push(const T& elem)
        {
            size_t write_target_place = _m_tail_for_write.load();
            size_t next_write_place = (write_target_place + 1) % _m_queue_size;

            while (!_m_tail_for_write.compare_exchange_weak(write_target_place, next_write_place))
            {
                next_write_place = (write_target_place + 1) % _m_queue_size;
            }

            assert(next_write_place != _m_head.load());

            _m_queue_buffer[write_target_place] = elem;

            while (!_m_tail_for_reading.compare_exchange_weak(write_target_place, next_write_place))
            {
                next_write_place = (write_target_place + 1) % _m_queue_size;
            }
        }

        bool pop(T* out_value)
        {
            size_t read_place = _m_head.load();
            size_t next_read_place;

            do
            {
                if (read_place == _m_tail_for_reading.load())
                    return false;

                next_read_place = (read_place + 1) % _m_queue_size;

            } while (!_m_head.compare_exchange_weak(read_place, next_read_place));

            *out_value = _m_queue_buffer[read_place];
            return true;
        }
    };

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
        /* Will be init in construction */
        const types_list _m_arch_typeinfo;
        const archtypes_map _m_arch_typeinfo_mapping;

        const types_set _m_types_set;
        const size_t _m_entity_size;
        const size_t _m_entity_count_per_chunk;
        arch_manager* _m_arch_manager;

    public:
        class arch_chunk
        {
            JECS_DISABLE_MOVE_AND_COPY(arch_chunk);

        private:
            using byte_t = uint8_t;
            static_assert(sizeof(byte_t) == 1, "sizeof(uint8_t) should be 1.");

            byte_t _m_chunk_buffer[CHUNK_SIZE];

            jeecs::game_entity::meta* _m_entities_meta;

            const types_set* _m_types;
            const archtypes_map* _m_arch_typeinfo_mapping;
            const size_t _m_entity_count;
            const size_t _m_entity_size;

            arch_type* _m_arch_type;
            mcmp_lockfree_fixed_loop_queue<jeecs::typing::entity_id_in_chunk_t>
                _m_free_slots;
#ifndef NDEBUG
            std::atomic_size_t _m_debug_free_count;
#endif

        public:
            arch_chunk* last; // for atomic_list;
        public:
            arch_chunk(arch_type* _arch_type)
                : _m_types(&_arch_type->_m_types_set)
                , _m_arch_typeinfo_mapping(&_arch_type->_m_arch_typeinfo_mapping)
                , _m_entity_count(_arch_type->_m_entity_count_per_chunk)
                , _m_entity_size(_arch_type->_m_entity_size)
                , _m_arch_type(_arch_type)
                , _m_free_slots(_arch_type->_m_entity_count_per_chunk)
#ifndef NDEBUG
                ,
                _m_debug_free_count(_arch_type->_m_entity_count_per_chunk)
#endif
            {
                assert(jeoffsetof(jeecs_impl::arch_type::arch_chunk, _m_chunk_buffer) == 0);

                _m_entities_meta = new jeecs::game_entity::meta[_m_entity_count]{};

                for (size_t i = 0; i < _m_entity_count; ++i)
                    _m_free_slots.push((jeecs::typing::entity_id_in_chunk_t)i);
            }
            ~arch_chunk()
            {
                // All entity in chunk should be free.
#ifndef NDEBUG
                assert(_m_debug_free_count == _m_entity_count);
#endif
                delete[] _m_entities_meta;
            }

        public:
            // ATTENTION: move_component_to WILL INVOKE DESTRUCT FUNCTION OF from_component
            //          `move_component_from` will move a specify component instance to current chunk.
            //         But component in current chunk has been destructed. So we didn't need to destruct
            //          it again.
            inline void move_component_from(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid, void* from_component) const
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping->at(tid);
                void* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);
                arch_typeinfo.m_typeinfo->move(component_addr, from_component);
                arch_typeinfo.m_typeinfo->destruct(from_component);
            }

            bool alloc_entity_id(jeecs::typing::entity_id_in_chunk_t* out_id, jeecs::typing::version_t* out_version)
            {
                if (_m_free_slots.pop(out_id))
                {
#ifndef NDEBUG
                    if (0 == _m_debug_free_count--)
                        jeecs::debug::logfatal("Entity free count broken!");
#endif
                    auto* meta = &_m_entities_meta[*out_id];

                    assert(meta->m_stat == jeecs::game_entity::entity_stat::UNAVAILABLE);
                    *out_version = ++meta->m_version;
                    return true;
                }
                return false;
            }
            inline void* get_component_addr(jeecs::typing::entity_id_in_chunk_t _eid, size_t _chunksize, size_t _offset) const noexcept
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
                auto fnd = _m_arch_typeinfo_mapping->find(tid);
                if (fnd == _m_arch_typeinfo_mapping->end())
                    return nullptr;
                const arch_type_info& arch_typeinfo = fnd->second;
                return get_component_addr(
                    eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);
            }
            inline void destruct_component_addr_with_typeid(jeecs::typing::entity_id_in_chunk_t eid, jeecs::typing::typeid_t tid) const noexcept
            {
                const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping->at(tid);
                auto* component_addr = get_component_addr(
                    eid, arch_typeinfo.m_typeinfo->m_chunk_size, arch_typeinfo.m_begin_offset_in_chunk);

                arch_typeinfo.m_typeinfo->destruct(component_addr);
            }
            inline const types_set* types() const noexcept
            {
                return _m_types;
            }
            inline arch_type* get_arch_type() const noexcept
            {
                return _m_arch_type;
            }
            inline const jeecs::game_entity::meta* get_entity_meta() const noexcept
            {
                return _m_entities_meta;
            }
            inline void close_all_entity(ecs_world* by_world)
            {
                std::unordered_set<jeecs::typing::entity_id_in_chunk_t> free_entity_ids;
                for (;;)
                {
                    jeecs::typing::entity_id_in_chunk_t freeid;
                    if (!_m_free_slots.pop(&freeid))
                        break;

                    free_entity_ids.insert(freeid);
                }

                for (jeecs::typing::entity_id_in_chunk_t eidx = 0; eidx < _m_entity_count; eidx++)
                {
                    if (free_entity_ids.find(eidx) == free_entity_ids.end())
                    {
                        jeecs::game_entity gentity;
                        gentity._m_id = eidx;
                        gentity._m_in_chunk = this;
                        gentity._m_version = _m_entities_meta[eidx].m_version;

                        je_ecs_world_destroy_entity(by_world, &gentity);
                    }
                    else
                        _m_free_slots.push(eidx);
                }
            }
            inline size_t get_entity_count_in_chunk() const noexcept
            {
                return _m_entity_count;
            }
            inline size_t get_entity_size_in_chunk() const noexcept
            {
                return _m_entity_size;
            }

            inline std::unordered_set<jeecs::typing::entity_id_in_chunk_t>
                _take_all_empty_for_defragmentation() noexcept
            {
                // 为了碎片整理，我们需要将所有的空位都取出来
                std::unordered_set<jeecs::typing::entity_id_in_chunk_t> empty_entity_ids;
                for (;;)
                {
                    jeecs::typing::entity_id_in_chunk_t freeid;
                    if (!_m_free_slots.pop(&freeid))
                        break;

                    assert(empty_entity_ids.find(freeid) == empty_entity_ids.end());
                    empty_entity_ids.insert(freeid);
                }
                return empty_entity_ids;
            }

        private:
            // Following function only invoke by command_buffer
            friend class command_buffer;
            friend class arch_type;

            void command_active_entity(jeecs::typing::entity_id_in_chunk_t eid, jeecs::game_entity::entity_stat stat) noexcept
            {
                _m_entities_meta[eid].m_stat = stat;
            }
            void command_add_free_slot(jeecs::typing::entity_id_in_chunk_t eid) noexcept
            {
                _m_free_slots.push(eid);
            }
            void command_close_entity(jeecs::typing::entity_id_in_chunk_t eid) noexcept
            {
                auto* meta = &_m_entities_meta[eid];

                meta->m_stat = jeecs::game_entity::entity_stat::UNAVAILABLE;
                ++meta->m_version;

                command_add_free_slot(eid);

                ++_m_arch_type->_m_free_count;
#ifndef NDEBUG
                ++_m_debug_free_count;
#endif
            }
        };

    private:
        std::atomic_size_t _m_free_count;
        std::atomic_size_t _m_total_count;

        jeecs::basic::atomic_list<arch_chunk> _m_chunks;

        // 这个锁用于防止碎片整理的时候，有其他线程在操作chunk链表
        std::shared_mutex _m_chunk_list_defragmentation_mx;

    public:
        struct entity
        {
            arch_chunk* _m_in_chunk;
            jeecs::typing::entity_id_in_chunk_t _m_id;
            jeecs::typing::version_t _m_version;

            // Do not invoke this function if possiable, you should get component by arch_type & system.
            template <typename CT = void>
            inline CT* get_component(jeecs::typing::typeid_t tid) const
            {
                if (_m_in_chunk != nullptr && _m_in_chunk->is_entity_valid(_m_id, _m_version))
                    return reinterpret_cast<CT*>(_m_in_chunk->get_component_addr_with_typeid(_m_id, tid));
                return nullptr;
            }

            template <typename CT>
            inline CT* get_component() const
            {
                return get_component<CT>(jeecs::typing::type_info::id<CT>());
            }

            inline arch_chunk* chunk() const noexcept
            {
                return _m_in_chunk;
            }

            inline bool operator<(const entity& another) const noexcept
            {
                if (_m_in_chunk < another._m_in_chunk)
                    return true;
                if (_m_in_chunk > another._m_in_chunk)
                    return false;
                if (_m_id < another._m_id)
                    return true;
                return false;
            }

            inline bool is_valid() const noexcept
            {
                return _m_in_chunk != nullptr &&
                    _m_in_chunk->is_entity_valid(_m_id, _m_version);
            }
        };

    public:
        arch_type(arch_manager* _arch_manager, const types_set& _types_set)
            : _m_types_set(_types_set)
            , _m_entity_size(0)
            , _m_entity_count_per_chunk(0)
            , _m_arch_manager(_arch_manager)
            , _m_free_count(0)
            , _m_total_count(0)
        {
            static_assert(offsetof(jeecs::game_entity, _m_in_chunk) == offsetof(entity, _m_in_chunk));
            static_assert(offsetof(jeecs::game_entity, _m_id) == offsetof(entity, _m_id));
            static_assert(offsetof(jeecs::game_entity, _m_version) == offsetof(entity, _m_version));

            assert(!_types_set.empty());

            for (jeecs::typing::typeid_t tid : _types_set)
                const_cast<types_list&>(_m_arch_typeinfo).push_back(jeecs::typing::type_info::of(tid));

            std::sort(
                const_cast<types_list&>(_m_arch_typeinfo).begin(),
                const_cast<types_list&>(_m_arch_typeinfo).end(),
                [](const jeecs::typing::type_info* a, const jeecs::typing::type_info* b)
                {
                    return a->m_size < b->m_size;
                });

            size_t component_reserved_gap = 0;
            size_t last_align = 0;
            size_t entity_size = 0;
            for (auto* typeinfo : _m_arch_typeinfo)
            {
                const size_t bulge_size = last_align % typeinfo->m_align;
                const size_t gap_size = typeinfo->m_align - bulge_size;
                if (gap_size != 0)
                    component_reserved_gap += gap_size;

                last_align = typeinfo->m_align;
                entity_size += typeinfo->m_chunk_size;
            }
            const_cast<size_t&>(_m_entity_size) = entity_size;

            const size_t chunk_size_without_gap = CHUNK_SIZE - component_reserved_gap;
            assert(_m_entity_size != 0 && _m_entity_size <= chunk_size_without_gap);

            const_cast<size_t&>(_m_entity_count_per_chunk) =
                chunk_size_without_gap / _m_entity_size;

            size_t mem_offset = 0;
            for (auto* typeinfo : _m_arch_typeinfo)
            {
                mem_offset = jeecs::basic::allign_size(mem_offset, typeinfo->m_align);

                const_cast<archtypes_map&>(_m_arch_typeinfo_mapping)[typeinfo->m_id] =
                    arch_type_info{ typeinfo, mem_offset };

                mem_offset += typeinfo->m_chunk_size * _m_entity_count_per_chunk;
            }
        }
        ~arch_type()
        {
            arch_chunk* chunk = _m_chunks.pick_all();
            while (chunk)
            {
                auto* next_chunk = chunk->last;
                delete chunk;

                chunk = next_chunk;
            }
        }

        // 开始进行chunk的碎片整理工作
        void command_defragmentation() noexcept
        {
            auto now_free_count = _m_free_count.load();
            if (now_free_count <= 2 * get_entity_count_per_chunk() || 2 * now_free_count < _m_total_count)
                // 如果空闲的实体数量未达到两个chunk的数量，或者空闲的实体数量小于总实体数量的一半
                // 那么我们就不进行碎片整理
                return;

            // NOTE: 我讨厌多线程，不过我们仍然很有机会
            std::lock_guard g1(_m_chunk_list_defragmentation_mx);

            // 1. 为碎片整理准备一个新的临时chunk队列，实际上就是清空整个chunk链表
            auto* alive_chunks = _m_chunks.pick_all();

            // 2. 转存chunks，然后从尾到头开始整理碎片
            std::list<arch_chunk*> chunks;
            while (alive_chunks)
            {
                // 使得老区块集中于头部，这样方便后续整理
                chunks.push_front(alive_chunks);
                alive_chunks = alive_chunks->last;
            }

            if (!chunks.empty())
            {
                auto target_chunk_idx = chunks.begin(), pick_chunk_idx = chunks.end();
                --pick_chunk_idx;

                while (pick_chunk_idx != target_chunk_idx)
                {
                    auto* target_chunk = *target_chunk_idx;
                    auto* pick_chunk = *pick_chunk_idx;

                    auto* pick_chunk_metas = pick_chunk->get_entity_meta();

                    // 拿出并占据待转移位置的空闲区域
                    auto free_places = pick_chunk->_take_all_empty_for_defragmentation();

                    // 此标记当填充过程中发现没有足够的空闲位置时，会被设置为true
                    bool gap_filling_completed = false;

                    size_t stubborn_count = get_entity_count_per_chunk();
                    for (jeecs::typing::entity_id_in_chunk_t i = 0; i < get_entity_count_per_chunk(); ++i)
                    {
                        auto* moving_entity_meta = &pick_chunk_metas[i];

                        // 如果当前位置并不空闲，且实体状态一切OK，则需要开始转移
                        if (free_places.find(i) == free_places.end())
                        {
                            if (moving_entity_meta->m_stat == jeecs::game_entity::entity_stat::READY)
                            {
                                // OK, 继续搬运，这搬运组件多是一件美事啊
                                jeecs::typing::entity_id_in_chunk_t new_entity_id;
                                jeecs::typing::version_t new_entity_version;

                                // 从target_chunk中找到一个空闲位置，如果target_chunk满了，就移动到下一个target，直到
                                // 完成分配，或者target_chunk == pick_chunk(自己搬到自己身上没有意义~)
                                while (!target_chunk->alloc_entity_id(&new_entity_id, &new_entity_version))
                                {
                                    // 如果target_chunk满了，就移动到下一个target
                                    if (++target_chunk_idx != pick_chunk_idx)
                                        target_chunk = *target_chunk_idx;
                                    else
                                    {
                                        // 哦吼，满啦！
                                        gap_filling_completed = true;
                                        break;
                                    }
                                }

                                if (!gap_filling_completed)
                                {
                                    // 占位成功，先将计数更新
                                    size_t _debug_count = _m_free_count--;
                                    (void)_debug_count;
#ifndef NDEBUG
                                    // NOTE: _m_free_count 必不可能为0，因为刚刚才成功创建了实体ID
                                    assert(_debug_count != 0);
#endif

                                    // OK, 开始搬运
                                    for (auto& arch_typeinfo : _m_arch_typeinfo_mapping)
                                    {
                                        void* moving_component_addr = pick_chunk->get_component_addr(i,
                                            arch_typeinfo.second.m_typeinfo->m_chunk_size,
                                            arch_typeinfo.second.m_begin_offset_in_chunk);
                                        void* new_component_addr = target_chunk->get_component_addr(new_entity_id,
                                            arch_typeinfo.second.m_typeinfo->m_chunk_size,
                                            arch_typeinfo.second.m_begin_offset_in_chunk);

                                        arch_typeinfo.second.m_typeinfo->move(new_component_addr, moving_component_addr);
                                        arch_typeinfo.second.m_typeinfo->destruct(moving_component_addr);
                                    }

                                    // 直接在这里激活新的实体
                                    assert(moving_entity_meta->m_stat != jeecs::game_entity::entity_stat::UNAVAILABLE);
                                    target_chunk->command_active_entity(new_entity_id, moving_entity_meta->m_stat);

                                    // 搬运完成，由于我们已经上了锁，这里可以直接通过常规流程关掉实体
                                    // NOTE: 事到如今说这个也没用，但是还是补充一下：在原本的设计中，不上锁的情况下，此处不能
                                    //      释放实体，因为可能会被其他线程“抢占位置”，这样释放chunk的时候就寄了；不过这里锁了。
                                    pick_chunk->command_close_entity(i);

                                    --stubborn_count;
                                }
                                else
                                {
                                    // 满了，没法搬了，摆烂吧
                                    break;
                                }
                            }
                            else
                            {
                                // 当前区块被占用，可能是预设体或者尚未完成提交的实体，我们无法执行任何操作
                                // 因为无论是哪一种，都必须保证实体的数据保持在原地；
                                //
                                // TODO: 将包含预设体的区块移动到前面，这样可以最大化利用空间
                            }
                        }
                        else
                        {
                            // 此处的实体完全空闲，啥也不用做
                            --stubborn_count;
                        }
                    }

                    // OK, 无论因为什么原因，到了这里，我们都需要还原pick_chunk的空闲位置
                    for (auto freeid : free_places)
                        pick_chunk->command_add_free_slot(freeid);

                    // 如果pick_chunk中的实体都是空闲的，那么我们就可以直接释放这个chunk了
                    if (stubborn_count == 0)
                    {
                        assert(!gap_filling_completed);

                        _m_free_count -= _m_entity_count_per_chunk;
                        _m_total_count -= _m_entity_count_per_chunk;

                        delete pick_chunk;
                        *pick_chunk_idx = nullptr;
                    }

                    // NOTE: 代码执行到这里，说明要么:
                    //  a) pick_chunk中的实体都被搬走了，要么
                    //  b) target_chunk已经没有空隙了
                    //
                    // 无论哪种情况，我们都需要将pick_chunk_idx向前移动

                    if (gap_filling_completed)
                    {
                        assert(pick_chunk_idx == target_chunk_idx);
                    }
                    else
                    {
                        assert(pick_chunk_idx != target_chunk_idx);
                        --pick_chunk_idx;
                    }
                }
            }

            // OK, 碎片整理完毕，将新的chunk队列重新挂载回去
            // NOTE: 按给定顺序插回，这样可以使得富集实体的区块集中在尾部，使得实体查询更快
            for (auto* chunk : chunks)
            {
                if (chunk == nullptr)
                    break;

                _m_chunks.add_one(chunk);
            }
        }

        void alloc_entity(
            arch_chunk** out_chunk,
            jeecs::typing::entity_id_in_chunk_t* out_eid,
            jeecs::typing::version_t* out_eversion) noexcept
        {
            std::shared_lock sg1(_m_chunk_list_defragmentation_mx);
            while (true)
            {
                size_t free_entity_count = _m_free_count.load();
                while (0 != free_entity_count)
                {
                    if (!_m_free_count.compare_exchange_weak(
                        free_entity_count, free_entity_count - 1))
                    {
                        continue;
                    }

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
                    // 有空位，但是没有找到空位（我在说什么？），这是不可能的！
                    abort();
                }
                // OK, 没有空位了，我们需要创建新的chunk
                //
                // NOTE: 实际上如果有并行执行的添加实体操作，此处可能创建"不必要"的实体区块
                //      不过问题不大，因为我们有碎片整理机制。
                arch_chunk* new_chunk = new arch_chunk(this);
                _m_chunks.add_one(new_chunk);

                _m_total_count += _m_entity_count_per_chunk;
                _m_free_count += _m_entity_count_per_chunk;
            }
        }
        entity instance_entity(const entity* prefab) noexcept
        {
            arch_chunk* chunk;
            jeecs::typing::entity_id_in_chunk_t entity_id;
            jeecs::typing::version_t entity_version;

            alloc_entity(&chunk, &entity_id, &entity_version);
            for (auto& arch_typeinfo : _m_arch_typeinfo_mapping)
            {
                void* component_addr = chunk->get_component_addr(entity_id,
                    arch_typeinfo.second.m_typeinfo->m_chunk_size,
                    arch_typeinfo.second.m_begin_offset_in_chunk);

                if (prefab == nullptr)
                    arch_typeinfo.second.m_typeinfo->construct(component_addr);
                else
                    arch_typeinfo.second.m_typeinfo->copy(
                        component_addr, prefab->get_component(arch_typeinfo.second.m_typeinfo->m_id));
            }

            return entity{ chunk, entity_id, entity_version };
        }
        inline arch_manager* get_arch_mgr() const noexcept
        {
            return _m_arch_manager;
        }
        inline arch_chunk* get_head_chunk() const noexcept
        {
            return _m_chunks.peek();
        }
        inline size_t get_entity_count_per_chunk() const noexcept
        {
            return _m_entity_count_per_chunk;
        }
        inline const arch_type_info* get_arch_type_info_by_type_id(jeecs::typing::typeid_t tid) const
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
        inline const types_set& get_types() const noexcept
        {
            return _m_types_set;
        }
        inline const types_list& get_type_infos() const noexcept
        {
            return _m_arch_typeinfo;
        }

        inline void create_chunk_info(const jeecs::dependence* depend, jeecs::dependence::arch_chunks_info* out_arch_info) const noexcept
        {
            out_arch_info->m_arch = const_cast<arch_type*>(this);
            out_arch_info->m_entity_count = get_entity_count_per_chunk();

            for (const auto& requirement : depend->m_requirements)
            {
                auto* arch_typeinfo = get_arch_type_info_by_type_id(requirement.m_type);
                if (arch_typeinfo != nullptr)
                {
                    out_arch_info->m_component_infos.emplace_back(
                        jeecs::dependence::arch_chunks_info::component_info{
                            arch_typeinfo->m_begin_offset_in_chunk,
                            arch_typeinfo->m_typeinfo->m_chunk_size });
                }
                else
                {
                    assert(
                        requirement.m_require == jeecs::requirement::ANYOF
                        || requirement.m_require == jeecs::requirement::MAYNOT
                        || requirement.m_require == jeecs::requirement::EXCEPT);

                    out_arch_info->m_component_infos.emplace_back(
                        jeecs::dependence::arch_chunks_info::component_info{
                            0,
                            0 });
                }
            }
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
        arch_manager(ecs_world* world) : _m_world(world)
        {
        }
        ~arch_manager()
        {
            for (auto& [types, archtype] : _m_arch_types_mapping)
                delete archtype;
        }

        arch_type* find_or_add_arch(const types_set& _types) noexcept
        {
            if (_types.empty())
                return nullptr;
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
                atype = new arch_type(this, _types);
            _m_arch_modified.clear();
            return atype;
        }
        arch_type::entity create_an_entity_with_component(const types_set& _types) noexcept
        {
            assert(!_types.empty());
            return find_or_add_arch(_types)->instance_entity(nullptr);
        }

    public:
        // Only invoke by update..
        inline bool _arch_modified() noexcept
        {
            return !_m_arch_modified.test_and_set();
        }
        inline void update_dependence_archinfo(jeecs::dependence* dependence) const noexcept
        {
            types_set contain_set, except_set /*, maynot_set*/;
            std::map<size_t /* Group id */, types_set> anyof_sets;

            for (auto& requirement : dependence->m_requirements)
            {
                switch (requirement.m_require)
                {
                case jeecs::requirement::type::CONTAINS:
                    contain_set.insert(requirement.m_type);
                    break;
                case jeecs::requirement::type::MAYNOT:
                    /*maynot_set.insert(requirement.m_type);*/ break;
                case jeecs::requirement::type::ANYOF:
                    anyof_sets[requirement.m_require_group_id].insert(requirement.m_type);
                    break;
                case jeecs::requirement::type::EXCEPT:
                    except_set.insert(requirement.m_type);
                    break;
                }
            }

            static auto contains = [](const types_set& a, const types_set& b)
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
            static auto contain_all_any = [](const types_set& a, const std::map<size_t /* Group id */, types_set>& b)
                {
                    for (auto& [_, type_id_set] : b)
                        if (contain_any(a, type_id_set) == false)
                            return false;
                    return true;
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
                    if (contains(typeset, contain_set)
                        && contain_all_any(typeset, anyof_sets)
                        && except(typeset, except_set))
                    {
                        // Current arch is matched!
                        arch->create_chunk_info(
                            dependence,
                            &dependence->m_archs.emplace_back());
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

    private:
        friend class command_buffer;
        inline void command_defragmentation() noexcept
        {
            std::shared_lock sg1(_m_arch_types_mapping_mx);
            for (auto& [types, archtype] : _m_arch_types_mapping)
                archtype->command_defragmentation();
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
        void (*m_free_function)(void*);

        ecs_job(ecs_universe* universe, job_for_worlds_t _job, void* custom_data, void (*free_function)(void*))
            : m_job_type(job_type::FOR_WORLDS)
            , m_for_worlds_job(_job)
            , m_universe(universe)
            , m_custom_data(custom_data)
            , m_free_function(free_function)
        {
            assert(_job != nullptr);
        }
        ecs_job(ecs_universe* universe, job_call_once_t _job, void* custom_data, void (*free_function)(void*))
            : m_job_type(job_type::CALL_ONCE)
            , m_call_once_job(_job)
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
                jeecs::typing::typeid_t m_type_id;
                void* m_component_addr; // if m_add_system_instance == nullptr, remove spcify comp

                typed_component* last;

                typed_component(jeecs::typing::typeid_t tid, void* addr)
                    : m_type_id(tid), m_component_addr(addr)
                {
                    // Do nothing else
                }
            };

            bool m_entity_removed_flag;
            jeecs::game_entity::entity_stat m_entity_active_stat;
            jeecs::basic::atomic_list<typed_component> m_adding_or_removing_components;

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

                typed_system(const jeecs::typing::type_info* tinfo, jeecs::game_system* addr)
                    : m_typeinfo(tinfo), m_add_system_instance(addr)
                {
                    // Do nothing else
                }
            };
            struct append_slice_query_cache
            {
                // NOTE: 当尝试添加一个切片查询缓存时，系统可能已经被移除了；这种情况下就不添加了
                jeecs::game_system* m_system_instance_may_not_exist;

                jeecs::typing::typehash_t m_query_slice_typehash;
                std::unique_ptr<jeecs::dependence> m_created_dependence;

                append_slice_query_cache* last;
            };

            bool m_destroy_world;

            // Update configs ?
            std::optional<bool> m_update_enabled;

            jeecs::basic::atomic_list<typed_system> m_adding_or_removing_systems;
            jeecs::basic::atomic_list<append_slice_query_cache> m_adding_query_caches;

            _world_command_buffer() = default;
        };

        std::shared_mutex _m_command_buffer_mx;
        std::shared_mutex _m_command_executer_guard_mx;

        using entity_ommand_buffer_map_t =
            std::map<arch_type::entity, _entity_command_buffer*>;

        ecs_world* _m_world;
        _world_command_buffer* _m_world_command_buffer;
        entity_ommand_buffer_map_t _m_entity_command_buffers;

        _entity_command_buffer* _find_or_create_buffer_for_entity(const arch_type::entity& e)
        {
            do
            {
                std::shared_lock sg1(_m_command_buffer_mx);
                auto fnd = _m_entity_command_buffers.find(e);
                if (fnd != _m_entity_command_buffers.end())
                    return fnd->second;

            } while (0);

            std::lock_guard g1(_m_command_buffer_mx);
            auto fnd = _m_entity_command_buffers.find(e);
            if (fnd != _m_entity_command_buffers.end())
                return fnd->second;

            auto* ebuf = new _entity_command_buffer{};
            ebuf->m_entity_removed_flag = false;
            ebuf->m_entity_active_stat = jeecs::game_entity::entity_stat::UNAVAILABLE;

            auto result = _m_entity_command_buffers.insert(std::make_pair(e, ebuf)).second;
            (void)result;
            assert(result);

            return ebuf;
        }
        _world_command_buffer* _find_or_create_buffer_for_world()
        {
            do
            {
                std::shared_lock sg1(_m_command_buffer_mx);
                if (_m_world_command_buffer != nullptr)
                    return _m_world_command_buffer;

            } while (0);

            std::lock_guard g1(_m_command_buffer_mx);
            if (_m_world_command_buffer != nullptr)
                return _m_world_command_buffer;

            auto* wbuf = new _world_command_buffer{};

            wbuf->m_destroy_world = false;
            wbuf->m_update_enabled = std::nullopt;

            return _m_world_command_buffer = wbuf;
        }

    public:
        command_buffer(ecs_world* world)
            : _m_world(world), _m_world_command_buffer(nullptr)
        {
        }
        ~command_buffer()
        {
            assert(_m_entity_command_buffers.empty() && _m_world_command_buffer == nullptr);
        }

        void init_new_entity(const arch_type::entity& e, jeecs::game_entity::entity_stat stat)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);
            assert(stat != jeecs::game_entity::entity_stat::UNAVAILABLE);
            _find_or_create_buffer_for_entity(e)->m_entity_active_stat = stat;
        }

        void remove_entity(const arch_type::entity& e)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for_entity(e)->m_entity_removed_flag = true;
        }

        void* append_component(const arch_type::entity& e, jeecs::typing::typeid_t component_type)
        {
            const auto* tinfo = jeecs::typing::type_info::of(component_type);
            std::shared_lock sl(_m_command_executer_guard_mx);

            // Instance component
            void* created_component = je_mem_alloc(tinfo->m_size);
            tinfo->construct(created_component);

            _find_or_create_buffer_for_entity(e)->m_adding_or_removing_components.add_one(
                new _entity_command_buffer::typed_component(component_type, created_component));

            return created_component;
        }
        void remove_component(const arch_type::entity& e, jeecs::typing::typeid_t component_type)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for_entity(e)->m_adding_or_removing_components.add_one(
                new _entity_command_buffer::typed_component(component_type, nullptr));
        }
        void close_world()
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p the destroy world operation has been committed to the command buffer.",
                _m_world);

            _find_or_create_buffer_for_world()->m_destroy_world = true;
        }

        void set_able_world(bool enable)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            if (enable)
                DEBUG_ARCH_LOG("World: %p trying to enable world.",
                    _m_world);
            else
                DEBUG_ARCH_LOG("World: %p trying to disable world.",
                    _m_world);

            _find_or_create_buffer_for_world()->m_update_enabled = std::optional(enable);
        }

        void add_system_instance(const jeecs::typing::type_info* type, jeecs::game_system* sys_instance)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p want to add system(%p) named '%s', operation has been committed.",
                _m_world, sys_instance, type->m_typename);

            assert(sys_instance);

            _find_or_create_buffer_for_world()->m_adding_or_removing_systems.add_one(
                new _world_command_buffer::typed_system(type, sys_instance));
        }
        void remove_system_instance(const jeecs::typing::type_info* type)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            DEBUG_ARCH_LOG("World: %p want to remove system named '%s', operation has been committed.",
                _m_world, type->m_typename);

            _find_or_create_buffer_for_world()->m_adding_or_removing_systems.add_one(
                new _world_command_buffer::typed_system(type, nullptr));
        }
        void add_system_query_cache(
            jeecs::game_system* system_instance,
            jeecs::typing::typehash_t hash,
            std::unique_ptr<jeecs::dependence> dependence)
        {
            std::shared_lock sl(_m_command_executer_guard_mx);

            _find_or_create_buffer_for_world()->m_adding_query_caches.add_one(
                new _world_command_buffer::append_slice_query_cache{
                    system_instance,
                    hash,
                    std::move(dependence),
                });
        }

    public:
        void update();
    };
    class ecs_world
    {
        JECS_DISABLE_MOVE_AND_COPY(ecs_world);

    public:
        using system_container_t =
            std::unordered_map<const jeecs::typing::type_info*, jeecs::game_system*>;
        using slice_cache_container_t =
            std::unordered_map<jeecs::typing::typehash_t, std::unique_ptr<jeecs::dependence>>;

        // NOTE: 此处之所以要根据不同系统实例缓存不同的切片，是考虑到编译防火墙，不同编译器/库对
        //      相同/不同的切片类型哈希可能不同/相同；为了规避因此导致的哈希冲突或者重复，针对不同
        //      实例缓存，这样确保一个实例对应的始终是相同的模块（库），避免编译防火墙问题。
        using system_slice_cache_container_t =
            std::unordered_map<jeecs::game_system*, slice_cache_container_t>;
    private:
        std::string _m_name;

        ecs_universe* _m_universe;

        command_buffer _m_command_buffer;
        arch_manager _m_arch_manager;

        bool _m_world_enabled;

        std::atomic_bool _m_destroying_flag;
        std::atomic_size_t _m_archmgr_updated_version;

        system_container_t m_systems;
        system_slice_cache_container_t m_system_slice_caches;
    private:
        inline static std::shared_mutex _m_alive_worlds_mx;
        inline static std::unordered_set<ecs_world*> _m_alive_worlds;

    public:
        ecs_world(ecs_universe* universe)
            : _m_name("<anonymous>")
            , _m_universe(universe)
            , _m_command_buffer(this)
            , _m_arch_manager(this)
            , _m_world_enabled(false)
            , _m_destroying_flag(false)
            , _m_archmgr_updated_version(100)
        {
            std::lock_guard g1(_m_alive_worlds_mx);
            _m_alive_worlds.insert(this);
        }
        ~ecs_world()
        {
            // Must be destroyed before destruct.
            assert(is_destroying());

            std::lock_guard g1(_m_alive_worlds_mx);
            _m_alive_worlds.erase(this);
        }
        static bool is_valid(ecs_world* world) noexcept
        {
            std::shared_lock sg1(_m_alive_worlds_mx);
            return _m_alive_worlds.find(world) != _m_alive_worlds.end() && !world->is_destroying();
        }
        system_container_t& get_system_instances() noexcept
        {
            return m_systems;
        }
        void _destroy_system_instance(
            const jeecs::typing::type_info* type, jeecs::game_system* sys) noexcept
        {
            if (_m_world_enabled)
                type->m_system_updaters->m_on_disable(sys);

            type->destruct(sys);
            je_mem_free(sys);
        }
        void execute_world_job(ecs_job* job) noexcept
        {
            assert(job->m_job_type == ecs_job::job_type::FOR_WORLDS);
            if (_m_world_enabled)
                job->m_for_worlds_job(this, job->m_custom_data);
        }
        void append_system_instance(const jeecs::typing::type_info* type, jeecs::game_system* sys) noexcept
        {
            auto fnd = m_systems.find(type);
            if (fnd == m_systems.end())
            {
                if (_m_world_enabled)
                    type->m_system_updaters->m_on_enable(sys);

                auto result = m_systems.insert(std::make_pair(type, sys)).second;
                (void)result;
                assert(result);

                // Create system slice cache for this system
                result = m_system_slice_caches.insert(
                    std::make_pair(sys, slice_cache_container_t{})).second;
                (void)result;
                assert(result);
            }
            else
            {
#ifndef NDEBUG
                jeecs::debug::logwarn(
                    "Trying to append system: '%s', but current world(%p) has already contain same one(%p), replace it with %p",
                    type->m_typename,
                    this, fnd->second,
                    sys);
#endif
                remove_system_instance(type);
                append_system_instance(type, sys);
            }
        }
        void remove_system_instance(const jeecs::typing::type_info* type) noexcept
        {
            auto fnd = m_systems.find(type);
            if (fnd != m_systems.end())
            {
                _destroy_system_instance(fnd->first, fnd->second);

                m_system_slice_caches.erase(fnd->second);
                m_systems.erase(fnd);
            }
#ifndef NDEBUG
            else
            {
                jeecs::debug::logwarn(
                    "Trying to remove system: '%s', but current world(%p) donot have this system.",
                    type->m_typename,
                    this);
            }
#endif
        }

        void append_slice_cache_for_system_instance(
            jeecs::game_system* sys,
            jeecs::typing::typehash_t hash,
            std::unique_ptr<jeecs::dependence>&& dependence)
        {
            auto fnd = m_system_slice_caches.find(sys);
            if (fnd != m_system_slice_caches.end())
            {
                auto& slice_caches = fnd->second;
                auto fnd2 = slice_caches.find(hash);
                if (fnd2 == slice_caches.end())
                {
                    slice_caches.insert(
                        std::make_pair(hash, std::move(dependence)));
                }
                // Else, already exist, do nothing.
            }
            // Else, system instance not exist, do nothing.
#ifndef NDEBUG
            else
            {
                jeecs::debug::logwarn(
                    "Trying to add slice cache for system instance: %p, but current world(%p) donot have this system instance.",
                    sys,
                    this);
            }
#endif
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
        size_t archtype_mgr_updated_version() const noexcept
        {
            return _m_archmgr_updated_version;
        }

        void update_dependence_archinfo(jeecs::dependence* require) const noexcept
        {
            _get_arch_mgr().update_dependence_archinfo(require);
        }

        bool update()
        {
            // Complete command buffer:
            _m_command_buffer.update();

            bool in_destroy = is_destroying();
            if (in_destroy)
            {
                // Remove all system from world.
                std::vector<const jeecs::typing::type_info*> _removing_sys_types;
                _removing_sys_types.reserve(m_systems.size());

                for (auto& [sys_type, sys_instance] : m_systems)
                {
                    (void)sys_instance;
                    _removing_sys_types.push_back(sys_type);
                }

                for (auto type : _removing_sys_types)
                    remove_system_instance(type);

                // Find all entity to close.
                _m_arch_manager.close_all_entity(this);

                // After this round, we should do a round of command buffer update, then close this world.
                _m_command_buffer.update();
            }

            if (_m_arch_manager._arch_modified())
            {
                ++_m_archmgr_updated_version;

                // Arch types modified, all dependence arch info need to be updated.
                for (auto& [system_instance, slice_cache] : m_system_slice_caches)
                {
                    (void)system_instance;
                    for (auto& [hash, depend] : slice_cache)
                    {
                        (void)hash;
                        depend->update(this);
                    }
                }
            }

            return !in_destroy;
        }

        inline arch_type::entity create_entity_with_component(const types_set& types, jeecs::game_entity::entity_stat stat)
        {
            if (is_destroying())
            {
                jeecs::debug::logwarn("It's not allowed to create entity while world is closing.");
                return {};
            }

            auto entity = _m_arch_manager.create_an_entity_with_component(types);
            _m_command_buffer.init_new_entity(entity, stat);
            return entity;
        }
        inline arch_type::entity create_entity_with_prefab(const arch_type::entity* prefab)
        {
            if (is_destroying())
            {
                jeecs::debug::logwarn("It's not allowed to create entity while world is closing.");
                return {};
            }
            if (!prefab->is_valid())
            {
                jeecs::debug::logerr("It's not allowed to create entity with invalid prefab entity, very dangerous.");
                return {};
            }

            auto entity = prefab->chunk()->get_arch_type()->instance_entity(prefab);
            _m_command_buffer.init_new_entity(entity, jeecs::game_entity::entity_stat::READY);
            return entity;
        }
        inline jeecs::game_system* request_to_append_system(const jeecs::typing::type_info* type)
        {
            if (is_destroying())
            {
                jeecs::debug::logwarn("It's not allowed to add system while world is closing.");
                return nullptr;
            }

            jeecs::game_system* sys = (jeecs::game_system*)je_mem_alloc(type->m_size);
            type->construct(sys, this);
            get_command_buffer().add_system_instance(type, sys);
            return sys;
        }
        inline void request_to_remove_system(const jeecs::typing::type_info* type)
        {
            get_command_buffer().remove_system_instance(type);
        }
        inline void request_to_set_able(bool enable) noexcept
        {
            get_command_buffer().set_able_world(enable);
        }
        inline bool fetch_and_request_slice_cache_dependence(
            jeecs::game_system* system_instance,
            jeecs::typing::typehash_t hash,
            jeecs::dependence** out_dependence)
        {
            // NOTE: `m_system_slice_caches` 只进行读操作，以确保安全。不允许在命令缓冲区处理期间
            //      执行 `fetch_and_request_slice_cache_dependence`

            auto fnd = m_system_slice_caches.find(system_instance);
            if (fnd != m_system_slice_caches.end())
            {
                auto fnd2 = fnd->second.find(hash);
                if (fnd2 != fnd->second.end())
                {
                    *out_dependence = fnd2->second.get();
                    return true;
                }
            }

            auto created_dependence = std::make_unique<jeecs::dependence>();
            *out_dependence = created_dependence.get();

            // NOTE: dependence 的需求将在外部初始化，更新也需要由外部执行

            get_command_buffer().add_system_query_cache(
                system_instance,
                hash,
                std::move(created_dependence));

            return false;
        }

        inline command_buffer& get_command_buffer() noexcept
        {
            return _m_command_buffer;
        }
        inline bool is_destroying() const noexcept
        {
            return _m_destroying_flag;
        }
        inline bool is_enabled() const noexcept
        {
            return _m_world_enabled;
        }
        inline void _ready_to_destroy() noexcept
        {
            _m_destroying_flag = true;
        }
        inline void _set_able_world(bool enable) noexcept
        {
            if (_m_world_enabled != enable)
            {
                if (enable)
                {
                    // Invoke OnActive
                    for (auto& sys : m_systems)
                        sys.first->m_system_updaters->m_on_enable(sys.second);
                }
                else
                {
                    // Invoke OnDisable
                    for (auto& sys : m_systems)
                        sys.first->m_system_updaters->m_on_disable(sys.second);
                }

                _m_world_enabled = enable;
            }
        }
        inline ecs_universe* get_universe() const noexcept
        {
            return _m_universe;
        }
    };

    void default_job_for_execute_sys_update_for_worlds(void* _ecs_world, void* _);

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
            _m_entity_command_buffers.begin(), _m_entity_command_buffers.end(),
            [](std::pair<const arch_type::entity, _entity_command_buffer*>& _buf_in_entity)
            {
                //  If entity not valid, skip component append&remove&active, but need
                // free temp components.
                arch_type::entity current_entity = _buf_in_entity.first;

                auto* modify_typed_components = _buf_in_entity.second->m_adding_or_removing_components.pick_all();
                std::unordered_map<jeecs::typing::typeid_t, void*> modifying_component_type_and_instances;

                while (modify_typed_components != nullptr)
                {
                    auto* current_modify_typed_components = modify_typed_components;
                    modify_typed_components = modify_typed_components->last;

                    if (!modifying_component_type_and_instances.insert(
                        std::make_pair(
                            current_modify_typed_components->m_type_id,
                            current_modify_typed_components->m_component_addr))
                        .second)
                    {
                        // Operation failed, there is a new operation has overwrite this operation.
                        if (current_modify_typed_components->m_component_addr != nullptr)
                        {
                            // If trying to append component instance, this request has been replaced, free it.
                            jeecs::typing::type_info::of(current_modify_typed_components->m_type_id)
                                ->destruct(current_modify_typed_components->m_component_addr);

                            je_mem_free(current_modify_typed_components->m_component_addr);
                        }
                    }
                    delete current_modify_typed_components;
                }

                if (current_entity.is_valid())
                {
                    if (_buf_in_entity.second->m_entity_removed_flag)
                    {
                        // Remove all new component;
                        for (auto& [tid, instance] : modifying_component_type_and_instances)
                        {
                            if (instance != nullptr)
                            {
                                jeecs::typing::type_info::of(tid)->destruct(instance);
                                je_mem_free(instance);
                            }
                        }

                        // Remove all component
                        for (jeecs::typing::typeid_t type_id : *current_entity.chunk()->types())
                        {
                            current_entity.chunk()->destruct_component_addr_with_typeid(
                                current_entity._m_id,
                                type_id);
                        }

                        // OK, Mark old entity chunk is freed,
                        current_entity.chunk()->command_close_entity(current_entity._m_id);
                    }
                    else
                    {
                        // 1. Mark entity as active..
                        if (_buf_in_entity.second->m_entity_active_stat != jeecs::game_entity::entity_stat::UNAVAILABLE)
                            current_entity.chunk()->command_active_entity(current_entity._m_id, _buf_in_entity.second->m_entity_active_stat);

                        if (current_entity.chunk()->get_entity_meta()[current_entity._m_id].m_stat == jeecs::game_entity::entity_stat::PREFAB)
                        {
                            // Remove all new component;
                            for (auto& [tid, instance] : modifying_component_type_and_instances)
                            {
                                if (instance != nullptr)
                                {
                                    const auto* tinfo = jeecs::typing::type_info::of(tid);

                                    jeecs::debug::logerr("Cannot add component: '%s' to prefab(%p:%zuv%zu).",
                                        tinfo->m_typename,
                                        current_entity._m_in_chunk,
                                        (size_t)current_entity._m_id,
                                        (size_t)current_entity._m_version);

                                    tinfo->destruct(instance);
                                    je_mem_free(instance);
                                }
                                else
                                {
                                    jeecs::debug::logerr("Cannot remove component: '%s' from prefab(%p:%zuv%zu).",
                                        jeecs::typing::type_info::of(tid)->m_typename,
                                        current_entity._m_in_chunk,
                                        (size_t)current_entity._m_id,
                                        (size_t)current_entity._m_version);
                                }
                            }
                        }
                        else
                        {
                            types_set new_chunk_types = *current_entity.chunk()->types();

                            // 2. Apply modify!
                            std::unordered_map<jeecs::typing::typeid_t, void*> append_component_type_addr_map;
                            for (auto& [tid, instance] : modifying_component_type_and_instances)
                            {
                                if (instance == nullptr)
                                {
                                    // Trying to remove.
                                    if (new_chunk_types.erase(tid))
                                    {
                                        current_entity.chunk()->destruct_component_addr_with_typeid(
                                            current_entity._m_id,
                                            tid);
                                    }
#ifndef NDEBUG
                                    else
                                        jeecs::debug::logwarn("Trying to remove '%s' from entity '%p:%zuv%zu', but the specify entity donot contain this component, skip.",
                                            jeecs::typing::type_info::of(tid)->m_typename,
                                            current_entity._m_in_chunk, current_entity._m_id, current_entity._m_version);
#endif
                                }
                                else
                                {
                                    append_component_type_addr_map[tid] = instance;

                                    // Trying to append.
                                    if (new_chunk_types.find(tid) == new_chunk_types.end())
                                        new_chunk_types.insert(tid);
                                    else
                                    {
#ifndef NDEBUG
                                        jeecs::debug::logwarn("Trying to append '%s' to entity '%p:%zuv%zu', but the specify entity already contained this component, replace it.",
                                            jeecs::typing::type_info::of(tid)->m_typename,
                                            current_entity._m_in_chunk, current_entity._m_id, current_entity._m_version);
#endif
                                        // Old component already contained in this entity.
                                        // Destroy old one for replacing.
                                        current_entity.chunk()->destruct_component_addr_with_typeid(
                                            current_entity._m_id, tid);
                                    }
                                }
                            }

                            // 3. Almost done! get new arch type:
                            auto* current_arch_type = current_entity.chunk()->get_arch_type();
                            auto* new_arch_type_my_null = current_arch_type->get_arch_mgr()->find_or_add_arch(new_chunk_types);

                            if (new_arch_type_my_null == current_arch_type)
                            {
                                // New & old arch is same, rebuilt in place.
                                for (auto [tid, instance] : modifying_component_type_and_instances)
                                {
                                    if (instance == nullptr)
                                        // NOTE: 照理来说，如果instance == nullptr，说明正在移除实体中的组件，
                                        //      此时如果new_arch_type_my_null == current_arch_type，那么说明
                                        //      本次移除实际上是失败的（即实际上实体没有这个组件），那么直接
                                        //      跳过这个操作
                                        continue;

                                    current_entity.chunk()->move_component_from(current_entity._m_id, tid, instance);
                                    je_mem_free(instance);
                                }
                            }
                            else
                            {
                                if (new_arch_type_my_null != nullptr)
                                {
                                    arch_type::arch_chunk* chunk;
                                    jeecs::typing::entity_id_in_chunk_t entity_id;
                                    jeecs::typing::version_t entity_version;

                                    new_arch_type_my_null->alloc_entity(&chunk, &entity_id, &entity_version);
                                    // Entity alloced, move component to here..

                                    for (jeecs::typing::typeid_t type_id : new_chunk_types)
                                    {
                                        auto fnd = append_component_type_addr_map.find(type_id);
                                        if (fnd == append_component_type_addr_map.end())
                                        {
                                            // 1. Move old component
                                            chunk->move_component_from(entity_id, type_id,
                                                current_entity.chunk()->get_component_addr_with_typeid(
                                                    current_entity._m_id, type_id));
                                        }
                                        else
                                        {
                                            // 2. Move new component
                                            assert(fnd->second != nullptr);

                                            chunk->move_component_from(entity_id, type_id, fnd->second);
                                            je_mem_free(fnd->second);
                                        }
                                    }

                                    // Active new one
                                    assert(current_entity.chunk()->get_entity_meta()[current_entity._m_id].m_stat
                                        == jeecs::game_entity::entity_stat::READY);
                                    chunk->command_active_entity(entity_id, jeecs::game_entity::entity_stat::READY);
                                }
                                else
                                {
                                    assert(new_chunk_types.empty());
                                }

                                // OK, Mark old entity chunk is freed,
                                current_entity.chunk()->command_close_entity(current_entity._m_id);
                            }
                        }
                    } // End component modify
                }
                else
                {
                    // Remove all new component;
                    for (auto& [tid, instance] : modifying_component_type_and_instances)
                    {
                        if (instance != nullptr)
                        {
                            jeecs::typing::type_info::of(tid)->destruct(instance);
                            je_mem_free(instance);
                        }
                    }
                }

                delete _buf_in_entity.second;
            });

        // Finish! clear buffer.
        _m_entity_command_buffers.clear();

        // 在此处整理ArchType的内存碎片：
        _m_world->_get_arch_mgr().command_defragmentation();

        /////////////////////////////////////////////////////////////////////////////////////

        if (_m_world_command_buffer != nullptr)
        {
            if (_m_world_command_buffer->m_destroy_world)
                _m_world->_ready_to_destroy();

            // Apply universe world is enabled/disabled.
            if (_m_world_command_buffer->m_update_enabled.has_value())
            {
                _m_world->_set_able_world(_m_world_command_buffer->m_update_enabled.value());
                _m_world_command_buffer->m_update_enabled = std::nullopt;
            }

            if (_m_world->is_enabled() || _m_world->is_destroying())
            {
                std::unordered_map<const jeecs::typing::type_info*, jeecs::game_system*> modifying_system_type_and_instances;

                auto* append_or_remove_system = _m_world_command_buffer->m_adding_or_removing_systems.pick_all();
                while (append_or_remove_system)
                {
                    auto* cur_append_or_remove_system = append_or_remove_system;
                    append_or_remove_system = append_or_remove_system->last;

                    if (!modifying_system_type_and_instances.insert(
                        std::make_pair(
                            cur_append_or_remove_system->m_typeinfo,
                            cur_append_or_remove_system->m_add_system_instance))
                        .second)

                    {
                        if (cur_append_or_remove_system->m_add_system_instance != nullptr)
                        {
                            cur_append_or_remove_system->m_typeinfo->destruct(cur_append_or_remove_system->m_add_system_instance);
                            je_mem_free(cur_append_or_remove_system->m_add_system_instance);
                        }
                    }

                    delete cur_append_or_remove_system;
                }

                for (auto [typeinfo, instance] : modifying_system_type_and_instances)
                {
                    if (instance == nullptr)
                        _m_world->remove_system_instance(typeinfo);
                    else
                        _m_world->append_system_instance(typeinfo, instance);
                }

                auto* append_query_caches = _m_world_command_buffer->m_adding_query_caches.pick_all();
                while (append_query_caches)
                {
                    auto* cur_append_query_caches = append_query_caches;
                    append_query_caches = append_query_caches->last;

                    _m_world->append_slice_cache_for_system_instance(
                        cur_append_query_caches->m_system_instance_may_not_exist,
                        cur_append_query_caches->m_query_slice_typehash,
                        std::move(cur_append_query_caches->m_created_dependence));

                    delete cur_append_query_caches;
                }

                delete _m_world_command_buffer;
                _m_world_command_buffer = nullptr;
            }
        }
    }

    // ecs_universe
    class ecs_universe
    {
        inline static std::mutex _m_alive_universes_mx;
        inline static std::list<ecs_universe*> _m_alive_universes;

        std::vector<ecs_world*> _m_world_list;

        std::mutex _m_removing_worlds_appending_mx;

        std::thread _m_universe_update_thread;
        std::atomic_flag _m_universe_update_thread_stop_flag = {};

        // Used for store shared jobs instance.
        std::vector<ecs_job*> _m_shared_pre_jobs;
        std::vector<ecs_job*> _m_shared_jobs;
        std::vector<ecs_job*> _m_shared_after_jobs;

        std::mutex _m_next_execute_interval_mx;

        double _m_frame_current_time = 0.;
        double _m_real_current_time = 0.;

        // 期待的universe同步间隔
        // 由于图形计算等开销，实际上的同步间隔可能大于此值
        double _m_frame_deltatime = 1. / 60.;

        // Universe每次Update时与实际世界时间进行计算得到的值
        double _m_real_deltatime = 1. / 60.;

        // Universe每次Update时与实际世界时间进行计算得到的值，与过去若干帧的平均更新间隔
        double _m_smooth_deltatime = 1. / 60.;

        // 最大DeltaTime值，防止出现巨大的跳变
        double _m_max_deltatime = 0.2;

        // 时间缩放
        double _m_time_scale = 1.0;

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
            void (*m_free_function)(void*);

            union
            {
                ecs_world* m_adding_world;
                ecs_job::job_call_once_t m_call_once_job;
                ecs_job::job_for_worlds_t m_for_worlds_job;
            };
            universe_action* last;
        };

        jeecs::basic::atomic_list<universe_action> _m_universe_actions;

        void append_universe_action(universe_action* act) noexcept
        {
            _m_universe_actions.add_one(act);
        }

    public:
        void set_frame_deltatime(double delta)
        {
            _m_frame_deltatime = delta;
        }
        double get_frame_deltatime() const
        {
            return _m_frame_deltatime;
        }

        double get_real_deltatime() const
        {
            return _m_real_deltatime * _m_time_scale;
        }
        double get_smooth_deltatime() const
        {
            return _m_smooth_deltatime * _m_time_scale;
        }

        double get_max_deltatime() const
        {
            return _m_max_deltatime;
        }
        void set_max_deltatime(double val)
        {
            _m_max_deltatime = val;
        }

        double get_time_scale() const
        {
            return _m_time_scale;
        }
        void set_time_scale(double scale)
        {
            _m_time_scale = std::max(0., scale);
        }

        void update_universe_action_and_worlds() noexcept
        {
            std::vector<ecs_world*> _m_removing_worlds;

            // Update all worlds, if world is closing, add it to _m_removing_worlds.
            ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                [this, &_m_removing_worlds](ecs_world* world)
                {
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
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
                        [cur_action](ecs_job* _job)
                        {
                            return _job->m_job_type == ecs_job::job_type::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
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
            // 0. 更新、处理Universe消息和世界更新，完成组件、实体、系统相关的
            //      增加移除和移动操作等
            update_universe_action_and_worlds();

            // 计算 DeltaTime
            double current_time = je_clock_time();
            _m_real_deltatime = jeecs::math::clamp(
                current_time - _m_real_current_time, 0., _m_max_deltatime);
            _m_smooth_deltatime = (_m_smooth_deltatime * 9. + _m_real_deltatime) / 10.;
            _m_real_current_time = current_time;

            // 一个逻辑帧从此处开始：
            // 1. 完成预先任务
            ParallelForeach(
                _m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                [this](ecs_job* shared_job)
                {
                    if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                        ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                            [shared_job](ecs_world* world)
                            {
                                world->execute_world_job(shared_job);
                            });
                    else
                        shared_job->m_call_once_job(shared_job->m_custom_data);
                });

            // 2. 完成常规任务
            ParallelForeach(
                _m_shared_jobs.begin(), _m_shared_jobs.end(),
                [this](ecs_job* shared_job)
                {
                    if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                        ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                            [shared_job](ecs_world* world)
                            {
                                world->execute_world_job(shared_job);
                            });
                    else
                        shared_job->m_call_once_job(shared_job->m_custom_data);
                });

            // 在此执行引擎的主动同步机制（如果需要）
            //
            // NOTE: 渲染画面等呈现型操作将在稍后呈现，为保证非垂直同步设定下
            //      呈现出的内容尽可能平滑，在此保证同步时间点完成后即刻提交
            if (_m_frame_deltatime > 0.)
            {
                _m_frame_current_time += _m_frame_deltatime;
                je_clock_sleep_until(_m_frame_current_time);
                if (current_time - _m_frame_current_time >= _m_max_deltatime)
                    _m_frame_current_time = current_time;
            }

            // 3. 完成提交任务
            //
            // NOTE: 渲染等呈现操作必须在提交完成后、世界更新前完成，为了规避图形资源
            //      因移除组件等行为而释放，产生错误的渲染结果——尽管高阶图形接口提供
            //      了图形资源安全和基本正确性保证；
            ParallelForeach(
                _m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                [this](ecs_job* shared_job)
                {
                    if (shared_job->m_job_type == ecs_job::job_type::FOR_WORLDS)
                        ParallelForeach(_m_world_list.begin(), _m_world_list.end(),
                            [shared_job](ecs_world* world)
                            {
                                world->execute_world_job(shared_job);
                            });
                    else
                        shared_job->m_call_once_job(shared_job->m_custom_data);
                });
        }

    public:
        ecs_universe()
        {
            std::lock_guard g1(_m_alive_universes_mx);
            _m_alive_universes.push_back(this);

            DEBUG_ARCH_LOG("Ready to create ecs_universe: %p.", this);

            // Append default jobs for updating systems.
            je_ecs_universe_register_for_worlds_job(this,
                default_job_for_execute_sys_update_for_worlds, nullptr, nullptr);

            _m_universe_update_thread_stop_flag.test_and_set();
            _m_universe_update_thread = std::thread(
                [this]()
                {
                    while (true)
                    {
                        update();
                        if (_m_world_list.empty() && !_m_universe_update_thread_stop_flag.test_and_set())
                            // If there is no world alive, and exit flag is setten, exit this thread.
                            break;
                    }

                    // Make sure universe action empty.
                    do
                    {
                        update();
                        assert(_m_universe_actions.peek() == nullptr);

                        // invoke callback
                        auto* callback_func_node = m_exit_callback_list.pick_all();
                        while (callback_func_node)
                        {
                            auto* current_callback = callback_func_node;
                            callback_func_node = callback_func_node->last;

                            current_callback->m_method();
                            delete current_callback;
                        }
                    } while (_m_universe_actions.peek() != nullptr);

                    // Free all ecs_jobs.
                    for (ecs_job* pre_job : _m_shared_pre_jobs)
                        delete pre_job;
                    for (ecs_job* job : _m_shared_jobs)
                        delete job;
                    for (ecs_job* after_job : _m_shared_after_jobs)
                        delete after_job;

                    // Clear all jobs.
                    _m_shared_pre_jobs.clear();
                    _m_shared_jobs.clear();
                    _m_shared_after_jobs.clear();
                });

            DEBUG_ARCH_LOG("Universe: %p created.", this);
        }

        struct callback_function_node
        {
            callback_function_node* last;
            std::function<void(void)> m_method;
        };
        jeecs::basic::atomic_list<callback_function_node> m_exit_callback_list;

        inline void register_exit_callback(const std::function<void(void)>& function) noexcept
        {
            callback_function_node* node = new callback_function_node();
            node->m_method = function;
            m_exit_callback_list.add_one(node);
        }

        inline void register_pre_call_once_job(je_job_call_once_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_PRE_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_pre_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_PRE_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_call_once_job(je_job_call_once_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_NORMAL_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_NORMAL_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_after_call_once_job(je_job_call_once_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_AFTER_JOB_CALL_ONCE;
            action->m_call_once_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }
        inline void register_after_for_worlds_job(je_job_for_worlds_t job, void* custom_data, void (*free_function)(void*)) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::ADD_AFTER_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;
            action->m_custom_data = custom_data;
            action->m_free_function = free_function;
            append_universe_action(action);
        }

        inline void unregister_pre_call_once_job(je_job_call_once_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_PRE_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_pre_for_worlds_job(je_job_for_worlds_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_PRE_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }
        inline void unregister_call_once_job(je_job_call_once_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_for_worlds_job(je_job_for_worlds_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }
        inline void unregister_after_call_once_job(je_job_call_once_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_CALL_ONCE;
            action->m_call_once_job = job;

            append_universe_action(action);
        }
        inline void unregister_after_for_worlds_job(je_job_for_worlds_t job) noexcept
        {
            universe_action* action = new universe_action;
            action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_FOR_WORLDS;
            action->m_for_worlds_job = job;

            append_universe_action(action);
        }

        ~ecs_universe()
        {
            do
            {
                std::lock_guard g1(_m_alive_universes_mx);
                auto fnd = std::find(_m_alive_universes.begin(), _m_alive_universes.end(), this);
                assert(fnd != _m_alive_universes.end());
                _m_alive_universes.erase(fnd);
            } while (0);

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

    public:
        static void _shutdown_all_universe()
        {
            std::lock_guard g1(_m_alive_universes_mx);
            std::unordered_set<ecs_universe*> shutdown_universes;
            for (;;)
            {
                bool all_universe_has_shutdown = true;

                for (auto* u : _m_alive_universes)
                {
                    if (shutdown_universes.find(u) == shutdown_universes.end())
                    {
                        shutdown_universes.insert(u);
                        all_universe_has_shutdown = false;
                    }

                    u->stop_universe_loop();
                    if (u->_m_universe_update_thread.joinable())
                        u->_m_universe_update_thread.join();
                }

                if (all_universe_has_shutdown)
                    break;
            }
        }
    };

    void default_job_for_execute_sys_update_for_worlds(void* _ecs_world, void* _)
    {
        ecs_world* cur_world = (ecs_world*)_ecs_world;

        ecs_world::system_container_t& active_systems =
            cur_world->get_system_instances();

        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_pre_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_state_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_transform_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_physics_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_late_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_commit_update(val.second);
            });
        ParallelForeach(
            active_systems.begin(), active_systems.end(),
            [](ecs_world::system_container_t::value_type& val)
            {
                val.first->m_system_updaters->m_graphic_update(val.second);
            });
    }
}

void* je_ecs_universe_create()
{
    return new jeecs_impl::ecs_universe();
}

void je_ecs_universe_register_exit_callback(void* universe, void (*callback)(void*), void* arg)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_exit_callback(
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

    reinterpret_cast<jeecs_impl::ecs_universe*>(ecs_universe)->register_exit_callback(
        [&]()
        {
            std::lock_guard g1(exit_mx);
            exit_flag.clear();
            exit_cv.notify_all();
        });

    do
    {
        std::unique_lock uq1(exit_mx);
        exit_cv.wait(uq1, [&]() -> bool
            { return !exit_flag.test_and_set(); });
    } while (0);
}

void je_ecs_universe_destroy(void* ecs_universe)
{
    delete reinterpret_cast<jeecs_impl::ecs_universe*>(ecs_universe);
}

void je_ecs_universe_stop(void* ecs_universe)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(ecs_universe)->stop_universe_loop();
}

void* je_arch_get_chunk(void* archtype)
{
    return reinterpret_cast<jeecs_impl::arch_type*>(archtype)->get_head_chunk();
}

void* je_arch_next_chunk(void* chunk)
{
    return reinterpret_cast<jeecs_impl::arch_type::arch_chunk*>(chunk)->last;
}

void* je_ecs_world_create(void* in_universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(in_universe)->create_world();
}

void je_ecs_world_destroy(void* world)
{
    reinterpret_cast<jeecs_impl::ecs_world*>(world)->get_command_buffer().close_world();
}

jeecs::game_system* je_ecs_world_add_system_instance(void* world, jeecs::typing::typeid_t type)
{
    return reinterpret_cast<jeecs_impl::ecs_world*>(world)->request_to_append_system(jeecs::typing::type_info::of(type));
}

jeecs::game_system* je_ecs_world_get_system_instance(void* world, jeecs::typing::typeid_t type)
{
    auto& syss = reinterpret_cast<jeecs_impl::ecs_world*>(world)->get_system_instances();
    auto fnd = syss.find(jeecs::typing::type_info::of(type));
    if (fnd == syss.end())
        return nullptr;
    return fnd->second;
}

void je_ecs_world_remove_system_instance(void* world, jeecs::typing::typeid_t type)
{
    reinterpret_cast<jeecs_impl::ecs_world*>(world)->request_to_remove_system(jeecs::typing::type_info::of(type));
}

void je_ecs_world_set_able(void* world, bool enable)
{
    reinterpret_cast<jeecs_impl::ecs_world*>(world)->request_to_set_able(enable);
}

void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::typeid_t* component_ids)
{
    jeecs_impl::types_set types;
    while (*component_ids != jeecs::typing::INVALID_TYPE_ID)
        types.insert(*(component_ids++));

    auto&& entity = reinterpret_cast<jeecs_impl::ecs_world*>(world)
        ->create_entity_with_component(types, jeecs::game_entity::entity_stat::READY);

    out_entity->_set_arch_chunk_info(entity._m_in_chunk, entity._m_id, entity._m_version);
}
void je_ecs_world_create_prefab_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::typing::typeid_t* component_ids)
{
    jeecs_impl::types_set types;
    while (*component_ids != jeecs::typing::INVALID_TYPE_ID)
        types.insert(*(component_ids++));

    auto entity = reinterpret_cast<jeecs_impl::ecs_world*>(world)
        ->create_entity_with_component(types, jeecs::game_entity::entity_stat::PREFAB);
    out_entity->_set_arch_chunk_info(entity._m_in_chunk, entity._m_id, entity._m_version);
}

void je_ecs_world_create_entity_with_prefab(
    void* world,
    jeecs::game_entity* out_entity,
    const jeecs::game_entity* prefab)
{
    auto entity = reinterpret_cast<jeecs_impl::ecs_world*>(world)
        ->create_entity_with_prefab(
            std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(prefab)));
    out_entity->_set_arch_chunk_info(entity._m_in_chunk, entity._m_id, entity._m_version);
}

size_t je_ecs_world_archmgr_updated_version(void* world)
{
    return reinterpret_cast<jeecs_impl::ecs_world*>(world)->archtype_mgr_updated_version();
}

void je_ecs_world_update_dependences_archinfo(void* world, jeecs::dependence* dependence)
{
    reinterpret_cast<jeecs_impl::ecs_world*>(world)->update_dependence_archinfo(dependence);
}

void* je_ecs_world_entity_add_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type)
{
    auto* entity_located_world =
        reinterpret_cast<jeecs_impl::ecs_world*>(je_ecs_world_of_entity(entity));
    if (entity_located_world != nullptr)
        return entity_located_world->get_command_buffer().append_component(
            *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity)),
            type);
    return nullptr;
}

void je_ecs_world_entity_remove_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type)
{
    auto* entity_located_world =
        reinterpret_cast<jeecs_impl::ecs_world*>(je_ecs_world_of_entity(entity));
    if (entity_located_world != nullptr)
        entity_located_world->get_command_buffer().remove_component(
            *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity)),
            type);
}

const jeecs::game_entity::meta* je_arch_entity_meta_addr_in_chunk(void* chunk)
{
    return reinterpret_cast<jeecs_impl::arch_type::arch_chunk*>(chunk)->get_entity_meta();
}

void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    jeecs::typing::typeid_t type)
{
    auto& ecs_entity = *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity));
    auto* component = ecs_entity.get_component(type);
    return component;
}

void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity)
{
    std::launder(reinterpret_cast<jeecs_impl::ecs_world*>(world))
        ->get_command_buffer().remove_entity(
            *std::launder(reinterpret_cast<const jeecs_impl::arch_type::entity*>(entity)));
}

void* je_ecs_world_in_universe(void* world)
{
    return reinterpret_cast<jeecs_impl::ecs_world*>(world)->get_universe();
}

bool je_ecs_world_is_valid(void* world)
{
    return jeecs_impl::ecs_world::is_valid(
        std::launder(reinterpret_cast<jeecs_impl::ecs_world*>(world)));
}

void* je_ecs_world_of_entity(const jeecs::game_entity* entity)
{
    auto* chunk = reinterpret_cast<jeecs_impl::arch_type::arch_chunk*>(entity->_m_in_chunk);
    if (chunk != nullptr)
        return chunk->get_arch_type()->get_arch_mgr()->get_world();
    return nullptr;
}

bool je_ecs_world_query_slice_dependence(
    void* world,
    jeecs::game_system* system_instance,
    jeecs::typing::typehash_t slice_type_hash,
    jeecs::dependence** out_dependence)
{
    return std::launder(reinterpret_cast<jeecs_impl::ecs_world*>(world))
        ->fetch_and_request_slice_cache_dependence(
            system_instance, slice_type_hash, out_dependence);
}

//////////////////// FOLLOWING IS DEBUG EDITOR API ////////////////////
void** jedbg_get_all_worlds_in_universe(void* _universe)
{
    jeecs_impl::ecs_universe* universe = reinterpret_cast<jeecs_impl::ecs_universe*>(_universe);
    auto result = universe->_get_all_worlds();

    void** out_result = (void**)je_mem_alloc(sizeof(void*) * (result.size() + 1));

    void** write_place = out_result;
    for (auto* worlds : result)
        *(write_place++) = (void*)worlds;
    *write_place = nullptr;

    return out_result;
}

const char* jedbg_get_world_name(void* _world)
{
    return reinterpret_cast<jeecs_impl::ecs_world*>(_world)->_name().c_str();
}

void jedbg_set_world_name(void* _world, const char* name)
{
    reinterpret_cast<jeecs_impl::ecs_world*>(_world)->_name(name);
}

void jedbg_free_entity(jeecs::game_entity* _entity_list)
{
    delete _entity_list;
}

jeecs::game_entity** jedbg_get_all_entities_in_world(void* _world)
{
    jeecs_impl::ecs_world* world = reinterpret_cast<jeecs_impl::ecs_world*>(_world);

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
                    jeecs::game_entity* entity = new jeecs::game_entity();
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
    const auto* cur_chunk = reinterpret_cast<jeecs_impl::arch_type::arch_chunk*>(_entity->_m_in_chunk);
    auto& cur_arch_type_infos = cur_chunk->get_arch_type()->get_type_infos();

    const jeecs::typing::type_info** outresult = (const jeecs::typing::type_info**)
        je_mem_alloc(
            sizeof(const jeecs::typing::type_info*) * (cur_arch_type_infos.size() + 1));

    auto write_result = outresult;
    for (auto* typeinfo : cur_arch_type_infos)
    {
        *(write_result++) = typeinfo;
    }
    *write_result = nullptr;

    // Sort by id.
    std::sort(outresult, write_result,
        [](const jeecs::typing::type_info* a, const jeecs::typing::type_info* b)
        {
            return a->m_id < b->m_id;
        });

    return outresult;
}

const jeecs::typing::type_info** jedbg_get_all_system_attached_in_world(void* _world)
{
    jeecs_impl::ecs_world* world = reinterpret_cast<jeecs_impl::ecs_world*>(_world);
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

void jedbg_get_entity_arch_information(
    jeecs::game_entity* _entity,
    size_t* _out_chunk_size,
    size_t* _out_entity_size,
    size_t* _out_all_entity_count_in_chunk)
{
    auto* chunkaddr = reinterpret_cast<jeecs_impl::arch_type::arch_chunk*>(_entity->_m_in_chunk);

    *_out_chunk_size = jeecs_impl::CHUNK_SIZE;
    *_out_entity_size = chunkaddr->get_entity_size_in_chunk();
    *_out_all_entity_count_in_chunk = chunkaddr->get_entity_count_in_chunk();
}

void je_ecs_universe_register_pre_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_pre_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_pre_call_once_job(void* universe, je_job_call_once_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_pre_call_once_job(job, data, freefunc);
}
void je_ecs_universe_register_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_call_once_job(void* universe, je_job_call_once_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_call_once_job(job, data, freefunc);
}
void je_ecs_universe_register_after_for_worlds_job(void* universe, je_job_for_worlds_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_after_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_register_after_call_once_job(void* universe, je_job_call_once_t job, void* data, void (*freefunc)(void*))
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->register_after_call_once_job(job, data, freefunc);
}

void je_ecs_universe_unregister_pre_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_pre_for_worlds_job(job);
}
void je_ecs_universe_unregister_pre_call_once_job(void* universe, je_job_call_once_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_pre_call_once_job(job);
}
void je_ecs_universe_unregister_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_for_worlds_job(job);
}
void je_ecs_universe_unregister_call_once_job(void* universe, je_job_call_once_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_call_once_job(job);
}
void je_ecs_universe_unregister_after_for_worlds_job(void* universe, je_job_for_worlds_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_after_for_worlds_job(job);
}
void je_ecs_universe_unregister_after_call_once_job(void* universe, je_job_call_once_t job)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->unregister_after_call_once_job(job);
}

double je_ecs_universe_get_frame_deltatime(void* universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->get_frame_deltatime();
}
void je_ecs_universe_set_frame_deltatime(void* universe, double delta)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->set_frame_deltatime(delta);
}

double je_ecs_universe_get_real_deltatime(void* universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->get_real_deltatime();
}
double je_ecs_universe_get_smooth_deltatime(void* universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->get_smooth_deltatime();
}

double je_ecs_universe_get_max_deltatime(void* universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->get_max_deltatime();
}
void je_ecs_universe_set_max_deltatime(void* universe, double val)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->set_max_deltatime(val);
}
void je_ecs_universe_set_time_scale(void* universe, double scale)
{
    reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->set_time_scale(scale);
}
double je_ecs_universe_get_time_scale(void* universe)
{
    return reinterpret_cast<jeecs_impl::ecs_universe*>(universe)->get_time_scale();
}

///////////////////////

void je_ecs_finish()
{
    jeecs_impl::ecs_universe::_shutdown_all_universe();
}
