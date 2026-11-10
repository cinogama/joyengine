#pragma once

// ============================================================================
// jeecs/game.hpp —— ECS C++ 层：jeecs::game_world / game_universe / game_system / game_entity / slice_requirement / collection
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/basic.hpp"

#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::game_entity [核心类型]
    引擎的实体索引类型
    用于指示一个ArchChunk中的位置，实体索引会因为组件发生变化（移除或添加）而失效，
    实体索引仅在当前帧保证有效，任何情况下不应该跨帧地储存实体索引。
    注意：
        * 若实体索引所在的世界仍然存在，那么失效的实体索引仍能进行各项操作，但均
            不生效，亦无法从中获取任何组件。
        * 若实体索引所在的世界已经被销毁，那么实体索引进行的操作可能直接导致崩溃。
            换言之，任何情况下不要跨世界地储存实体索引
    说明：
    ECS引擎中，实体是一个仅存在于概念中的存在：一组有“关联”的组件集合，其关联
    即为实体。而由管理组件存储的ArchType特性决定，组件会在不同的ArchType中迁移，因
    此没有切实有效的手段可以持有一个实体的长期索引。
    实体索引是一个储存有：所属Chunk，位置和版本的三元组。其中，所属Chunk和位置可以
    确定和获取实体的组件，版本则用于确认当前索引是否是有效的——因为地址和位置会在
    实体迁移之后复用，此前持有的实体可能已经移动到其他位置。每次实体的变动，所属的
    Chunk都会对指定位置记录的版本信息进行更新。只需要校验Chunk内的版本和索引的版本
    即可。
    */
    class game_entity
    {
    public:
        // C ABI 实体句柄（平凡布局）。game_entity 以组合方式持有它，方法操作 _m_raw。
        // je_entity_stat_t / meta 不再嵌套于此，请使用 C 类型 je_entity_stat_t / je_game_entity_meta_t。
        je_game_entity_t _m_raw;

        inline game_entity& _set_arch_chunk_info(
            je_chunk_t* chunk,
            je_entity_id_in_chunk_t index,
            je_version_t ver) noexcept
        {
            _m_raw._m_in_chunk = chunk;
            _m_raw._m_id = index;
            _m_raw._m_version = ver;

            return *this;
        }

        /*
        jeecs::game_entity::get_component [方法]
        从指定实体中获取一个指定类型的组件。
        若实体失效或实体不存在指定组件，则返回nullptr
        否则返回指定类型组件的地址
        */
        template <typename T>
        inline T* get_component() const noexcept;

        /*
        jeecs::game_entity::add_component [方法]
        向指定实体中添加一个指定类型的组件，若最终导致实体的组件发生变化，
        那么旧的实体索引在下一帧失效
            * 若实体失效则返回nullptr,
            * 无论实体是否存在指定的组件，总是创建一个新的组件实例并返回其地址
            * `最后`执行对组件的操作会真正生效，如果生效的是添加操作，那么：
                1. 如果实体此前已有存在的组件，则替换之
                2. 如果实体此前不存在此组件，则更新实体，旧的实体索引将失效
        */
        template <typename T>
        inline T* add_component() const noexcept;

        /*
        jeecs::game_entity::remove_component [方法]
        向指定实体中移除一个指定类型的组件，若最终导致实体的组件发生变化，
        那么旧的实体索引在下一帧失效
            * `最后`执行对组件的操作会真正生效，如果生效的是移除操作，那么：
                1. 如果实体此前已经存在此组件，那么更新实体，旧的实体索引将失效
                2. 如果实体此前不存在此组件，那么无事发生
        */
        template <typename T>
        inline void remove_component() const noexcept;

        /*
        jeecs::game_entity::game_world [方法]
        获取当前实体所属的世界
            * 即便实体索引失效，此方法依然能返回所属的世界
        */
        inline jeecs::game_world game_world() const noexcept;

        /*
        jeecs::game_entity::close [方法]
        若索引未失效，则关闭当前实体索引指向的实体。实体索引将在下一帧失效
        */
        inline void close() const noexcept;

        inline bool operator==(const game_entity& e) const noexcept
        {
            return _m_raw._m_in_chunk == e._m_raw._m_in_chunk &&
                _m_raw._m_id == e._m_raw._m_id &&
                _m_raw._m_version == e._m_raw._m_version;
        }
        inline bool operator!=(const game_entity& e) const noexcept
        {
            return _m_raw._m_in_chunk != e._m_raw._m_in_chunk ||
                _m_raw._m_id != e._m_raw._m_id ||
                _m_raw._m_version != e._m_raw._m_version;
        }
    };
    static_assert(std::is_trivial_v<game_entity>);
    static_assert(std::is_standard_layout_v<game_entity>);
    class game_universe;

    class game_world
    {
        je_game_world_t* _m_ecs_world_addr;

    public:
        game_world(je_game_world_t* ecs_world_addr)
            : _m_ecs_world_addr(ecs_world_addr)
        {
        }

    private:
        friend class game_system;

    public:
        inline je_game_world_t* handle() const noexcept
        {
            return _m_ecs_world_addr;
        }

        template <typename FirstCompT, typename... CompTs>
        inline game_entity add_entity()
        {
            const je_type_id_t component_ids[] = {
                typing::id<FirstCompT>(),
                typing::id<CompTs>()...,
            };

            game_entity gentity;

            je_ecs_world_entity_create_with_components(
                handle(), &gentity._m_raw, component_ids,
                sizeof(component_ids) / sizeof(je_type_id_t));

            return gentity;
        }
        inline game_entity add_entity(const game_entity& prefab)
        {
            game_entity gentity;
            je_ecs_world_entity_create_with_prefab(
                handle(), &gentity._m_raw, &prefab._m_raw);

            return gentity;
        }
        template <typename FirstCompT, typename... CompTs>
        inline game_entity add_prefab()
        {
            const je_type_id_t component_ids[] = {
                typing::id<FirstCompT>(),
                typing::id<CompTs>()...,
            };

            game_entity gentity;

            je_ecs_world_prefab_create_with_components(
                handle(), &gentity._m_raw, component_ids,
                sizeof(component_ids) / sizeof(je_type_id_t));

            return gentity;
        }

        template <typename SystemT>
        inline SystemT* add_system()
        {
            static_assert(
                std::is_base_of_v<game_system, SystemT>,
                "SystemT must be based of `game_system`.");

            return static_cast<SystemT*>(
                je_ecs_world_system_instance_add(handle(), typing::id<SystemT>()));
        }

        template <typename SystemT>
        inline SystemT* get_system()
        {
            static_assert(
                std::is_base_of_v<game_system, SystemT>,
                "SystemT must be based of `game_system`.");

            return static_cast<SystemT*>(
                je_ecs_world_system_instance_get(handle(), typing::id<SystemT>()));
        }

        template <typename SystemT>
        inline void remove_system()
        {
            static_assert(
                std::is_base_of_v<game_system, SystemT>,
                "SystemT must be based of `game_system`.");

            je_ecs_world_system_instance_remove(handle(), typing::id<SystemT>());
        }

        // This function only used for editor.
        inline std::optional<game_entity> _add_entity(const std::vector<je_type_id_t>& components)
        {
            if (components.empty())
                return std::nullopt;

            game_entity gentity;
            je_ecs_world_entity_create_with_components(
                handle(), &gentity._m_raw, components.data(), components.size());

            return gentity;
        }
        // This function only used for editor.
        inline std::optional<game_entity> _add_prefab(const std::vector<je_type_id_t>& components)
        {
            if (components.empty())
                return std::nullopt;

            game_entity gentity;
            je_ecs_world_prefab_create_with_components(
                handle(), &gentity._m_raw, components.data(), components.size());

            return gentity;
        }

        inline void remove_entity(const game_entity& entity)
        {
            je_ecs_world_entity_destroy(handle(), &entity._m_raw);
        }

        inline operator bool() const noexcept
        {
            return handle() != nullptr;
        }

        void close() const noexcept
        {
            je_ecs_world_destroy(_m_ecs_world_addr);
        }

        void set_enable(bool able) const noexcept
        {
            je_ecs_world_enable_set(handle(), able);
        }

        inline game_universe get_universe() const noexcept;
    };

    // 组件筛选要求现在直接使用 C ABI 的 je_component_requirement_t（见文件顶部 C ABI 段），
    // 字段对应：m_kind / m_group_id / m_typeid，枚举值 JE_COMPONENT_REQUIRE_*。
    static_assert(std::is_trivial_v<je_component_requirement_t>);

    namespace slice_requirement
    {
        namespace base
        {
            struct view_base
            {
                struct component_info
                {
                    size_t m_component_offset_in_chunk;
                    size_t m_component_offset_of_unit;
                };

            public:
                // 接收预缓存的单个 component_info，避免每次解引用都走扁平偏移数组的双重间接。
                inline static void* get_component_by_cached_info(
                    const component_info* info,
                    je_chunk_t* chunkbuf,
                    je_entity_id_in_chunk_t entity_id)
                {
                    if (info->m_component_offset_of_unit != 0)
                    {
                        size_t offset = info->m_component_offset_in_chunk
                            + info->m_component_offset_of_unit * entity_id;
                        return reinterpret_cast<char*>(chunkbuf) + offset;
                    }
                    return nullptr;
                }
                // 编译期版本：cid 由 pack_index_v 在编译期决定；
                //cached_infos 应为调用方预缓存的 component_info 数组（按 view 的 Components... 顺序）。
                // 对于 CONTAINS（引用）类型走无分支路径；
                // 对于 MAYNOT（指针）类型保留 nullptr 检查。
                template <typename ComponentT, typename... ArgTs>
                inline static ComponentT get_component_from_archchunk(
                    const component_info* cached_infos,
                    je_chunk_t* chunkbuf,
                    je_entity_id_in_chunk_t entity_id)
                {
                    constexpr size_t cid = typing::pack_index_v<ComponentT, ArgTs...>;
                    static_assert(cid != SIZE_MAX, "ComponentT must be one of ArgTs...");
                    const auto& info = cached_infos[cid];

                    if constexpr (std::is_pointer_v<ComponentT>)
                    {
                        // MAYNOT：offset_of_unit == 0 表示此 arch 不含该可选组件
                        if (info.m_component_offset_of_unit == 0)
                            return nullptr;
                    }

                    // CONTAINS 路径无分支：arch 保证 offset_of_unit > 0
                    assert(sizeof(typename typing::origin_t<ComponentT>) == info.m_component_offset_of_unit);

                    auto* component_ptr = static_cast<typename typing::origin_t<ComponentT>*>(
                        static_cast<void*>(
                            reinterpret_cast<char*>(chunkbuf)
                            + info.m_component_offset_in_chunk
                            + sizeof(typename typing::origin_t<ComponentT>) * entity_id));

                    if constexpr (std::is_reference_v<ComponentT>)
                        return *component_ptr;
                    else
                    {
                        static_assert(std::is_pointer_v<ComponentT>);
                        return component_ptr;
                    }
                }
            protected:
                template<typename T, typename ... Ts>
                static void _apply_dependence_impl(basic::vector<je_component_requirement_t>* out_requirements)
                {
                    static_assert(
                        std::is_pointer_v<T> || std::is_reference_v<T>);

                    out_requirements->push_back(
                        je_component_requirement_t{
                            std::is_pointer_v<T>
                                ? JE_COMPONENT_REQUIRE_MAYNOT
                                : JE_COMPONENT_REQUIRE_CONTAINS,
                            typing::id<typing::origin_t<T>>() });

                    if constexpr (sizeof...(Ts) > 0)
                        _apply_dependence_impl<Ts...>(out_requirements);
                }

                template<typename ... Ts>
                static void _apply_dependence(basic::vector<je_component_requirement_t>* out_requirements)
                {
                    if constexpr (sizeof...(Ts) > 0)
                        _apply_dependence_impl<Ts...>(out_requirements);
                }
            };

            template<je_component_requirement_kind_t RequireType>
            struct requirement_base
            {
                template<typename T, typename ... Ts>
                static void _apply_dependence_impl(int group, basic::vector<je_component_requirement_t>* out_requirements)
                {
                    out_requirements->push_back(
                        je_component_requirement_t{
                            RequireType >= JE_COMPONENT_REQUIRE_ANYOF_0
                                ? RequireType + group
                                : RequireType,
                            typing::id<typing::origin_t<T>>() });

                    if constexpr (sizeof...(Ts) > 0)
                        _apply_dependence_impl<Ts...>(group, out_requirements);
                }

                template<typename ... Ts>
                static void _apply_dependence(int group, basic::vector<je_component_requirement_t>* out_requirements)
                {
                    if constexpr (sizeof...(Ts) > 0)
                        _apply_dependence_impl<Ts...>(group, out_requirements);
                }
            };
            struct contains_base : requirement_base<JE_COMPONENT_REQUIRE_CONTAINS> {};
            struct except_base : requirement_base<JE_COMPONENT_REQUIRE_EXCEPT> {};
            struct anyof_base : requirement_base<JE_COMPONENT_REQUIRE_ANYOF_0> {};
        }

        template<typing::traits::is_reference_or_pointer ... Components>
        struct view : base::view_base
        {
            using components = std::tuple<Components...>;
            using entity_with_components = std::tuple<const game_entity, Components...>;
            static void apply_dependence(basic::vector<je_component_requirement_t>* out_requirements)
            {
                _apply_dependence<Components...>(out_requirements);
            }

            static components fetch_component_slice_from_chunk(
                const component_info* cached_infos,
                je_chunk_t* chunkbuf,
                je_entity_id_in_chunk_t entity_id)
            {
                return components{
                    get_component_from_archchunk<Components, Components...>(
                        cached_infos, chunkbuf, entity_id)... };
            }
            static entity_with_components fetch_entity_and_component_slice_from_chunk(
                const component_info* cached_infos,
                je_chunk_t* chunkbuf,
                je_entity_id_in_chunk_t entity_id,
                je_version_t entity_version)
            {
                return entity_with_components{
                    game_entity{
                        chunkbuf,
                        entity_id,
                        entity_version,
                    },
                    get_component_from_archchunk<Components, Components...>(
                        cached_infos, chunkbuf, entity_id)... };
            }
        };
        template<typename ... Components>
        struct contains : base::contains_base
        {
            using components = std::tuple<Components...>;
            static void apply_dependence(int group, basic::vector<je_component_requirement_t>* out_requirements)
            {
                _apply_dependence<Components...>(group, out_requirements);
            }
        };
        template<typename ... Components>
        struct except : base::except_base
        {
            using components = std::tuple<Components...>;
            static void apply_dependence(int group, basic::vector<je_component_requirement_t>* out_requirements)
            {
                _apply_dependence<Components...>(group, out_requirements);
            }
        };
        template<typename ... Components>
        struct anyof : base::anyof_base
        {
            using components = std::tuple<Components...>;
            static void apply_dependence(int group, basic::vector<je_component_requirement_t>* out_requirements)
            {
                _apply_dependence<Components...>(group, out_requirements);
            }
        };

        namespace traits
        {
            template<typename T>
            concept is_view = std::is_base_of_v<base::view_base, T>;

            template<typename T>
            concept is_requirement =
                std::is_base_of_v<base::contains_base, T>
                || std::is_base_of_v<base::except_base, T>
                || std::is_base_of_v<base::anyof_base, T>;
        }
    }

    /*
    从 jeecs::selector 接过筛选组件集合的大任，作为其的继任者，collection 机制
    闪亮登场！

    然而实际上这个玩意儿并没有做什么特别的事情，只是把原本 selector 从传入的函数类型
    中析取对组件的类型要求改成了传入 SliceView 和 SliceRequirements 来表达。

    此外，从原本的传递一个回调函数改成了一个满足 C++ 旧式前向迭代器规范的迭代器类型，
    可以更友好地适配 STL 提供的算法。

    此外，筛选器的依赖缓存也被转移到了 World 内（指 System 的 query 系列方法），这样
    做的目的是避免在系统类型内定义切片实例（写起来麻烦）；当然，喜欢创建独立的 collection
    实例也是被允许的，或许真的有人喜欢这么做呢？

    最后，因为系统的 query 将依赖状态放在了 World 内，所以其更新也不需要每次都执行检
    查（由 World 代办了）。最棒的是，这样一来，再也不需要像 selector 机制一样，对 query
    有严格的执行顺序要求，完全可以随意地按任何顺序执行，甚至能够并行执行！

    当然，collection 并没有规避所有的问题：因此吧之前 selector 机制中的说明贴在下面：

    ```
        实际上只要和ArchSystem扯上关系，就永远不可能干净。很不幸，选择器正是一根搅屎棍，它负责从
        ArchSystem管理的区域内按照我们的需求，分离出满足我们需求的ArchType，再从上面把合法的组件
        一个个摘出来递到我们面前。

        在这里——jeecs.h中，选择器的实现已经显得非常麻烦，但实际上这里只是选择器的一部分，在
        ArchSystem中，有一个名为je_ecs_world_update_dependences_archinfo的函数。这个函数在黑暗处
        负责在适当的实际更新选择器的筛选结果。

        为了优雅，背后就得承担代价；为了性能我们就得做出牺牲。伟大的圣窝窝头，这么做真的值得吗？

                                                                       ——虔诚的窝窝头信徒
                                                                           mr_cino
    ```
    至于 selector，感谢它三年来的付出和陪伴，从 JoyEngine 4.9 版本起，不再提供。
    */

    template<
        slice_requirement::traits::is_view SliceView,
        slice_requirement::traits::is_requirement ... SliceRequirements>
    class collection
    {
        template<int Group, typename T, typename ... Ts>
        static void _apply_requirements_impl(basic::vector<je_component_requirement_t>* reqs)
        {
            static_assert(
                std::is_base_of_v<slice_requirement::base::contains_base, T>
                || std::is_base_of_v<slice_requirement::base::except_base, T>
                || std::is_base_of_v<slice_requirement::base::anyof_base, T>);

            T::apply_dependence(Group, reqs);

            if constexpr (sizeof...(Ts) > 0)
                _apply_requirements_impl<Group + 1, Ts...>(reqs);
        }
        template<int Group, typename ... Ts>
        static void _apply_requirements(basic::vector<je_component_requirement_t>* reqs)
        {
            if constexpr (sizeof...(Ts) > 0)
                _apply_requirements_impl<Group, Ts...>(reqs);
        }
    public:
        // 首次初始化：收集视图需求（CONTAINS/MAYNOT）与其它需求（contain/except/anyof），
        // 交由 C ABI 的 je_ecs_requirements_collect 生成不透明的 je_collected_requirements_t
        // 并挂到 collection->m_collected_requirement 上。视图需求必须排在最前。
        static void apply_requirements(je_requirement_collection_t* collection)
        {
            static_assert(std::is_base_of_v<slice_requirement::base::view_base, SliceView>,
                "First template argument of collection must be collection::view.");

            basic::vector<je_component_requirement_t> reqs;
            SliceView::apply_dependence(&reqs);
            const size_t view_requirement_count = reqs.size();
            _apply_requirements<1, SliceRequirements...>(&reqs);

            collection->m_collected_requirement = je_ecs_requirements_collect(
                reqs.data(),
                view_requirement_count,
                reqs.size() - view_requirement_count);
        }

    public:
        // CRTP 基类：封装 slice / entity_slice 共享的迭代逻辑与状态。
        // 派生类只需提供 value_type、iterator typedefs、operator* 以及接受
        // const je_requirement_collection_t* / const je_dependence_arch_infos_t* 的构造函数。
        template <typename Derived>
        class slice_base
        {
        public:
            static constexpr size_t _component_count =
                std::tuple_size_v<typename SliceView::components>;

        protected:
            const je_dependence_arch_infos_t* m_archs_current;
            const je_dependence_arch_infos_t* m_archs_end;

            je_chunk_t* m_chunk_current;
            const je_game_entity_meta_t* m_chunk_current_entity_meta;
            je_entity_id_in_chunk_t m_chunk_entity_current_index;

            // 预缓存本视图所需的 component_info（按 SliceView::Components... 顺序）。
            // 在进入新 arch 时一次性刷新，避免每次解引用都走 je_dependence_arch_infos_t 的
            // m_view_component_offset / m_view_component_size 双重间接。
            std::array<typename SliceView::component_info, _component_count> m_cached_infos{};

            slice_base() = default;

            // For `end()` only.
            explicit slice_base(const je_dependence_arch_infos_t* _archs_end)
                : m_archs_current(_archs_end)
                , m_archs_end(_archs_end)
                , m_chunk_current(nullptr)
                , m_chunk_current_entity_meta(nullptr)
                , m_chunk_entity_current_index(0)
            {
            }

            // 进入一个新 arch：刷新 chunk、meta、预缓存 component_info。
            inline void _enter_current_arch()
            {
                m_chunk_current = je_arch_chunk_get(m_archs_current->m_arch);
                assert(m_chunk_current != nullptr);
                m_chunk_current_entity_meta = je_arch_entity_meta_addr_in_chunk(m_chunk_current);

                for (size_t i = 0; i < _component_count; ++i)
                {
                    m_cached_infos[i].m_component_offset_in_chunk =
                        m_archs_current->m_view_component_offset[i];
                    m_cached_infos[i].m_component_offset_of_unit =
                        m_archs_current->m_view_component_size[i];
                }
            }
            // 在同一 arch 内切换 chunk：仅刷新 meta 指针。
            inline void _enter_current_chunk()
            {
                m_chunk_current_entity_meta = je_arch_entity_meta_addr_in_chunk(m_chunk_current);
            }

            void _move_to_valid_entity()
            {
                for (;;)
                {
                    if (m_chunk_entity_current_index >= m_archs_current->m_entity_count)
                    {
                        // Move to next chunk.
                        m_chunk_entity_current_index = 0;
                        m_chunk_current = je_arch_chunk_next(m_chunk_current);
                        if (m_chunk_current == nullptr)
                        {
                            // Move to next arch.
                            if (++m_archs_current == m_archs_end)
                                // End! m_archs_current == m_archs_end && m_chunk_current == nullptr
                                break;

                            _enter_current_arch();
                        }
                        else
                        {
                            _enter_current_chunk();
                        }
                    }

                    if (JE_ENTITY_STAT_READY
                        == m_chunk_current_entity_meta[m_chunk_entity_current_index].m_stat)
                        break;

                    ++m_chunk_entity_current_index;
                }
            }

            // 由派生类的 collection 构造函数调用，统一初始化路径。
            void _init_from_collection(const je_requirement_collection_t* collection)
            {
                m_archs_current = collection->m_cached_archs;
                m_archs_end = collection->m_cached_archs + collection->m_cached_arch_count;
                m_chunk_entity_current_index = 0;

                if (m_archs_current != m_archs_end)
                {
                    _enter_current_arch();
                    _move_to_valid_entity();
                }
                else
                {
                    // NOTE: 没有枚举到任何 ArchType，直接置为 end 状态。
                    m_chunk_current = nullptr;
                    m_chunk_current_entity_meta = nullptr;
                    m_chunk_entity_current_index = 0;
                }
            }

        public:
            Derived& operator ++()
            {
                ++m_chunk_entity_current_index;
                _move_to_valid_entity();
                return static_cast<Derived&>(*this);
            }
            Derived operator ++(int)
            {
                Derived current = static_cast<Derived&>(*this);
                ++m_chunk_entity_current_index;
                _move_to_valid_entity();
                return current;
            }
            bool operator ==(const Derived& pindex) const
            {
                return m_chunk_current == pindex.m_chunk_current
                    && m_chunk_entity_current_index == pindex.m_chunk_entity_current_index;
            }
            bool operator !=(const Derived& pindex) const
            {
                return m_chunk_current != pindex.m_chunk_current
                    || m_chunk_entity_current_index != pindex.m_chunk_entity_current_index;
            }

            Derived begin()
            {
                return static_cast<Derived&>(*this);
            }
            Derived end() const
            {
                return Derived(m_archs_end);
            }

            // 注意：slice 是 forward_iterator，调用 std::for_each(par_unseq, ...)
            // 时大多数标准库实现难以有效切分工作（无法随机访问）。
            template<typename FT>
            void foreach_parallel(FT&& ft)
            {
                ::jeecs::parallel_foreach(
                    static_cast<Derived&>(*this),
                    end(),
                    std::forward<FT>(ft));
            }
        };

        class slice : public slice_base<slice>
        {
            using base_t = slice_base<slice>;
        public:
            typedef ptrdiff_t difference_type;
            typedef typename SliceView::components value_type;
            typedef void pointer;
            typedef void reference;
            typedef std::forward_iterator_tag iterator_category;

            slice() = default;
            slice(const slice&) = default;
            slice(slice&&) = default;
            slice& operator=(const slice&) = default;
            slice& operator=(slice&&) = default;

            explicit slice(const je_dependence_arch_infos_t* _archs_end)
                : base_t(_archs_end) {
            }
            explicit slice(const je_requirement_collection_t* collection)
            {
                this->_init_from_collection(collection);
            }

            value_type operator*()
            {
                return SliceView::fetch_component_slice_from_chunk(
                    this->m_cached_infos.data(),
                    this->m_chunk_current,
                    this->m_chunk_entity_current_index);
            }
        };

        class entity_slice : public slice_base<entity_slice>
        {
            using base_t = slice_base<entity_slice>;
        public:
            typedef ptrdiff_t difference_type;
            typedef typename SliceView::entity_with_components value_type;
            typedef void pointer;
            typedef void reference;
            typedef std::forward_iterator_tag iterator_category;

            entity_slice() = default;
            entity_slice(const entity_slice&) = default;
            entity_slice(entity_slice&&) = default;
            entity_slice& operator=(const entity_slice&) = default;
            entity_slice& operator=(entity_slice&&) = default;

            explicit entity_slice(const je_dependence_arch_infos_t* _archs_end)
                : base_t(_archs_end) {
            }
            explicit entity_slice(const je_requirement_collection_t* collection)
            {
                this->_init_from_collection(collection);
            }

            value_type operator*()
            {
                return SliceView::fetch_entity_and_component_slice_from_chunk(
                    this->m_cached_infos.data(),
                    this->m_chunk_current,
                    this->m_chunk_entity_current_index,
                    this->m_chunk_current_entity_meta[this->m_chunk_entity_current_index].m_version);
            }
        };
    };

    class game_universe
    {
        je_game_universe_t* _m_universe_addr;

    public:
        game_universe(je_game_universe_t* universe_addr)
            : _m_universe_addr(universe_addr)
        {
        }
        inline je_game_universe_t* handle() const noexcept
        {
            return _m_universe_addr;
        }
        inline game_world create_world()
        {
            return je_ecs_world_create(_m_universe_addr);
        }
        inline void wait() const noexcept
        {
            je_ecs_universe_loop(handle());
        }
        inline void grow() const noexcept
        {
            je_ecs_universe_lifetime_grow(handle());
        }
        inline void trim() const noexcept
        {
            je_ecs_universe_lifetime_trim(handle());
        }
        inline double get_real_deltatimed() const
        {
            return je_ecs_universe_real_deltatime_get(handle());
        }
        inline float get_real_deltatime() const
        {
            return (float)get_real_deltatimed();
        }
        inline double get_smooth_deltatimed() const
        {
            return je_ecs_universe_smooth_deltatime_get(handle());
        }
        inline float get_smooth_deltatime() const
        {
            return (float)get_smooth_deltatimed();
        }
        inline double get_timescaled() const
        {
            return je_ecs_universe_time_scale_get(handle());
        }
        inline float get_timescale() const
        {
            return (float)get_timescaled();
        }
        inline void set_timescaled(double scale) const
        {
            je_ecs_universe_time_scale_set(handle(), scale);
        }
        inline void set_timescale(float scale) const
        {
            set_timescaled((double)scale);
        }

        inline operator bool() const noexcept
        {
            return _m_universe_addr;
        }

    public:
        static game_universe create_universe()
        {
            return game_universe(je_ecs_universe_create());
        }
        static void destroy_universe(game_universe universe)
        {
            return je_ecs_universe_destroy(universe.handle());
        }
    };

    class game_system
    {
        JECS_DISABLE_MOVE_AND_COPY(game_system);

    private:
        game_world _m_game_world;

        template<
            slice_requirement::traits::is_view SliceView,
            slice_requirement::traits::is_requirement... SliceRequirements>
        je_requirement_collection_t* _fetch_query_slice_cache()
        {
            je_requirement_collection_t* requirement_collection;
            je_game_world_t* const world_inst = get_world().handle();

            if (!je_ecs_world_slice_dependence_query(
                world_inst,
                this,
                typeid(collection<SliceView, SliceRequirements...>).hash_code(),
                &requirement_collection))
            {
                // This collection is just created, need to apply requirements
                // for first-time initialization, then update arch info.
                collection<SliceView, SliceRequirements...>::apply_requirements(
                    requirement_collection);
                je_ecs_world_collection_update(world_inst, requirement_collection);
            }
            return requirement_collection;
        }

    public:
        game_system(game_world world)
            : _m_game_world(world)
        {
        }

        inline double deltatimed() const
        {
            return _m_game_world.get_universe().get_real_deltatimed();
        }
        inline float deltatime() const
        {
            return _m_game_world.get_universe().get_real_deltatime();
        }
        inline double smooth_deltatimed() const
        {
            return _m_game_world.get_universe().get_smooth_deltatimed();
        }
        inline float smooth_deltatime() const
        {
            return _m_game_world.get_universe().get_smooth_deltatime();
        }
        inline double timescaled() const
        {
            return _m_game_world.get_universe().get_timescaled();
        }
        inline float timescale() const
        {
            return _m_game_world.get_universe().get_timescale();
        }

        // Get binded world or attached world
        inline game_world get_world() const noexcept
        {
            assert(_m_game_world);
            return _m_game_world;
        }

        template<
            slice_requirement::traits::is_view SliceView,
            slice_requirement::traits::is_requirement ... SliceRequirements>
        inline typename collection<SliceView, SliceRequirements...>::slice query()
        {
            return typename collection<SliceView, SliceRequirements...>::slice(
                _fetch_query_slice_cache<SliceView, SliceRequirements...>());
        }
        template<
            slice_requirement::traits::is_view SliceView,
            slice_requirement::traits::is_requirement ... SliceRequirements>
        inline typename collection<SliceView, SliceRequirements...>::entity_slice query_entity()
        {
            return typename collection<SliceView, SliceRequirements...>::entity_slice(
                _fetch_query_slice_cache<SliceView, SliceRequirements...>());
        }
    };

    inline game_universe game_world::get_universe() const noexcept
    {
        return game_universe(je_ecs_world_in_universe(handle()));
    }

    template <typename T>
    inline T* game_entity::get_component() const noexcept
    {
        return (T*)je_ecs_world_entity_component_get(&_m_raw,
            typing::id<T>());
    }
    template <typename T>
    inline T* game_entity::add_component() const noexcept
    {
        return (T*)je_ecs_world_entity_component_add(&_m_raw,
            typing::id<T>());
    }
    template <typename T>
    inline void game_entity::remove_component() const noexcept
    {
        return je_ecs_world_entity_component_remove(&_m_raw,
            typing::id<T>());
    }

    inline jeecs::game_world game_entity::game_world() const noexcept
    {
        return jeecs::game_world(je_ecs_world_of_entity(&_m_raw));
    }
    inline void game_entity::close() const noexcept
    {
        if (_m_raw._m_in_chunk == nullptr)
            return;

        game_world().remove_entity(*this);
    }
}
#endif // __cplusplus
