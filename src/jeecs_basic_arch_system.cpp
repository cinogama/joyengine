#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/basic.hpp"
#include "jeecs/clock.hpp"
#include "jeecs/core.hpp"
#include "jeecs/debug.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/game.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs_basic_bootup.hpp"

#include <list>
#include <thread>
#include <condition_variable>
#include <unordered_map>
#include <optional>

#ifdef NDEBUG
#define DEBUG_ARCH_LOG(...) ((void)0)
#define DEBUG_ARCH_LOG_WARN(...) ((void)0)
#else
#define DEBUG_ARCH_LOG(...) debug::loginfo(__VA_ARGS__)
#define DEBUG_ARCH_LOG_WARN(...) debug::logwarn(__VA_ARGS__)
#endif

#define OFFSET_OF(T, M) ((::size_t)&reinterpret_cast<char const volatile &>((((T *)0)->M)))

namespace jeecs::ecs {}

using namespace std;
using namespace jeecs;
using namespace jeecs::ecs;

namespace jeecs::ecs
{
    namespace
    {
        class ArchType;

        using TypesSet = vector<je_type_id_t>;
        namespace types_set
        {
            // TypesSet 是排序去重的 TypesSet，其不变式只能通过
            // 以下三个辅助函数维护；archetype 的身份与 map 键序都依赖它是升序无重复的。
            inline void insert(TypesSet& _types, je_type_id_t _tid) noexcept
            {
                auto _at = lower_bound(_types.begin(), _types.end(), _tid);
                if (_at == _types.end() || *_at != _tid)
                    _types.insert(_at, _tid);
            }
            inline bool contains(const TypesSet& _types, je_type_id_t _tid) noexcept
            {
                return binary_search(_types.begin(), _types.end(), _tid);
            }
            inline bool erase(TypesSet& _types, je_type_id_t _tid) noexcept
            {
                auto _at = lower_bound(_types.begin(), _types.end(), _tid);
                if (_at == _types.end() || *_at != _tid)
                    return false;
                _types.erase(_at);
                return true;
            }
            // 从未排序数组批量构建，一次分配 + 排序 + 去重；
            // 优于对逐元素调用 types_set::insert（每次插入都要搬移后半段）。
            inline TypesSet make(const je_type_id_t* _type_ids, size_t _count) noexcept
            {
                TypesSet _types(_type_ids, _type_ids + _count);
                sort(_types.begin(), _types.end());
                _types.erase(unique(_types.begin(), _types.end()), _types.end());
                return _types;
            }
        }

        struct SelectedArchCache
        {
            size_t* m_view_component_offset;
            size_t* m_view_component_size;

            SelectedArchCache(size_t view_size)
            {
                m_view_component_offset =
                    static_cast<size_t*>(malloc(2 * view_size * sizeof(size_t)));
                m_view_component_size = m_view_component_offset + view_size;

                assert(m_view_component_offset != nullptr);
            }
            ~SelectedArchCache()
            {
                if (m_view_component_offset != nullptr)
                    free(m_view_component_offset);
            }
            SelectedArchCache(SelectedArchCache&& another)
                : m_view_component_offset(another.m_view_component_offset)
                , m_view_component_size(another.m_view_component_size)
            {
                another.m_view_component_offset = nullptr;
            }
            SelectedArchCache& operator = (SelectedArchCache&& another)
            {
                m_view_component_offset = another.m_view_component_offset;
                m_view_component_size = another.m_view_component_size;
                another.m_view_component_offset = nullptr;

                return *this;
            }
            SelectedArchCache(const SelectedArchCache&) = delete;
            SelectedArchCache& operator = (const SelectedArchCache&) = delete;
        };
    }
}

// je_game_universe_t/je_game_world_t/je_archtype_t/je_chunk_t 在公开头文件中仅作不透明前向声明，
// 此处给出空结构体定义，作为实际实现类的公开基类。引擎内部实现类
// （Universe 等）公开继承它们，使 C ABI 边界处可通过
// derived↔base 的 static_cast 完成 je_Xxx* 与实现类指针之间的转换，
// 无需 reinterpret_cast。空基类优化（EBO）保证 je_chunk_t* 与 arch_chunk 对象地址一致。
struct je_game_universe_t {};
struct je_game_world_t {};
struct je_archtype_t {};
struct je_chunk_t {};

struct je_collected_requirements_t
{
    // 排序去重的紧凑类型集合：插入/删除/查找必须走 types_set_* 辅助函数
    TypesSet m_contain_set;
    TypesSet m_except_set;
    map<int /*je_component_requirement_kind_t*/, TypesSet> m_anyof_sets;

    vector<je_component_requirement_t> m_view_required;
    vector<je_dependence_arch_infos_t> m_selected_archs;

    unordered_map<const ArchType*, SelectedArchCache>
        m_selected_arch_cache;
};

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

namespace jeecs::ecs
{
    namespace
    {
        constexpr size_t CHUNK_SIZE = 16 * 1024; // 16K

        struct HoldedRequirementCollection : public je_requirement_collection_t
        {
            JECS_DISABLE_MOVE_AND_COPY(HoldedRequirementCollection);
            HoldedRequirementCollection()
            {
                m_collected_requirement = nullptr;
            }
            ~HoldedRequirementCollection()
            {
                if (m_collected_requirement != nullptr)
                    delete m_collected_requirement;
            }
        };

        class CommandBuffer;
        class ArchManager;
        class World;

        // 基于 Treiber 栈的 MPMC 无锁固定容量 id 栈，用于空闲 eid 池。
        // 约束：入栈值必须是 [0, capacity) 内的下标，且任意时刻每个值在栈中至多
        // 出现一次——free eid 池天然满足：eid 只有从“已分配”转为“已释放”时才会
        // 重新入栈。因此每个值独占一个链接槽位，直接用值本身索引 _m_next。
        // 栈头把“槽下标 + 修改计数”打包进一个 64 位原子量：计数随每次成功的栈头
        // 变更严格递增，用于消除经典 Treiber 栈的 ABA 问题——同一 eid 会被反复
        // 压栈/出栈，仅凭下标无法区分“同一个 id 的两次入栈”。
        template <typename T>
        class MCMPLockFreeStack
        {
            JECS_DISABLE_MOVE_AND_COPY(MCMPLockFreeStack);

            static constexpr uint64_t NIL = 0xFFFFFFFF;

            const uint32_t _m_capacity;
            atomic<uint32_t>* const _m_next; // _m_next[id]：id 在栈内的下一个下标
            atomic<uint64_t> _m_head;        // 低 32 位：栈顶下标或 NIL；高 32 位：ABA 计数

        public:
            explicit MCMPLockFreeStack(size_t capacity)
                : _m_capacity(static_cast<uint32_t>(capacity))
                , _m_next(new atomic<uint32_t>[capacity] {})
                , _m_head(NIL)
            {
                assert(capacity != 0 && capacity < NIL);
                assert(_m_head.is_lock_free());
            }
            ~MCMPLockFreeStack()
            {
                delete[] _m_next;
            }

            void push(T elem)
            {
                const uint64_t idx = static_cast<uint64_t>(elem);
                assert(idx < _m_capacity);

                uint64_t head = _m_head.load(memory_order_relaxed);
                for (;;)
                {
                    // 先挂接、再发布。即便与某个迟滞的 pop 并发读写同一槽位，
                    // 对方的 CAS 也会因高位计数不同而失败，不会消费到坏链接。
                    _m_next[idx].store(static_cast<uint32_t>(head), memory_order_relaxed);

                    const uint64_t new_head = ((head >> 32) + 1) << 32 | idx;
                    if (_m_head.compare_exchange_weak(
                        head, new_head, memory_order_release, memory_order_relaxed))
                        return;
                }
            }

            bool pop(T* out_value)
            {
                uint64_t head = _m_head.load(memory_order_acquire);
                for (;;)
                {
                    const uint64_t idx = head & 0xFFFFFFFF;
                    if (idx == NIL)
                        return false;

                    const uint64_t next = _m_next[idx].load(memory_order_relaxed);
                    const uint64_t new_head = ((head >> 32) + 1) << 32 | next;
                    if (_m_head.compare_exchange_weak(
                        head, new_head, memory_order_acquire, memory_order_acquire))
                    {
                        *out_value = static_cast<T>(idx);
                        return true;
                    }
                }
            }
        };

        class ArchType : public je_archtype_t
        {
            JECS_DISABLE_MOVE_AND_COPY(ArchType);
            // ahahahah, ArchType is coming!

            struct arch_type_info
            {
                const je_type_info_t* m_typeinfo;
                size_t m_begin_offset_in_chunk;
            };

        public:
            using types_list = vector<const je_type_info_t*>;
            using archtypes_map = unordered_map<je_type_id_t, arch_type_info>;

        private:
            /* Will be init in construction */
            const types_list _m_arch_typeinfo;
            const archtypes_map _m_arch_typeinfo_mapping;

            const TypesSet _m_types_set;
            const size_t _m_entity_size;
            const je_entity_id_in_chunk_t _m_entity_count_per_chunk;
            ArchManager* _m_arch_manager;

        public:
            class arch_chunk : public je_chunk_t
            {
                JECS_DISABLE_MOVE_AND_COPY(arch_chunk);

            private:
                using byte_t = uint8_t;
                static_assert(sizeof(byte_t) == 1, "sizeof(uint8_t) should be 1.");

                byte_t _m_chunk_buffer[CHUNK_SIZE];

                je_game_entity_meta_t* _m_entities_meta;

                const TypesSet* _m_types;
                const archtypes_map* _m_arch_typeinfo_mapping;
                const je_entity_id_in_chunk_t _m_entity_count;
                const size_t _m_entity_size;

                ArchType* _m_arch_type;
                MCMPLockFreeStack<je_entity_id_in_chunk_t>
                    _m_free_slots;
#ifndef NDEBUG
                atomic<je_entity_id_in_chunk_t> _m_debug_free_count;
#endif

            public:
                arch_chunk* last; // for atomic_list;
            public:
                arch_chunk(ArchType* _arch_type)
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
                    assert(OFFSET_OF(ArchType::arch_chunk, _m_chunk_buffer) == 0);

                    _m_entities_meta = new je_game_entity_meta_t[_m_entity_count]{};

                    // 逆序压栈：栈是 LIFO，这样首次分配仍按 0,1,2... 顺序取出，
                    // 与原先用队列时的分配顺序保持一致。
                    for (je_entity_id_in_chunk_t i = _m_entity_count; i-- > 0;)
                        _m_free_slots.push(i);
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
                inline void move_component_from(je_entity_id_in_chunk_t eid, je_type_id_t tid, void* from_component) const
                {
                    const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping->at(tid);
                    void* component_addr = get_component_addr(eid, arch_typeinfo.m_typeinfo->m_size, arch_typeinfo.m_begin_offset_in_chunk);
                    typing::move(arch_typeinfo.m_typeinfo, component_addr, from_component);
                    typing::destruct(arch_typeinfo.m_typeinfo, from_component);
                }

                bool alloc_entity_id(je_entity_id_in_chunk_t* out_id, je_version_t* out_version)
                {
                    if (_m_free_slots.pop(out_id))
                    {
#ifndef NDEBUG
                        if (0 == _m_debug_free_count--)
                            debug::logfatal("Entity free count broken!");
#endif
                        auto* meta = &_m_entities_meta[*out_id];

                        assert(meta->m_stat == JE_ENTITY_STAT_UNAVAILABLE);
                        *out_version = ++meta->m_version;
                        return true;
                    }
                    return false;
                }
                inline void* get_component_addr(je_entity_id_in_chunk_t _eid, size_t _chunksize, size_t _offset) const noexcept
                {
                    return (void*)(_m_chunk_buffer + _offset + _eid * _chunksize);
                }
                inline bool is_entity_valid(je_entity_id_in_chunk_t eid, je_version_t eversion) const noexcept
                {
                    if (_m_entities_meta[eid].m_version != eversion)
                        return false;
                    return true;
                }
                inline void* get_component_addr_with_typeid(je_entity_id_in_chunk_t eid, je_type_id_t tid) const noexcept
                {
                    auto fnd = _m_arch_typeinfo_mapping->find(tid);
                    if (fnd == _m_arch_typeinfo_mapping->end())
                        return nullptr;
                    const arch_type_info& arch_typeinfo = fnd->second;
                    return get_component_addr(
                        eid, arch_typeinfo.m_typeinfo->m_size, arch_typeinfo.m_begin_offset_in_chunk);
                }
                inline void destruct_component_addr_with_typeid(je_entity_id_in_chunk_t eid, je_type_id_t tid) const noexcept
                {
                    const arch_type_info& arch_typeinfo = _m_arch_typeinfo_mapping->at(tid);
                    auto* component_addr = get_component_addr(
                        eid, arch_typeinfo.m_typeinfo->m_size, arch_typeinfo.m_begin_offset_in_chunk);

                    typing::destruct(arch_typeinfo.m_typeinfo, component_addr);
                }
                inline const TypesSet* types() const noexcept
                {
                    return _m_types;
                }
                inline ArchType* get_arch_type() const noexcept
                {
                    return _m_arch_type;
                }
                inline const je_game_entity_meta_t* get_entity_meta() const noexcept
                {
                    return _m_entities_meta;
                }
                inline void close_all_entity(je_game_world_t* by_world)
                {
                    unordered_set<je_entity_id_in_chunk_t> free_entity_ids;
                    for (;;)
                    {
                        je_entity_id_in_chunk_t freeid;
                        if (!_m_free_slots.pop(&freeid))
                            break;

                        free_entity_ids.emplace(freeid);
                    }

                    for (je_entity_id_in_chunk_t eidx = 0; eidx < _m_entity_count; eidx++)
                    {
                        if (free_entity_ids.find(eidx) == free_entity_ids.end())
                        {
                            game_entity gentity;
                            gentity._set_arch_chunk_info(this, eidx, _m_entities_meta[eidx].m_version);

                            je_ecs_world_entity_destroy(by_world, &gentity._m_raw);
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

                inline unordered_set<je_entity_id_in_chunk_t>
                    _take_all_empty_for_defragmentation() noexcept
                {
                    // 为了碎片整理，我们需要将所有的空位都取出来
                    unordered_set<je_entity_id_in_chunk_t> empty_entity_ids;
                    for (;;)
                    {
                        je_entity_id_in_chunk_t freeid;
                        if (!_m_free_slots.pop(&freeid))
                            break;

                        assert(empty_entity_ids.find(freeid) == empty_entity_ids.end());
                        empty_entity_ids.emplace(freeid);
                    }
                    return empty_entity_ids;
                }

            private:
                // Following function only invoke by CommandBuffer
                friend class CommandBuffer;
                friend class ArchType;

                void command_active_entity(je_entity_id_in_chunk_t eid, je_entity_stat_t stat) noexcept
                {
                    _m_entities_meta[eid].m_stat = stat;
                }
                void command_add_free_slot(je_entity_id_in_chunk_t eid) noexcept
                {
                    _m_free_slots.push(eid);
                }
                void command_close_entity(je_entity_id_in_chunk_t eid) noexcept
                {
                    auto* meta = &_m_entities_meta[eid];

                    meta->m_stat = JE_ENTITY_STAT_UNAVAILABLE;
                    ++meta->m_version;

                    command_add_free_slot(eid);

                    ++_m_arch_type->_m_free_count;
#ifndef NDEBUG
                    ++_m_debug_free_count;
#endif
                }
            };

        private:
            atomic<je_entity_id_in_chunk_t> _m_free_count;
            atomic<je_entity_id_in_chunk_t> _m_total_count;

            basic::atomic_list<arch_chunk> _m_chunks;

            // 被碎片整理搬空的 chunk 回收池（单链表，复用 chunk->last 串联）：
            // 跨帧持有的旧实体句柄仍会解引用其所在 chunk 的元数据（版本号已
            // 失配，操作不生效），因此搬空的 chunk 不能直接 delete，否则旧句
            // 悬垂（UB）。后续分配新 chunk 时优先从池中复用。
            arch_chunk* _m_recycled_chunks;
            shared_mutex _m_recycled_chunks_mx;

            // 这个锁用于防止碎片整理的时候，有其他线程在操作chunk链表
            shared_mutex _m_chunk_list_defragmentation_mx;

        public:
            struct entity
            {
                arch_chunk* _m_in_chunk;
                je_entity_id_in_chunk_t _m_id;
                je_version_t _m_version;

                // Do not invoke this function if possiable, you should get component by ArchType & system.
                template <typename CT = void>
                inline CT* get_component(je_type_id_t tid) const
                {
                    if (_m_in_chunk != nullptr && _m_in_chunk->is_entity_valid(_m_id, _m_version))
                        return static_cast<CT*>(_m_in_chunk->get_component_addr_with_typeid(_m_id, tid));
                    return nullptr;
                }

                template <typename CT>
                inline CT* get_component() const
                {
                    return get_component<CT>(typing::id<CT>());
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
            ArchType(ArchManager* _arch_manager, const TypesSet& _types_set)
                : _m_types_set(_types_set)
                , _m_entity_size(0)
                , _m_entity_count_per_chunk(0)
                , _m_arch_manager(_arch_manager)
                , _m_free_count(0)
                , _m_total_count(0)
                , _m_recycled_chunks(nullptr)
            {
                static_assert(offsetof(game_entity, _m_raw) == 0);
                static_assert(offsetof(je_game_entity_t, _m_in_chunk) == offsetof(entity, _m_in_chunk));
                static_assert(offsetof(je_game_entity_t, _m_id) == offsetof(entity, _m_id));
                static_assert(offsetof(je_game_entity_t, _m_version) == offsetof(entity, _m_version));

                assert(!_types_set.empty());

                for (je_type_id_t tid : _types_set)
                    const_cast<types_list&>(_m_arch_typeinfo).push_back(typing::of(tid));

                sort(
                    const_cast<types_list&>(_m_arch_typeinfo).begin(),
                    const_cast<types_list&>(_m_arch_typeinfo).end(),
                    [](const je_type_info_t* a, const je_type_info_t* b)
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
                    entity_size += typeinfo->m_size;
                }
                const_cast<size_t&>(_m_entity_size) = entity_size;

                const size_t chunk_size_without_gap = CHUNK_SIZE - component_reserved_gap;
                assert(_m_entity_size != 0 && _m_entity_size <= chunk_size_without_gap);

                const_cast<je_entity_id_in_chunk_t&>(_m_entity_count_per_chunk) =
                    static_cast<je_entity_id_in_chunk_t>(chunk_size_without_gap / _m_entity_size);

                size_t mem_offset = 0;
                for (auto* typeinfo : _m_arch_typeinfo)
                {
                    mem_offset = basic::allign_size(mem_offset, typeinfo->m_align);

                    const_cast<archtypes_map&>(_m_arch_typeinfo_mapping)[typeinfo->m_id] =
                        arch_type_info{ typeinfo, mem_offset };

                    mem_offset += typeinfo->m_size * _m_entity_count_per_chunk;
                }
            }
            ~ArchType()
            {
                arch_chunk* chunk = _m_chunks.pick_all();
                while (chunk)
                {
                    auto* next_chunk = chunk->last;
                    delete chunk;

                    chunk = next_chunk;
                }

                // 回收池中的 chunk 也需要释放（析构时不可能再有并发访问）
                while (_m_recycled_chunks != nullptr)
                {
                    arch_chunk* next_chunk = _m_recycled_chunks->last;
                    delete _m_recycled_chunks;
                    _m_recycled_chunks = next_chunk;
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
                lock_guard g1(_m_chunk_list_defragmentation_mx);

                // 1. 为碎片整理准备一个新的临时chunk队列，实际上就是清空整个chunk链表
                auto* alive_chunks = _m_chunks.pick_all();

                // 2. 转存chunks，然后从尾到头开始整理碎片
                list<arch_chunk*> chunks;
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
                        for (je_entity_id_in_chunk_t i = 0; i < get_entity_count_per_chunk(); ++i)
                        {
                            auto* moving_entity_meta = &pick_chunk_metas[i];

                            // 如果当前位置并不空闲，且实体状态一切OK，则需要开始转移
                            if (free_places.find(i) == free_places.end())
                            {
                                if (moving_entity_meta->m_stat == JE_ENTITY_STAT_READY)
                                {
                                    // OK, 继续搬运，这搬运组件多是一件美事啊
                                    je_entity_id_in_chunk_t new_entity_id;
                                    je_version_t new_entity_version;

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
                                                arch_typeinfo.second.m_typeinfo->m_size,
                                                arch_typeinfo.second.m_begin_offset_in_chunk);
                                            void* new_component_addr = target_chunk->get_component_addr(new_entity_id,
                                                arch_typeinfo.second.m_typeinfo->m_size,
                                                arch_typeinfo.second.m_begin_offset_in_chunk);

                                            typing::move(arch_typeinfo.second.m_typeinfo, new_component_addr, moving_component_addr);
                                            typing::destruct(arch_typeinfo.second.m_typeinfo, moving_component_addr);
                                        }

                                        // 直接在这里激活新的实体
                                        assert(moving_entity_meta->m_stat != JE_ENTITY_STAT_UNAVAILABLE);
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

                        // 如果pick_chunk中的实体都是空闲的，那么我们就可以回收这个chunk了
                        if (stubborn_count == 0)
                        {
                            assert(!gap_filling_completed);

                            _m_free_count -= _m_entity_count_per_chunk;
                            _m_total_count -= _m_entity_count_per_chunk;

                            // 不能直接 delete：跨帧持有的旧句柄仍会解引用此
                            // chunk 的元数据。放入回收池保持内存有效，待复用。
                            {
                                lock_guard g2(_m_recycled_chunks_mx);
                                pick_chunk->last = _m_recycled_chunks;
                                _m_recycled_chunks = pick_chunk;
                            }
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
                //      被删空的chunk并不保证构成链表的连续后缀：更靠尾的一侧可能留有含预设体等
                //      无法搬运实体的幸存chunk，因此只能逐个跳过nullptr，不能提前break——否则
                //      幸存chunk会脱链（泄漏且从查询中消失），其空闲计数也无法归还（alloc将abort）。
                for (auto* chunk : chunks)
                {
                    if (chunk == nullptr)
                        continue;

                    _m_chunks.add_one(chunk);
                }
            }

            void alloc_entity(
                arch_chunk** out_chunk,
                je_entity_id_in_chunk_t* out_eid,
                je_version_t* out_eversion) noexcept
            {
                shared_lock sg1(_m_chunk_list_defragmentation_mx);
                while (true)
                {
                    je_entity_id_in_chunk_t free_entity_count = _m_free_count.load();
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
                    // OK, 没有空位了，优先复用回收池中搬空的chunk，否则创建新的chunk
                    //
                    // NOTE: 实际上如果有并行执行的添加实体操作，此处可能创建"不必要"的实体区块
                    //      不过问题不大，因为我们有碎片整理机制。
                    arch_chunk* recycled_chunk = nullptr;
                    {
                        lock_guard g2(_m_recycled_chunks_mx);
                        if (_m_recycled_chunks != nullptr)
                        {
                            recycled_chunk = _m_recycled_chunks;
                            _m_recycled_chunks = recycled_chunk->last;
                        }
                    }
                    arch_chunk* new_chunk =
                        recycled_chunk != nullptr ? recycled_chunk : new arch_chunk(this);
                    _m_chunks.add_one(new_chunk);

                    _m_total_count += _m_entity_count_per_chunk;
                    _m_free_count += _m_entity_count_per_chunk;
                }
            }
            entity instance_entity(/* OPTIONAL */ const entity* prefab) noexcept
            {
                arch_chunk* chunk;
                je_entity_id_in_chunk_t entity_id;
                je_version_t entity_version;

                alloc_entity(&chunk, &entity_id, &entity_version);
                for (auto& arch_typeinfo : _m_arch_typeinfo_mapping)
                {
                    void* component_addr = chunk->get_component_addr(entity_id,
                        arch_typeinfo.second.m_typeinfo->m_size,
                        arch_typeinfo.second.m_begin_offset_in_chunk);

                    if (prefab == nullptr)
                    {
                        typing::construct(
                            arch_typeinfo.second.m_typeinfo,
                            component_addr,
                            nullptr);
                    }
                    else
                    {
                        typing::copy(
                            arch_typeinfo.second.m_typeinfo,
                            component_addr,
                            prefab->get_component(
                                arch_typeinfo.second.m_typeinfo->m_id));
                    }
                }

                return entity{ chunk, entity_id, entity_version };
            }
            inline ArchManager* get_arch_mgr() const noexcept
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
            inline const arch_type_info* get_arch_type_info_by_type_id(je_type_id_t tid) const
            {
                auto fnd = _m_arch_typeinfo_mapping.find(tid);
                if (fnd != _m_arch_typeinfo_mapping.end())
                    return &fnd->second;
                return nullptr;
            }
            inline void close_all_entity(je_game_world_t* by_world)
            {
                auto* chunk = get_head_chunk();
                while (chunk)
                {
                    chunk->close_all_entity(by_world);

                    chunk = chunk->last;
                }
            }
            inline const TypesSet& get_types() const noexcept
            {
                return _m_types_set;
            }
            inline const types_list& get_type_infos() const noexcept
            {
                return _m_arch_typeinfo;
            }

            inline void create_chunk_info(
                je_collected_requirements_t* depend,
                je_dependence_arch_infos_t* out_arch_info) const noexcept
            {
                out_arch_info->m_arch = const_cast<ArchType*>(this);
                out_arch_info->m_entity_count = get_entity_count_per_chunk();

                auto& arch_cache = depend->m_selected_arch_cache.emplace(
                    this, depend->m_view_required.size()).first->second;

                size_t i = 0;
                for (const auto& view_requirement : depend->m_view_required)
                {
                    auto* arch_typeinfo = get_arch_type_info_by_type_id(view_requirement.m_typeid);
                    if (arch_typeinfo != nullptr)
                    {
                        arch_cache.m_view_component_offset[i] = arch_typeinfo->m_begin_offset_in_chunk;
                        arch_cache.m_view_component_size[i] = arch_typeinfo->m_typeinfo->m_size;
                    }
                    else
                    {
                        assert(
                            view_requirement.m_kind == JE_COMPONENT_REQUIRE_MAYNOT
                            || view_requirement.m_kind == JE_COMPONENT_REQUIRE_EXCEPT
                            || view_requirement.m_kind >= JE_COMPONENT_REQUIRE_ANYOF_0);

                        arch_cache.m_view_component_offset[i] = 0;
                        arch_cache.m_view_component_size[i] = 0;
                    }
                    ++i;
                }

                out_arch_info->m_view_component_offset = arch_cache.m_view_component_offset;
                out_arch_info->m_view_component_size = arch_cache.m_view_component_size;
            }
        };
        class ArchManager
        {
            JECS_DISABLE_MOVE_AND_COPY(ArchManager);

            World* _m_world;
            map<TypesSet, ArchType*> _m_arch_types_mapping;

            mutable shared_mutex _m_arch_types_mapping_mx;
            atomic_flag _m_arch_modified;

        public:
            ArchManager(World* world)
                : _m_world(world)
                , _m_arch_modified()
            {
            }
            ~ArchManager()
            {
                for (auto& [types, archtype] : _m_arch_types_mapping)
                    delete archtype;
            }

            ArchType* find_or_add_arch(const TypesSet& _types) noexcept
            {
                do
                {
                    shared_lock sg1(_m_arch_types_mapping_mx);
                    auto fnd = _m_arch_types_mapping.find(_types);
                    if (fnd != _m_arch_types_mapping.end())
                    {
                        return fnd->second;
                    }
                } while (0);

                lock_guard g1(_m_arch_types_mapping_mx);
                ArchType*& atype = _m_arch_types_mapping[_types];
                if (nullptr == atype)
                    atype = new ArchType(this, _types);
                _m_arch_modified.clear();
                return atype;
            }
            ArchType::entity create_an_entity_with_component(const TypesSet& _types) noexcept
            {
                return find_or_add_arch(_types)->instance_entity(nullptr);
            }

        public:
            // Only invoke by update..
            inline bool _arch_modified() noexcept
            {
                return !_m_arch_modified.test_and_set();
            }
            inline void update_collection_archinfo(je_requirement_collection_t* collection) const noexcept
            {
                static auto contains = [](const TypesSet& a, const TypesSet& b)
                    {
                        // b ⊆ a（双方均为排序序列）
                        return includes(a.begin(), a.end(), b.begin(), b.end());
                    };
                static auto contain_any = [](const TypesSet& a, const TypesSet& b)
                    {
                        for (auto type_id : b)
                            if (types_set::contains(a, type_id))
                                return true;
                        return b.empty();
                    };
                static auto contain_all_any = [](
                    const TypesSet& a, const map<int, TypesSet>& b)
                    {
                        for (auto& [_, type_id_set] : b)
                            if (contain_any(a, type_id_set) == false)
                                return false;
                        return true;
                    };
                static auto except = [](const TypesSet& a, const TypesSet& b)
                    {
                        for (auto type_id : b)
                            if (types_set::contains(a, type_id))
                                return false;
                        return true;
                    };

                auto* const collected_requirement = collection->m_collected_requirement;
                do
                {
                    shared_lock sg1(_m_arch_types_mapping_mx);
                    for (auto& [typeset, arch] : _m_arch_types_mapping)
                    {
                        if (collected_requirement->m_selected_arch_cache.contains(arch))
                            // Has been founded.
                            continue;

                        if (contains(typeset, collected_requirement->m_contain_set)
                            && except(typeset, collected_requirement->m_except_set)
                            && contain_all_any(typeset, collected_requirement->m_anyof_sets))
                        {
                            // Current arch is matched!
                            arch->create_chunk_info(
                                collected_requirement,
                                &collected_requirement->m_selected_archs.emplace_back());
                        }
                    }
                } while (0);

                collection->m_cached_archs =
                    collected_requirement->m_selected_archs.data();
                collection->m_cached_arch_count =
                    collected_requirement->m_selected_archs.size();
            }
            inline void close_all_entity(je_game_world_t* by_world)
            {
                shared_lock sg1(_m_arch_types_mapping_mx);
                for (auto& [types, archtype] : _m_arch_types_mapping)
                {
                    archtype->close_all_entity(by_world);
                }
            }
            inline vector<ArchType*> _get_all_arch_types() const noexcept
            {
                // THIS FUNCTION ONLY FOR EDITOR
                vector<ArchType*> arch_list;
                shared_lock sg1(_m_arch_types_mapping_mx);
                for (auto& pair : _m_arch_types_mapping)
                    arch_list.push_back(pair.second);

                return arch_list;
            }
            inline World* get_world() const noexcept
            {
                return _m_world;
            }

        private:
            friend class CommandBuffer;
            inline void command_defragmentation() noexcept
            {
                shared_lock sg1(_m_arch_types_mapping_mx);
                for (auto& [types, archtype] : _m_arch_types_mapping)
                    archtype->command_defragmentation();
            }
        };

        class Universe;

        struct Job
        {
            JECS_DISABLE_MOVE_AND_COPY(Job);

            enum kind
            {
                CALL_ONCE,
                FOR_WORLDS,
            };
            kind m_job_type;

            union
            {
                je_job_for_worlds_t m_for_worlds_job;
                je_job_call_once_t m_call_once_job;
            };

            Universe* m_universe;
            void* m_custom_data;
            void (*m_free_function)(void*);

            Job(Universe* universe, je_job_for_worlds_t _job, void* custom_data, void (*free_function)(void*))
                : m_job_type(kind::FOR_WORLDS)
                , m_for_worlds_job(_job)
                , m_universe(universe)
                , m_custom_data(custom_data)
                , m_free_function(free_function)
            {
                assert(_job != nullptr);
            }
            Job(Universe* universe, je_job_call_once_t _job, void* custom_data, void (*free_function)(void*))
                : m_job_type(kind::CALL_ONCE)
                , m_call_once_job(_job)
                , m_universe(universe)
                , m_custom_data(custom_data)
                , m_free_function(free_function)
            {
                assert(_job != nullptr);
            }
            ~Job()
            {
                if (m_free_function != nullptr)
                    m_free_function(m_custom_data);
            }
        };
        class CommandBuffer
        {
            // CommandBuffer used to store operations happend in a entity.
            JECS_DISABLE_MOVE_AND_COPY(CommandBuffer);

            struct _entity_command_buffer
            {
                JECS_DISABLE_MOVE_AND_COPY(_entity_command_buffer);

                struct typed_component
                {
                    je_type_id_t m_type_id;
                    void* m_component_addr; // if m_add_system_instance == nullptr, remove spcify comp

                    typed_component* last;

                    typed_component(je_type_id_t tid, void* addr)
                        : m_type_id(tid), m_component_addr(addr)
                    {
                        // Do nothing else
                    }
                };

                bool m_entity_removed_flag;
                je_entity_stat_t m_entity_active_stat;
                basic::atomic_list<typed_component> m_adding_or_removing_components;

                _entity_command_buffer() = default;
            };
            struct _world_command_buffer
            {
                JECS_DISABLE_MOVE_AND_COPY(_world_command_buffer);

                struct typed_system
                {
                    const je_type_info_t* m_typeinfo;
                    je_system_t m_add_system_instance; // if m_add_system_instance == nullptr, remove spcify sys

                    typed_system* last;

                    typed_system(const je_type_info_t* tinfo, je_system_t addr)
                        : m_typeinfo(tinfo)
                        , m_add_system_instance(addr)
                    {
                        // Do nothing else
                    }
                };
                struct append_slice_query_cache
                {
                    // NOTE: 当尝试添加一个切片查询缓存时，系统可能已经被移除了；这种情况下就不添加了
                    game_system* m_system_instance_may_not_exist;

                    je_type_hash_t m_query_slice_typehash;
                    unique_ptr<HoldedRequirementCollection> m_created_collection;

                    append_slice_query_cache* last;
                };

                bool m_destroy_world;

                // Update configs ?
                optional<bool> m_update_enabled;

                basic::atomic_list<typed_system> m_adding_or_removing_systems;
                basic::atomic_list<append_slice_query_cache> m_adding_query_caches;

                _world_command_buffer() = default;
            };

            shared_mutex _m_command_buffer_mx;
            shared_mutex _m_command_executer_guard_mx;

            using entity_command_buffer_map_t =
                map<ArchType::entity, _entity_command_buffer*>;

            World* _m_world;
            _world_command_buffer* _m_world_command_buffer;
            entity_command_buffer_map_t _m_entity_command_buffers;

            _entity_command_buffer* _find_or_create_buffer_for_entity(const ArchType::entity& e)
            {
                do
                {
                    shared_lock sg1(_m_command_buffer_mx);
                    auto fnd = _m_entity_command_buffers.find(e);
                    if (fnd != _m_entity_command_buffers.end())
                        return fnd->second;

                } while (0);

                lock_guard g1(_m_command_buffer_mx);
                auto fnd = _m_entity_command_buffers.find(e);
                if (fnd != _m_entity_command_buffers.end())
                    return fnd->second;

                auto* ebuf = new _entity_command_buffer{};
                ebuf->m_entity_removed_flag = false;
                ebuf->m_entity_active_stat = JE_ENTITY_STAT_UNAVAILABLE;

                auto result = _m_entity_command_buffers.emplace(e, ebuf).second;
                (void)result;
                assert(result);

                return ebuf;
            }
            _world_command_buffer* _find_or_create_buffer_for_world()
            {
                do
                {
                    shared_lock sg1(_m_command_buffer_mx);
                    if (_m_world_command_buffer != nullptr)
                        return _m_world_command_buffer;

                } while (0);

                lock_guard g1(_m_command_buffer_mx);
                if (_m_world_command_buffer != nullptr)
                    return _m_world_command_buffer;

                auto* wbuf = new _world_command_buffer{};

                wbuf->m_destroy_world = false;
                wbuf->m_update_enabled = nullopt;

                return _m_world_command_buffer = wbuf;
            }

        public:
            CommandBuffer(World* world)
                : _m_world(world), _m_world_command_buffer(nullptr)
            {
            }
            ~CommandBuffer()
            {
                assert(_m_entity_command_buffers.empty() && _m_world_command_buffer == nullptr);
            }

            void init_new_entity(const ArchType::entity& e, je_entity_stat_t stat)
            {
                shared_lock sl(_m_command_executer_guard_mx);
                assert(stat != JE_ENTITY_STAT_UNAVAILABLE);
                _find_or_create_buffer_for_entity(e)->m_entity_active_stat = stat;
            }

            void remove_entity(const ArchType::entity& e)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                _find_or_create_buffer_for_entity(e)->m_entity_removed_flag = true;
            }

            void* append_component(const ArchType::entity& e, je_type_id_t component_type)
            {
                const auto* tinfo = typing::of(component_type);

                // Instance component
                void* created_component = malloc(tinfo->m_size);
                typing::construct(tinfo, created_component, nullptr);

                shared_lock sl(_m_command_executer_guard_mx);

                _find_or_create_buffer_for_entity(e)->m_adding_or_removing_components.add_one(
                    new _entity_command_buffer::typed_component(component_type, created_component));

                return created_component;
            }
            void remove_component(const ArchType::entity& e, je_type_id_t component_type)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                _find_or_create_buffer_for_entity(e)->m_adding_or_removing_components.add_one(
                    new _entity_command_buffer::typed_component(component_type, nullptr));
            }
            void close_world()
            {
                shared_lock sl(_m_command_executer_guard_mx);

                DEBUG_ARCH_LOG("World: %p the destroy world operation has been committed to the command buffer.",
                    _m_world);

                _find_or_create_buffer_for_world()->m_destroy_world = true;
            }

            void set_enable_world(bool enable)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                if (enable)
                    DEBUG_ARCH_LOG("World: %p trying to enable world.",
                        _m_world);
                else
                    DEBUG_ARCH_LOG("World: %p trying to disable world.",
                        _m_world);

                _find_or_create_buffer_for_world()->m_update_enabled = optional(enable);
            }

            void add_system_instance(const je_type_info_t* type, je_system_t sys_instance)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                DEBUG_ARCH_LOG("World: %p want to add system(%p) named '%s', operation has been committed.",
                    _m_world, sys_instance, type->m_typename);

                assert(sys_instance);

                _find_or_create_buffer_for_world()->m_adding_or_removing_systems.add_one(
                    new _world_command_buffer::typed_system(type, sys_instance));
            }
            void remove_system_instance(const je_type_info_t* type)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                DEBUG_ARCH_LOG("World: %p want to remove system named '%s', operation has been committed.",
                    _m_world, type->m_typename);

                _find_or_create_buffer_for_world()->m_adding_or_removing_systems.add_one(
                    new _world_command_buffer::typed_system(type, nullptr));
            }
            void add_system_query_cache(
                game_system* system_instance,
                je_type_hash_t hash,
                unique_ptr<HoldedRequirementCollection>&& collection)
            {
                shared_lock sl(_m_command_executer_guard_mx);

                _find_or_create_buffer_for_world()->m_adding_query_caches.add_one(
                    new _world_command_buffer::append_slice_query_cache{
                        system_instance,
                        hash,
                        std::move(collection),
                    });
            }

        public:
            void update();
        };
        class World : public je_game_world_t
        {
            JECS_DISABLE_MOVE_AND_COPY(World);

        public:
            using system_container_t =
                unordered_map<const je_type_info_t*, je_system_t>;
            using slice_cache_container_t =
                unordered_map<je_type_hash_t, unique_ptr<HoldedRequirementCollection>>;

            // NOTE: 此处之所以要根据不同系统实例缓存不同的切片，是考虑到编译防火墙，不同编译器/库对
            //      相同/不同的切片类型哈希可能不同/相同；为了规避因此导致的哈希冲突或者重复，针对不同
            //      实例缓存，这样确保一个实例对应的始终是相同的模块（库），避免编译防火墙问题。
            using system_slice_cache_container_t =
                unordered_map<je_system_t, slice_cache_container_t>;
        private:
            Universe* _m_universe;

            CommandBuffer _m_command_buffer;
            ArchManager _m_arch_manager;

            // Monotonically increasing counter bumped in update() whenever the arch-type set
            // changed. Unlike _m_arch_modified (which is test-and-set / consumed by update()),
            // this counter is never consumed, so external observers (e.g. persistent
            // RequirementCollection caches) can reliably detect that a refresh is needed.
            uint64_t _m_arch_change_counter;

            bool _m_world_enabled;
            bool _m_destroying_flag;

            system_container_t m_systems;
            system_slice_cache_container_t m_system_slice_caches;
        private:
            inline static shared_mutex _m_alive_worlds_mx;
            inline static unordered_set<World*> _m_alive_worlds;

        public:
            World(Universe* universe)
                : _m_universe(universe)
                , _m_command_buffer(this)
                , _m_arch_manager(this)
                , _m_arch_change_counter(100)
                , _m_world_enabled(false)
                , _m_destroying_flag(false)
            {
                lock_guard g1(_m_alive_worlds_mx);
                _m_alive_worlds.emplace(this);
            }
            ~World()
            {
                // Must be destroyed before destruct.
                assert(_m_destroying_flag);

                lock_guard g1(_m_alive_worlds_mx);
                _m_alive_worlds.erase(this);
            }
            system_container_t& get_system_instances() noexcept
            {
                return m_systems;
            }
            void _destroy_system_instance(
                const je_type_info_t* type, je_system_t sys) noexcept
            {
                if (_m_world_enabled)
                    type->m_system_updaters->m_on_disable(sys);

                typing::destruct(type, sys);
                free(sys);
            }
            void execute_world_job(Job* job) noexcept
            {
                assert(job->m_job_type == Job::kind::FOR_WORLDS);
                if (_m_world_enabled)
                    job->m_for_worlds_job(this, job->m_custom_data);
            }
            void append_system_instance(const je_type_info_t* type, je_system_t sys) noexcept
            {
                auto fnd = m_systems.find(type);
                if (fnd == m_systems.end())
                {
                    if (_m_world_enabled)
                        type->m_system_updaters->m_on_enable(sys);

                    auto result = m_systems.emplace(type, sys).second;
                    (void)result;
                    assert(result);

                    // Create system slice cache for this system
                    result = m_system_slice_caches.emplace(
                        sys, slice_cache_container_t{}).second;
                    (void)result;
                    assert(result);
                }
                else
                {
#ifndef NDEBUG
                    debug::logwarn(
                        "Trying to append system: '%s', but current world(%p) has already contain same one(%p), replace it with %p",
                        type->m_typename,
                        this, fnd->second,
                        sys);
#endif
                    remove_system_instance(type);
                    append_system_instance(type, sys);
                }
            }
            void remove_system_instance(const je_type_info_t* type) noexcept
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
                    debug::logwarn(
                        "Trying to remove system: '%s', but current world(%p) donot have this system.",
                        type->m_typename,
                        this);
                }
#endif
            }

            static void _drop_collection_instance(
                je_requirement_collection_t* collection)
            {
                delete collection->m_collected_requirement;
                delete collection;
            }

            void append_slice_cache_for_system_instance(
                game_system* sys,
                je_type_hash_t hash,
                unique_ptr<HoldedRequirementCollection>&& collection)
            {
                auto fnd = m_system_slice_caches.find(sys);
                if (fnd != m_system_slice_caches.end())
                {
                    auto& slice_caches = fnd->second;

                    if (slice_caches.emplace(hash, std::move(collection)).second)
                        return;

                    // Else, already exist, do nothing.
                }
                // Else, system instance not exist, do nothing.
#ifndef NDEBUG
                else
                {
                    debug::logwarn(
                        "Trying to add slice cache for system instance: %p, but current world(%p) donot have this system instance.",
                        sys,
                        this);
                }
#endif
            }

            ArchManager& _get_arch_mgr() noexcept
            {
                // NOTE: This function used for editor
                return _m_arch_manager;
            }

            const ArchManager& _get_arch_mgr() const noexcept
            {
                // NOTE: This function used for editor
                return _m_arch_manager;
            }

            // Non-consuming peek of the arch-type change counter. Two observations of this
            // value being equal guarantees no new arch-type was created in between.
            inline uint64_t get_arch_change_version() const noexcept
            {
                return _m_arch_change_counter;
            }

        public:
            void update_collection_archinfo(je_requirement_collection_t* collection) const noexcept
            {
                _get_arch_mgr().update_collection_archinfo(collection);
            }

            bool update()
            {
                // Complete command buffer:
                _m_command_buffer.update();

                if (_m_destroying_flag)
                {
                    // Remove all system from world.
                    vector<const je_type_info_t*> _removing_sys_types;
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
                    // Arch types modified, all collection arch info need to be updated.
                    ++_m_arch_change_counter;
                    for (auto& [system_instance, slice_cache] : m_system_slice_caches)
                    {
                        (void)system_instance;
                        for (auto& [hash, collection] : slice_cache)
                        {
                            (void)hash;
                            je_ecs_world_collection_update(this, collection.get());
                        }
                    }
                }
                return !_m_destroying_flag;
            }

            inline ArchType::entity create_entity_with_component(const TypesSet& types, je_entity_stat_t stat)
            {
                auto entity = _m_arch_manager.create_an_entity_with_component(types);
                _m_command_buffer.init_new_entity(entity, stat);
                return entity;
            }
            inline ArchType::entity create_entity_with_prefab(const ArchType::entity* prefab)
            {
                if (!prefab->is_valid())
                {
                    debug::logerr("It's not allowed to create entity with invalid prefab entity, very dangerous.");
                    return {};
                }

                auto entity = prefab->chunk()->get_arch_type()->instance_entity(prefab);
                _m_command_buffer.init_new_entity(entity, JE_ENTITY_STAT_READY);
                return entity;
            }
            inline je_system_t request_to_append_system(const je_type_info_t* type)
            {
                if (type->m_type_class != JE_SYSTEM)
                    return nullptr;

                je_system_t sys = static_cast<je_system_t>(malloc(type->m_size));
                typing::construct(type, sys, this);
                get_command_buffer().add_system_instance(type, sys);
                return sys;
            }
            inline void request_to_remove_system(const je_type_info_t* type)
            {
                get_command_buffer().remove_system_instance(type);
            }
            inline void request_to_set_enable(bool enable) noexcept
            {
                get_command_buffer().set_enable_world(enable);
            }
            inline bool fetch_and_request_slice_cache_dependence(
                game_system* system_instance,
                je_type_hash_t hash,
                je_requirement_collection_t** out_collection)
            {
                // NOTE: `m_system_slice_caches` 只进行读操作，以确保安全。不允许在命令缓冲区处理期间
                //      执行 `fetch_and_request_slice_cache_dependence`

                auto fnd = m_system_slice_caches.find(system_instance);
                if (fnd != m_system_slice_caches.end())
                {
                    auto fnd2 = fnd->second.find(hash);
                    if (fnd2 != fnd->second.end())
                    {
                        *out_collection = fnd2->second.get();
                        return true;
                    }
                }

                unique_ptr<HoldedRequirementCollection> created_collection =
                    make_unique<HoldedRequirementCollection>();

                *out_collection = created_collection.get();

                // NOTE: dependence 的需求将在外部初始化，更新也需要由外部执行
                get_command_buffer().add_system_query_cache(
                    system_instance,
                    hash,
                    std::move(created_collection));

                return false;
            }
            inline CommandBuffer& get_command_buffer() noexcept
            {
                return _m_command_buffer;
            }
            inline bool is_enabled() const noexcept
            {
                return _m_world_enabled;
            }
            inline bool _is_destroying() const noexcept
            {
                return _m_destroying_flag;
            }
            inline void _ready_to_destroy() noexcept
            {
                _m_destroying_flag = true;
            }
            inline void _set_enable_world(bool enable) noexcept
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
            inline Universe* get_universe() const noexcept
            {
                return _m_universe;
            }
        };

        void CommandBuffer::update()
        {
            // 太可怕了，这个函数实在是太可怕了……
            // 丑陋而混乱的实现！简直是灾难的起点！
            lock_guard g1(_m_command_executer_guard_mx);

            // Update all operate in this buffer
            ::parallel_foreach(
                _m_entity_command_buffers.begin(), _m_entity_command_buffers.end(),
                [](pair<const ArchType::entity, _entity_command_buffer*>& _buf_in_entity)
                {
                    //  If entity not valid, skip component append&remove&active, but need
                    // free temp components.
                    ArchType::entity current_entity = _buf_in_entity.first;

                    auto* modify_typed_components = _buf_in_entity.second->m_adding_or_removing_components.pick_all();
                    unordered_map<je_type_id_t, void*> modifying_component_type_and_instances;

                    while (modify_typed_components != nullptr)
                    {
                        auto* current_modify_typed_components = modify_typed_components;
                        modify_typed_components = modify_typed_components->last;

                        if (!modifying_component_type_and_instances.emplace(
                            current_modify_typed_components->m_type_id,
                            current_modify_typed_components->m_component_addr)
                            .second)
                        {
                            // Operation failed, there is a new operation has overwrite this operation.
                            if (current_modify_typed_components->m_component_addr != nullptr)
                            {
                                // If trying to append component instance, this request has been replaced, free it.
                                typing::destruct(
                                    typing::of(current_modify_typed_components->m_type_id),
                                    current_modify_typed_components->m_component_addr);

                                free(current_modify_typed_components->m_component_addr);
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
                                    typing::destruct(typing::of(tid), instance);
                                    free(instance);
                                }
                            }

                            // Remove all component
                            for (je_type_id_t type_id : *current_entity.chunk()->types())
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
                            if (_buf_in_entity.second->m_entity_active_stat != JE_ENTITY_STAT_UNAVAILABLE)
                                current_entity.chunk()->command_active_entity(current_entity._m_id, _buf_in_entity.second->m_entity_active_stat);

                            if (current_entity.chunk()->get_entity_meta()[current_entity._m_id].m_stat == JE_ENTITY_STAT_PREFAB)
                            {
                                // Remove all new component;
                                for (auto& [tid, instance] : modifying_component_type_and_instances)
                                {
                                    if (instance != nullptr)
                                    {
                                        const auto* tinfo = typing::of(tid);

                                        debug::logerr("Cannot add component: '%s' to prefab(%p:%zuv%zu).",
                                            tinfo->m_typename,
                                            current_entity._m_in_chunk,
                                            (size_t)current_entity._m_id,
                                            (size_t)current_entity._m_version);

                                        typing::destruct(tinfo, instance);
                                        free(instance);
                                    }
                                    else
                                    {
                                        debug::logerr("Cannot remove component: '%s' from prefab(%p:%zuv%zu).",
                                            typing::of(tid)->m_typename,
                                            current_entity._m_in_chunk,
                                            (size_t)current_entity._m_id,
                                            (size_t)current_entity._m_version);
                                    }
                                }
                            }
                            else
                            {
                                TypesSet new_chunk_types = *current_entity.chunk()->types();

                                // 2. Apply modify!
                                unordered_map<je_type_id_t, void*> append_component_type_addr_map;
                                for (auto& [tid, instance] : modifying_component_type_and_instances)
                                {
                                    if (instance == nullptr)
                                    {
                                        // Trying to remove.
                                        if (types_set::erase(new_chunk_types, tid))
                                        {
                                            current_entity.chunk()->destruct_component_addr_with_typeid(
                                                current_entity._m_id,
                                                tid);
                                        }
#ifndef NDEBUG
                                        else
                                            debug::logwarn(
                                                "Trying to remove '%s' from entity '%p:%uv%u', but the specify entity donot contain this component, skip.",
                                                typing::of(tid)->m_typename,
                                                current_entity._m_in_chunk, current_entity._m_id, current_entity._m_version);
#endif
                                    }
                                    else
                                    {
                                        append_component_type_addr_map[tid] = instance;

                                        // Trying to append.
                                        if (!types_set::contains(new_chunk_types, tid))
                                            types_set::insert(new_chunk_types, tid);
                                        else
                                        {
#ifndef NDEBUG
                                            debug::logwarn(
                                                "Trying to append '%s' to entity '%p:%uv%u', but the specify entity already contained this component, replace it.",
                                                typing::of(tid)->m_typename,
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
                                auto* const current_arch_type = current_entity.chunk()->get_arch_type();
                                auto* const new_arch_type_my_null =
                                    current_arch_type->get_arch_mgr()->find_or_add_arch(
                                        new_chunk_types);

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
                                        free(instance);
                                    }
                                }
                                else
                                {
                                    if (new_arch_type_my_null != nullptr)
                                    {
                                        ArchType::arch_chunk* chunk;
                                        je_entity_id_in_chunk_t entity_id;
                                        je_version_t entity_version;

                                        new_arch_type_my_null->alloc_entity(&chunk, &entity_id, &entity_version);
                                        // Entity alloced, move component to here..

                                        for (je_type_id_t type_id : new_chunk_types)
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
                                                free(fnd->second);
                                            }
                                        }

                                        // Active new one
                                        assert(current_entity.chunk()->get_entity_meta()[current_entity._m_id].m_stat
                                            == JE_ENTITY_STAT_READY);
                                        chunk->command_active_entity(entity_id, JE_ENTITY_STAT_READY);
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
                                typing::destruct(typing::of(tid), instance);
                                free(instance);
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
                    _m_world->_set_enable_world(_m_world_command_buffer->m_update_enabled.value());
                    _m_world_command_buffer->m_update_enabled = nullopt;
                }

                if (_m_world->is_enabled() || _m_world->_is_destroying())
                {
                    unordered_map<const je_type_info_t*, je_system_t> modifying_system_type_and_instances;

                    auto* append_or_remove_system = _m_world_command_buffer->m_adding_or_removing_systems.pick_all();
                    while (append_or_remove_system)
                    {
                        auto* cur_append_or_remove_system = append_or_remove_system;
                        append_or_remove_system = append_or_remove_system->last;

                        if (!modifying_system_type_and_instances.emplace(
                            cur_append_or_remove_system->m_typeinfo,
                            cur_append_or_remove_system->m_add_system_instance).second)

                        {
                            if (cur_append_or_remove_system->m_add_system_instance != nullptr)
                            {
                                typing::destruct(cur_append_or_remove_system->m_typeinfo, cur_append_or_remove_system->m_add_system_instance);
                                free(cur_append_or_remove_system->m_add_system_instance);
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
                            std::move(cur_append_query_caches->m_created_collection));

                        delete cur_append_query_caches;
                    }

                    delete _m_world_command_buffer;
                    _m_world_command_buffer = nullptr;
                }
            }
        }

        // Universe
        class Universe : public je_game_universe_t
        {
            inline static mutex _m_alive_universes_mx;
            inline static list<Universe*> _m_alive_universes;

            mutex _m_removing_worlds_appending_mx;
            thread _m_universe_update_thread;

            vector<World*> _m_world_list;

            // Used for store shared jobs instance.
            vector<Job*> _m_shared_pre_jobs;
            vector<Job*> _m_shared_jobs;
            vector<Job*> _m_shared_after_jobs;

            mutex _m_next_execute_interval_mx;

            // 以下字段需要初始化
            atomic_bool _m_universe_update_thread_stop_flag;
            atomic_size_t _m_universe_life;

            double _m_frame_current_time;
            double _m_real_current_time;

            // 期待的universe同步间隔
            // 由于图形计算等开销，实际上的同步间隔可能大于此值
            double _m_frame_deltatime;
            // Universe每次Update时与实际世界时间进行计算得到的值
            double _m_real_deltatime;
            // Universe每次Update时与实际世界时间进行计算得到的值，与过去若干帧的平均更新间隔
            double _m_smooth_deltatime;
            // 最大DeltaTime值，防止出现巨大的跳变
            double _m_max_deltatime;
            // 时间缩放
            double _m_time_scale;

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
                    World* m_adding_world;
                    je_job_call_once_t m_call_once_job;
                    je_job_for_worlds_t m_for_worlds_job;
                };
                universe_action* last;
            };

            basic::atomic_list<universe_action> _m_universe_actions;

            void append_universe_action(universe_action* act) noexcept
            {
                _m_universe_actions.add_one(act);
            }
            static void default_job_for_execute_sys_update_for_worlds_in_universe(je_game_world_t* _ecs_world, void* _)
            {
                World* cur_world = static_cast<World*>(_ecs_world);

                World::system_container_t& active_systems =
                    cur_world->get_system_instances();

                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_pre_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_state_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_transform_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_physics_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_late_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_commit_update(val.second);
                    });
                ::parallel_foreach(
                    active_systems.begin(), active_systems.end(),
                    [](World::system_container_t::value_type& val)
                    {
                        val.first->m_system_updaters->m_graphic_update(val.second);
                    });
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
                _m_time_scale = max(0., scale);
            }

            void update_universe_action_and_worlds() noexcept
            {
                vector<World*> _m_removing_worlds;

                // Update all worlds, if world is closing, add it to _m_removing_worlds.
                ::parallel_foreach(_m_world_list.begin(), _m_world_list.end(),
                    [this, &_m_removing_worlds](World* world)
                    {
                        if (!world->update())
                        {
                            lock_guard g1(_m_removing_worlds_appending_mx);

                            // Ready remove the world from list;
                            _m_removing_worlds.push_back(world);
                        }
                    });

                // Remove all closed worlds
                for (World* removed_world : _m_removing_worlds)
                {
                    auto fnd = find(_m_world_list.begin(), _m_world_list.end(), removed_world);
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
                        _m_shared_pre_jobs.push_back(new Job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;
                    case universe_action::action_type::ADD_PRE_JOB_CALL_ONCE:
                        _m_shared_pre_jobs.push_back(new Job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;
                    case universe_action::action_type::ADD_NORMAL_JOB_FOR_WORLDS:
                        _m_shared_jobs.push_back(new Job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;
                    case universe_action::action_type::ADD_NORMAL_JOB_CALL_ONCE:
                        _m_shared_jobs.push_back(new Job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;
                    case universe_action::action_type::ADD_AFTER_JOB_FOR_WORLDS:
                        _m_shared_after_jobs.push_back(new Job(this, cur_action->m_for_worlds_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;
                    case universe_action::action_type::ADD_AFTER_JOB_CALL_ONCE:
                        _m_shared_after_jobs.push_back(new Job(this, cur_action->m_call_once_job, cur_action->m_custom_data, cur_action->m_free_function));
                        break;

                    case universe_action::action_type::REMOVE_PRE_JOB_FOR_WORLDS:
                    {
                        auto fnd = find_if(_m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                            });

                        if (fnd == _m_shared_pre_jobs.end())
                            debug::logerr("Cannot find pre-job(%p) in universe(%p), remove failed.",
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
                        auto fnd = find_if(_m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
                            });

                        if (fnd == _m_shared_pre_jobs.end())
                            debug::logerr("Cannot find pre-callonce-job(%p) in universe(%p), remove failed.",
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
                        auto fnd = find_if(_m_shared_jobs.begin(), _m_shared_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                            });

                        if (fnd == _m_shared_jobs.end())
                            debug::logerr("Cannot find job(%p) in universe(%p), remove failed.",
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
                        auto fnd = find_if(_m_shared_jobs.begin(), _m_shared_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
                            });

                        if (fnd == _m_shared_jobs.end())
                            debug::logerr("Cannot find callonce-job(%p) in universe(%p), remove failed.",
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
                        auto fnd = find_if(_m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::FOR_WORLDS && _job->m_for_worlds_job == cur_action->m_for_worlds_job;
                            });

                        if (fnd == _m_shared_after_jobs.end())
                            debug::logerr("Cannot find after-job(%p) in universe(%p), remove failed.",
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
                        auto fnd = find_if(_m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                            [cur_action](Job* _job)
                            {
                                return _job->m_job_type == Job::kind::CALL_ONCE && _job->m_call_once_job == cur_action->m_call_once_job;
                            });

                        if (fnd == _m_shared_after_jobs.end())
                            debug::logerr("Cannot find after-callonce-job(%p) in universe(%p), remove failed.",
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
                _m_real_deltatime = math::clamp(
                    current_time - _m_real_current_time, 0., _m_max_deltatime);
                _m_smooth_deltatime = (_m_smooth_deltatime * 9. + _m_real_deltatime) / 10.;
                _m_real_current_time = current_time;

                // 一个逻辑帧从此处开始：
                // 1. 完成预先任务
                ::parallel_foreach(
                    _m_shared_pre_jobs.begin(), _m_shared_pre_jobs.end(),
                    [this](Job* shared_job)
                    {
                        if (shared_job->m_job_type == Job::kind::FOR_WORLDS)
                            ::parallel_foreach(_m_world_list.begin(), _m_world_list.end(),
                                [shared_job](World* world)
                                {
                                    world->execute_world_job(shared_job);
                                });
                        else
                            shared_job->m_call_once_job(shared_job->m_custom_data);
                    });

                // 2. 完成常规任务
                ::parallel_foreach(
                    _m_shared_jobs.begin(), _m_shared_jobs.end(),
                    [this](Job* shared_job)
                    {
                        if (shared_job->m_job_type == Job::kind::FOR_WORLDS)
                            ::parallel_foreach(_m_world_list.begin(), _m_world_list.end(),
                                [shared_job](World* world)
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
                ::parallel_foreach(
                    _m_shared_after_jobs.begin(), _m_shared_after_jobs.end(),
                    [this](Job* shared_job)
                    {
                        if (shared_job->m_job_type == Job::kind::FOR_WORLDS)
                            ::parallel_foreach(_m_world_list.begin(), _m_world_list.end(),
                                [shared_job](World* world)
                                {
                                    world->execute_world_job(shared_job);
                                });
                        else
                            shared_job->m_call_once_job(shared_job->m_custom_data);
                    });
            }

        public:
            Universe()
                : _m_universe_update_thread_stop_flag(false)
                , _m_universe_life(1)
                , _m_frame_current_time(0.)
                , _m_real_current_time(0.)
                , _m_frame_deltatime(1. / 60.)
                , _m_real_deltatime(1. / 60.)
                , _m_smooth_deltatime(1. / 60.)
                , _m_max_deltatime(0.2)
                , _m_time_scale(1.0)
                , _m_universe_exited(false)
            {
                lock_guard g1(_m_alive_universes_mx);
                _m_alive_universes.push_back(this);

                DEBUG_ARCH_LOG("Ready to create Universe: %p.", this);

                // Append default jobs for updating systems.
                je_ecs_universe_for_worlds_job_register(this,
                    Universe::default_job_for_execute_sys_update_for_worlds_in_universe, nullptr, nullptr);

                _m_universe_update_thread = thread(
                    [this]()
                    {
                        while (true)
                        {
                            update();

                            bool exit_flag = _m_universe_update_thread_stop_flag.load(
                                memory_order_relaxed);

                            if (exit_flag)
                            {
                                if (_m_world_list.empty())
                                    // If there is no world alive, and exit flag is setten, exit this thread.
                                    break;

                                // Only invoke in game thread!
                                for (auto* world : _m_world_list)
                                    je_ecs_world_destroy(world);
                            }
                        }

                        // Make sure universe action empty.
                        do
                        {
                            update();
                            assert(_m_universe_actions.peek() == nullptr);

                            lock_guard g1(_m_exit_callbacks_mx);
                            // 先置退出标记再排水：之后到达的注册会看到标记
                            // 而直接失败，不会把回调挂进一张不会再被消费的表
                            _m_universe_exited.store(true, memory_order_relaxed);

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
                        for (Job* pre_job : _m_shared_pre_jobs)
                            delete pre_job;
                        for (Job* job : _m_shared_jobs)
                            delete job;
                        for (Job* after_job : _m_shared_after_jobs)
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
                function<void(void)> m_method;
            };
            basic::atomic_list<callback_function_node> m_exit_callback_list;
            // 保护退出回调的注册与排水：更新线程退出时置位 _m_universe_exited
            // 并一次性消费回调表；此后再注册将失败（返回 false），调用方
            //（如 je_ecs_universe_loop 的 wait）不得继续等待，否则死锁。
            shared_mutex _m_exit_callbacks_mx;
            atomic_bool _m_universe_exited;

            inline bool register_exit_callback(const function<void(void)>& function) noexcept
            {
                lock_guard g1(_m_exit_callbacks_mx);
                if (_m_universe_exited.load(memory_order_relaxed))
                    return false;

                callback_function_node* node = new callback_function_node();
                node->m_method = function;
                m_exit_callback_list.add_one(node);
                return true;
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
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_PRE_JOB_CALL_ONCE;
                action->m_call_once_job = job;

                append_universe_action(action);
            }
            inline void unregister_pre_for_worlds_job(je_job_for_worlds_t job) noexcept
            {
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_PRE_JOB_FOR_WORLDS;
                action->m_for_worlds_job = job;

                append_universe_action(action);
            }
            inline void unregister_call_once_job(je_job_call_once_t job) noexcept
            {
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_CALL_ONCE;
                action->m_call_once_job = job;

                append_universe_action(action);
            }
            inline void unregister_for_worlds_job(je_job_for_worlds_t job) noexcept
            {
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_NORMAL_JOB_FOR_WORLDS;
                action->m_for_worlds_job = job;

                append_universe_action(action);
            }
            inline void unregister_after_call_once_job(je_job_call_once_t job) noexcept
            {
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_CALL_ONCE;
                action->m_call_once_job = job;

                append_universe_action(action);
            }
            inline void unregister_after_for_worlds_job(je_job_for_worlds_t job) noexcept
            {
                universe_action* action = new universe_action{};
                action->m_type = universe_action::action_type::REMOVE_AFTER_JOB_FOR_WORLDS;
                action->m_for_worlds_job = job;

                append_universe_action(action);
            }
            ~Universe()
            {
                do
                {
                    lock_guard g1(_m_alive_universes_mx);
                    auto fnd = find(_m_alive_universes.begin(), _m_alive_universes.end(), this);
                    assert(fnd != _m_alive_universes.end());
                    _m_alive_universes.erase(fnd);
                } while (0);

                DEBUG_ARCH_LOG("Universe: %p closing.", this);

                stop_universe_loop();
                if (_m_universe_update_thread.joinable())
                    _m_universe_update_thread.join();

                DEBUG_ARCH_LOG("Universe: %p closed.", this);
            }
        private:
            void stop_universe_loop() noexcept
            {
                _m_universe_update_thread_stop_flag.store(
                    true, memory_order_relaxed);
            }
        public:
            void grow_lifetime() noexcept
            {
                auto current_life = _m_universe_life.load();
                for (;;)
                {
                    if (current_life == 0)
                    {
                        debug::logfatal("Universe(%p) has been destroyed, cannot use it anymore.", this);
                        break;
                    }

                    if (_m_universe_life.compare_exchange_strong(current_life, current_life + 1))
                        break;
                }
            }
            void trim_lifetime() noexcept
            {
                auto current_life = _m_universe_life.load();
                for (;;)
                {
                    if (current_life == 0)
                    {
                        debug::logfatal("Universe(%p) has been destroyed, cannot use it anymore.", this);
                        break;
                    }
                    if (_m_universe_life.compare_exchange_strong(current_life, current_life - 1))
                    {
                        if (current_life - 1 == 0)
                            stop_universe_loop();
                        break;
                    }
                }
            }
            World* create_world()
            {
                DEBUG_ARCH_LOG("Universe: %p want to create a world.", this);

                World* new_world = new World(this);

                universe_action* add_world_action = new universe_action;
                add_world_action->m_type = universe_action::ADD_WORLD;
                add_world_action->m_adding_world = new_world;

                append_universe_action(add_world_action);

                return new_world;
            }
            vector<World*> _get_all_worlds()
            {
                // NOTE: This function is designed for editor
                return _m_world_list;
            }

        public:
            static void _shutdown_all_universe()
            {
                lock_guard g1(_m_alive_universes_mx);
                unordered_set<Universe*> shutdown_universes;
                for (;;)
                {
                    bool all_universe_has_shutdown = true;

                    for (auto* u : _m_alive_universes)
                    {
                        if (shutdown_universes.find(u) == shutdown_universes.end())
                        {
                            shutdown_universes.emplace(u);
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
    }
}

je_game_universe_t* je_ecs_universe_create()
{
    return new Universe();
}

void je_ecs_universe_exit_callback_register(je_game_universe_t* universe, void (*callback)(void*), /* OPTIONAL */ void* arg)
{
    static_cast<Universe*>(universe)->register_exit_callback(
        [callback, arg]()
        {
            callback(arg);
        });
}

void je_ecs_universe_loop(je_game_universe_t* universe_instance)
{
    condition_variable exit_cv;
    mutex exit_mx;
    atomic_flag exit_flag = {};
    exit_flag.test_and_set();

    const bool registered =
        static_cast<Universe*>(universe_instance)->register_exit_callback(
            [&]()
            {
                lock_guard g1(exit_mx);
                exit_flag.clear();
                exit_cv.notify_all();
            });

    if (!registered)
        // 更新线程已经退出（生命周期早已归零），没有会触发回调的一方，
        // 直接返回而不是永远等待
        return;

    do
    {
        unique_lock uq1(exit_mx);
        exit_cv.wait(uq1, [&]() -> bool
            { return !exit_flag.test_and_set(); });
    } while (0);
}

void je_ecs_universe_destroy(je_game_universe_t* universe_instance)
{
    delete static_cast<Universe*>(universe_instance);
}

void je_ecs_universe_lifetime_grow(je_game_universe_t* universe_instance)
{
    static_cast<Universe*>(universe_instance)->grow_lifetime();
}

void je_ecs_universe_lifetime_trim(je_game_universe_t* universe_instance)
{
    static_cast<Universe*>(universe_instance)->trim_lifetime();
}

je_chunk_t* je_arch_chunk_get(je_archtype_t* archtype)
{
    return static_cast<ArchType*>(archtype)->get_head_chunk();
}

/* OPTIONAL: If no successor chunk */ je_chunk_t* je_arch_chunk_next(je_chunk_t* chunk)
{
    return static_cast<ArchType::arch_chunk*>(chunk)->last;
}

je_game_world_t* je_ecs_world_create(je_game_universe_t* in_universe)
{
    return static_cast<Universe*>(in_universe)->create_world();
}

void je_ecs_world_destroy(je_game_world_t* world)
{
    static_cast<World*>(world)->get_command_buffer().close_world();
}

/* OPTIONAL */ je_system_t je_ecs_world_system_instance_add(je_game_world_t* world, je_type_id_t type)
{
    return static_cast<World*>(world)->request_to_append_system(typing::of(type));
}

/* OPTIONAL */ je_system_t je_ecs_world_system_instance_get(je_game_world_t* world, je_type_id_t type)
{
    auto& syss = static_cast<World*>(world)->get_system_instances();
    auto fnd = syss.find(typing::of(type));
    if (fnd == syss.end())
        return nullptr;
    return fnd->second;
}

void je_ecs_world_system_instance_remove(je_game_world_t* world, je_type_id_t type)
{
    static_cast<World*>(world)->request_to_remove_system(typing::of(type));
}

void je_ecs_world_enable_set(je_game_world_t* world, bool enable)
{
    static_cast<World*>(world)->request_to_set_enable(enable);
}

void je_ecs_world_entity_create_with_components(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_type_id_t* component_ids,
    size_t component_count)
{
    assert(component_count != 0);

    const TypesSet types =
        types_set::make(component_ids, component_count);

    auto&& entity = static_cast<World*>(world)
        ->create_entity_with_component(types, JE_ENTITY_STAT_READY);

    out_entity->_m_in_chunk = entity._m_in_chunk;
    out_entity->_m_id = entity._m_id;
    out_entity->_m_version = entity._m_version;
}
void je_ecs_world_prefab_create_with_components(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_type_id_t* component_ids,
    size_t component_count)
{
    assert(component_count != 0);

    const TypesSet types =
        types_set::make(component_ids, component_count);

    auto entity = static_cast<World*>(world)
        ->create_entity_with_component(types, JE_ENTITY_STAT_PREFAB);
    out_entity->_m_in_chunk = entity._m_in_chunk;
    out_entity->_m_id = entity._m_id;
    out_entity->_m_version = entity._m_version;
}

void je_ecs_world_entity_create_with_prefab(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_game_entity_t* prefab)
{
    auto entity = static_cast<World*>(world)
        ->create_entity_with_prefab(
            reinterpret_cast<const ArchType::entity*>(prefab));
    out_entity->_m_in_chunk = entity._m_in_chunk;
    out_entity->_m_id = entity._m_id;
    out_entity->_m_version = entity._m_version;
}

je_collected_requirements_t* je_ecs_requirements_collect(
    /* OPTIONAL: If total requirement count is 0 */ const je_component_requirement_t* requirements,
    size_t view_requiremnt_count,
    size_t other_requiremnts_count)
{
    je_collected_requirements_t* const new_collected = new je_collected_requirements_t();

    new_collected->m_view_required.resize(view_requiremnt_count);
    memcpy(
        new_collected->m_view_required.data(),
        requirements,
        view_requiremnt_count * sizeof(je_component_requirement_t));

    const size_t total_requiremet_count =
        view_requiremnt_count + other_requiremnts_count;

    for (size_t i = 0; i < total_requiremet_count; ++i)
    {
        const auto& requirement = requirements[i];
        switch (requirement.m_kind)
        {
        case JE_COMPONENT_REQUIRE_CONTAINS:
            types_set::insert(new_collected->m_contain_set, requirement.m_typeid);
            break;
        case JE_COMPONENT_REQUIRE_MAYNOT:
            break;
        case JE_COMPONENT_REQUIRE_EXCEPT:
            types_set::insert(new_collected->m_except_set, requirement.m_typeid);
            break;
        default /* JE_COMPONENT_REQUIRE_ANYOF_0... */:
            types_set::insert(
                new_collected->m_anyof_sets[requirement.m_kind], requirement.m_typeid);
            break;

        }
    }
    return new_collected;
}

void je_ecs_requirements_free(je_collected_requirements_t* requirements)
{
    delete requirements;
}

void je_ecs_world_collection_update(
    je_game_world_t* world,
    je_requirement_collection_t* collection)
{
    static_cast<World*>(world)->update_collection_archinfo(collection);
}

size_t je_ecs_world_arch_change_version_get(je_game_world_t* world)
{
    return static_cast<World*>(world)->get_arch_change_version();
}

/* OPTIONAL: If entity is invalid */ void* je_ecs_world_entity_component_add(
    const je_game_entity_t* entity,
    je_type_id_t type)
{
    auto* entity_located_world =
        static_cast<World*>(je_ecs_world_of_entity(entity));
    if (entity_located_world != nullptr)
        return entity_located_world->get_command_buffer().append_component(
            *reinterpret_cast<const ArchType::entity*>(entity),
            type);
    return nullptr;
}

void je_ecs_world_entity_component_remove(
    const je_game_entity_t* entity,
    je_type_id_t type)
{
    auto* entity_located_world =
        static_cast<World*>(je_ecs_world_of_entity(entity));
    if (entity_located_world != nullptr)
        entity_located_world->get_command_buffer().remove_component(
            *reinterpret_cast<const ArchType::entity*>(entity),
            type);
}

const je_game_entity_meta_t* je_arch_entity_meta_addr_in_chunk(je_chunk_t* chunk)
{
    return static_cast<ArchType::arch_chunk*>(chunk)->get_entity_meta();
}

/* OPTIONAL: If entity is invalid or no such component */ void* je_ecs_world_entity_component_get(
    const je_game_entity_t* entity,
    je_type_id_t type)
{
    auto& ecs_entity = *reinterpret_cast<const ArchType::entity*>(entity);
    auto* component = ecs_entity.get_component(type);
    return component;
}

void je_ecs_world_entity_destroy(
    je_game_world_t* world,
    const je_game_entity_t* entity)
{
    static_cast<World*>(world)
        ->get_command_buffer().remove_entity(
            *reinterpret_cast<const ArchType::entity*>(entity));
}

je_game_universe_t* je_ecs_world_in_universe(je_game_world_t* world)
{
    return static_cast<World*>(world)->get_universe();
}

/* OPTIONAL: If entity is invalid */ je_game_world_t* je_ecs_world_of_entity(const je_game_entity_t* entity)
{
    auto* chunk = static_cast<ArchType::arch_chunk*>(entity->_m_in_chunk);
    if (chunk != nullptr)
        return chunk->get_arch_type()->get_arch_mgr()->get_world();
    return nullptr;
}

bool je_ecs_world_slice_dependence_query(
    je_game_world_t* world,
    game_system* system_instance,
    je_type_hash_t slice_type_hash,
    je_requirement_collection_t** out_collection)
{
    return static_cast<World*>(world)
        ->fetch_and_request_slice_cache_dependence(
            system_instance, slice_type_hash, out_collection);
}

//////////////////// FOLLOWING IS DEBUG EDITOR API ////////////////////
je_game_world_t** je_debug_universe_worlds_get_all(je_game_universe_t* _universe)
{
    Universe* universe = static_cast<Universe*>(_universe);
    auto result = universe->_get_all_worlds();

    je_game_world_t** out_result = (je_game_world_t**)je_mem_alloc(sizeof(je_game_world_t*) * (result.size() + 1));

    je_game_world_t** write_place = out_result;
    for (auto* worlds : result)
        *(write_place++) = worlds;
    *write_place = nullptr;

    return out_result;
}

void je_debug_entity_free(je_game_entity_t* _entity_list)
{
    delete _entity_list;
}

je_game_entity_t** je_debug_world_entities_get_all(je_game_world_t* _world)
{
    World* world = static_cast<World*>(_world);

    vector<je_game_entity_t*> out_entities;

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
                if (entity_meta_arr[i].m_stat == JE_ENTITY_STAT_READY)
                {
                    je_game_entity_t* entity = new je_game_entity_t{};
                    entity->_m_in_chunk = chunk;
                    entity->_m_id = (je_entity_id_in_chunk_t)i;
                    entity->_m_version = entity_meta_arr[i].m_version;

                    out_entities.push_back(entity);
                }
            }
            chunk = chunk->last;
        }
    }
    je_game_entity_t** out_result = (je_game_entity_t**)je_mem_alloc(sizeof(je_game_entity_t*) * (out_entities.size() + 1));
    memcpy(out_result, out_entities.data(), out_entities.size() * sizeof(je_game_entity_t*));
    out_result[out_entities.size()] = nullptr;

    return out_result;
}

const je_type_info_t** je_debug_entity_components_get_all(const je_game_entity_t* _entity)
{
    const auto* cur_chunk = static_cast<ArchType::arch_chunk*>(_entity->_m_in_chunk);
    auto& cur_arch_type_infos = cur_chunk->get_arch_type()->get_type_infos();

    const je_type_info_t** outresult = (const je_type_info_t**)
        je_mem_alloc(
            sizeof(const je_type_info_t*) * (cur_arch_type_infos.size() + 1));

    auto write_result = outresult;
    for (auto* typeinfo : cur_arch_type_infos)
    {
        *(write_result++) = typeinfo;
    }
    *write_result = nullptr;

    // Sort by id.
    sort(outresult, write_result,
        [](const je_type_info_t* a, const je_type_info_t* b)
        {
            return a->m_id < b->m_id;
        });

    return outresult;
}

const je_type_info_t** je_debug_world_systems_attached_get_all(je_game_world_t* _world)
{
    World* world = static_cast<World*>(_world);
    auto& syss = world->get_system_instances();

    vector<const je_type_info_t*> attached_result;
    attached_result.reserve(syss.size());

    for (auto& attached_sys : syss)
        attached_result.push_back(attached_sys.first);

    size_t bufsz = attached_result.size() * sizeof(const je_type_info_t*);
    const je_type_info_t** result = (const je_type_info_t**)je_mem_alloc(bufsz + sizeof(const je_type_info_t*));

    memcpy(result, attached_result.data(), bufsz);
    result[attached_result.size()] = nullptr;

    return result;
}

void je_debug_entity_arch_information_get(
    je_game_entity_t* _entity,
    size_t* _out_chunk_size,
    size_t* _out_entity_size,
    size_t* _out_all_entity_count_in_chunk)
{
    auto* chunkaddr = static_cast<ArchType::arch_chunk*>(_entity->_m_in_chunk);

    *_out_chunk_size = CHUNK_SIZE;
    *_out_entity_size = chunkaddr->get_entity_size_in_chunk();
    *_out_all_entity_count_in_chunk = chunkaddr->get_entity_count_in_chunk();
}

void je_ecs_universe_pre_for_worlds_job_register(je_game_universe_t* universe, je_job_for_worlds_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_pre_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_pre_call_once_job_register(je_game_universe_t* universe, je_job_call_once_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_pre_call_once_job(job, data, freefunc);
}
void je_ecs_universe_for_worlds_job_register(je_game_universe_t* universe, je_job_for_worlds_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_call_once_job_register(je_game_universe_t* universe, je_job_call_once_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_call_once_job(job, data, freefunc);
}
void je_ecs_universe_after_for_worlds_job_register(je_game_universe_t* universe, je_job_for_worlds_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_after_for_worlds_job(job, data, freefunc);
}
void je_ecs_universe_after_call_once_job_register(je_game_universe_t* universe, je_job_call_once_t job, /* OPTIONAL */ void* data, /* OPTIONAL */ void (*freefunc)(void*))
{
    static_cast<Universe*>(universe)->register_after_call_once_job(job, data, freefunc);
}

void je_ecs_universe_pre_for_worlds_job_unregister(je_game_universe_t* universe, je_job_for_worlds_t job)
{
    static_cast<Universe*>(universe)->unregister_pre_for_worlds_job(job);
}
void je_ecs_universe_pre_call_once_job_unregister(je_game_universe_t* universe, je_job_call_once_t job)
{
    static_cast<Universe*>(universe)->unregister_pre_call_once_job(job);
}
void je_ecs_universe_for_worlds_job_unregister(je_game_universe_t* universe, je_job_for_worlds_t job)
{
    static_cast<Universe*>(universe)->unregister_for_worlds_job(job);
}
void je_ecs_universe_call_once_job_unregister(je_game_universe_t* universe, je_job_call_once_t job)
{
    static_cast<Universe*>(universe)->unregister_call_once_job(job);
}
void je_ecs_universe_after_for_worlds_job_unregister(je_game_universe_t* universe, je_job_for_worlds_t job)
{
    static_cast<Universe*>(universe)->unregister_after_for_worlds_job(job);
}
void je_ecs_universe_after_call_once_job_unregister(je_game_universe_t* universe, je_job_call_once_t job)
{
    static_cast<Universe*>(universe)->unregister_after_call_once_job(job);
}

double je_ecs_universe_frame_deltatime_get(je_game_universe_t* universe)
{
    return static_cast<Universe*>(universe)->get_frame_deltatime();
}
void je_ecs_universe_frame_deltatime_set(je_game_universe_t* universe, double delta)
{
    static_cast<Universe*>(universe)->set_frame_deltatime(delta);
}

double je_ecs_universe_real_deltatime_get(je_game_universe_t* universe)
{
    return static_cast<Universe*>(universe)->get_real_deltatime();
}
double je_ecs_universe_smooth_deltatime_get(je_game_universe_t* universe)
{
    return static_cast<Universe*>(universe)->get_smooth_deltatime();
}

double je_ecs_universe_max_deltatime_get(je_game_universe_t* universe)
{
    return static_cast<Universe*>(universe)->get_max_deltatime();
}
void je_ecs_universe_max_deltatime_set(je_game_universe_t* universe, double val)
{
    static_cast<Universe*>(universe)->set_max_deltatime(val);
}
void je_ecs_universe_time_scale_set(je_game_universe_t* universe, double scale)
{
    static_cast<Universe*>(universe)->set_time_scale(scale);
}
double je_ecs_universe_time_scale_get(je_game_universe_t* universe)
{
    return static_cast<Universe*>(universe)->get_time_scale();
}

///////////////////////
namespace jeecs::ecs
{
    void bootup(int, char**)
    {
    }

    void shutdown()
    {
        Universe::_shutdown_all_universe();
    }
}

#undef DEBUG_ARCH_LOG
#undef DEBUG_ARCH_LOG_WARN
#undef OFFSET_OF
