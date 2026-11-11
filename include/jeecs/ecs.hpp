#pragma once

// ============================================================================
// jeecs/ecs.hpp —— ECS：实体/世界/宇宙/原型 C 类型与 je_ecs_* / je_arch_* C API
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/core.hpp"

// ---- C ABI ECS 不透明句柄（opaque handle）前向声明 ----
// 这些类型对外仅暴露指针；完整定义在引擎内部（src/）。C/C++ 通用，二进制兼容。
// 在引擎内部以空结构体作为公开基类，实际实现类（jeecs::ecs_universe 等）继承它们，
// 因此 C ABI 边界处可使用干净的 derived↔base static_cast，无需 reinterpret_cast。
// 此处提前声明是因为 je_game_entity_t（见下方）需要引用 je_chunk_t。
typedef struct je_game_universe_t je_game_universe_t;   // ECS 宇宙（全局上下文）
typedef struct je_game_world_t    je_game_world_t;      // ECS 世界
typedef struct je_archtype_t je_archtype_t;   // ECS 原型（ArchType）
typedef struct je_chunk_t    je_chunk_t;      // ECS 原型分块（ArchChunk）

// ---- 实体（game_entity）C 类型 ----
// jeecs::game_entity（C++ 包装）以组合方式持有 je_game_entity_t 作为成员；
// je_entity_stat_t / meta 不再提供 C++ 别名，直接使用 je_entity_stat_t / je_game_entity_meta_t。
typedef uint32_t je_entity_id_in_chunk_t;

typedef enum je_entity_stat_t {
    JE_ENTITY_STAT_UNAVAILABLE = 0, // Entity is destroyed or just not ready.
    JE_ENTITY_STAT_READY,           // Entity is OK, works as normal.
    JE_ENTITY_STAT_PREFAB,          // Current entity is prefab.
} je_entity_stat_t;

typedef struct je_game_entity_meta_t {
    je_version_t m_version;
    je_entity_stat_t m_stat;
} je_game_entity_meta_t;

// 注意：除预设体（JE_ENTITY_STAT_PREFAB）之外的 je_game_entity_t，只允许在一帧期间持有；
// 一帧结束时，引擎可能执行碎片回收（defragmentation），导致 je_game_entity_t 索引失效。
typedef struct je_game_entity_t {
    je_chunk_t* _m_in_chunk;
    je_entity_id_in_chunk_t _m_id;
    je_version_t _m_version;
} je_game_entity_t;

#ifdef __cplusplus
// je_ecs_world_slice_dependence_query 等接口引用 jeecs::game_system*；
// 完整定义见 jeecs/game.hpp。
namespace jeecs
{
    class game_system;
    class game_world;
}
#endif // __cplusplus

JE_FORCE_CAPI

////////////////////// ARCH //////////////////////

/*
je_arch_chunk_get [基本接口]
通过给定的ArchType，获取其首个Chunk，由于ArchType总是至少有一个Chunk，
所以总是返回非nullptr值。
    * 此方法一般由 collection 调用，用于获取指定ArchType中的组件信息
*/
JE_API je_chunk_t* je_arch_chunk_get(je_archtype_t* archtype);

/*
je_arch_chunk_next [基本接口]
通过给定的Chunk，获取其下一个Chunk，如果给定Chunk没有后继则返回nullptr
    * 此方法一般由 collection 调用，用于获取指定ArchType中的组件信息
*/
JE_API /* OPTIONAL: If no successor chunk */ je_chunk_t* je_arch_chunk_next(je_chunk_t* chunk);

/*
je_arch_entity_meta_addr_in_chunk [基本接口]
通过给定的Chunk，获取Chunk中的实体元数据起始地址。
*/
JE_API const je_game_entity_meta_t* je_arch_entity_meta_addr_in_chunk(je_chunk_t* chunk);
////////////////////// ECS //////////////////////

/*
je_ecs_universe_create [基本接口]
创建指定的宇宙（全局上下文）
引擎允许同时存在多个Universe，原则上不同Universe之间的数据严格隔离并无关
引擎中所有的世界、实体（组件）都位于特定的宇宙中。
说明：
    宇宙创建后会创建一个线程，在循环内执行逻辑更新等操作。此循环会在宇宙的生命周期结束终止，
    且所有世界关闭之后退出，并依次执行以下操作：
        1. 处理剩余的Universe消息
        2. 按注册的相反顺序调用退出时回调，若Universe仍有消息未处理，
        返回第一步
        3. 解除注册所有Job
    完成全部操作后，宇宙将处于可销毁状态。
*/
JE_API je_game_universe_t* je_ecs_universe_create(void);

/*
je_ecs_universe_loop [基本接口]
等待指定的宇宙直到其完全退出运行，这是一个阻塞函数。
此函数一般用于宇宙创建并完成初始状态设定之后，阻塞主线程避免过早的退出。
    * 由于实现机制（依赖Universe的退出回调函数）限制，不允许在Universe被
    stop之后调用，否则将导致死锁
请参见：
    je_ecs_universe_exit_callback_register
*/
JE_API void je_ecs_universe_loop(je_game_universe_t* universe);

/*
je_ecs_universe_destroy [基本接口]
销毁一个宇宙，阻塞直到Universe完全销毁
* 使用此操作销毁宇宙时，无视宇宙的生命周期计数，立即请求终止宇宙的运行
请参见：
    je_ecs_universe_loop
*/
JE_API void je_ecs_universe_destroy(je_game_universe_t* universe);

/*
je_ecs_universe_lifetime_grow [基本接口]
延长指定宇宙的生命周期，防止其自动销毁
* 此函数用于在需要手动管理宇宙生命周期时使用，每次调用都会增加宇宙的引用计数
* 使用 je_ecs_universe_lifetime_trim 缩短生命周期
请参见：
    je_ecs_universe_lifetime_trim
*/
JE_API void je_ecs_universe_lifetime_grow(je_game_universe_t* universe);

/*
je_ecs_universe_lifetime_trim [基本接口]
减少指定宇宙的生命周期引用计数
    * universe 创建时自带有一次引用计数。
    * 当引用计数归零时，请求终止指定宇宙的运行，此函数会请求销毁宇宙中的所有世界，宇宙
        会在所有世界完全关闭后终止运行。
    * 这意味着如果在退出过程中创建了新的世界，宇宙的工作将继续持续直到这些世界完全退出，
        因此不推荐在组件/系统的析构函数中做多余的逻辑操作。析构函数仅用于释放资源。
    请参见：
        je_ecs_universe_lifetime_grow
*/
JE_API void je_ecs_universe_lifetime_trim(je_game_universe_t* universe);

/*
je_ecs_universe_exit_callback_register [基本接口]
向指定宇宙中注册宇宙关闭之后的回调函数，关于回调函数的调用时机：
    * 不能对已经终止的宇宙注册回调，回调函数将不会得到执行，并导致内存泄漏！
请参见
    je_ecs_universe_create
*/
JE_API void je_ecs_universe_exit_callback_register(
    je_game_universe_t* universe,
    void (*callback)(void*),
    /* OPTIONAL */ void* arg);

typedef void (*je_job_for_worlds_t)(je_game_world_t* /*world*/, void* /*custom_data*/);
typedef void (*je_job_call_once_t)(void* /*custom_data*/);

/*
je_ecs_universe_pre_for_worlds_job_register [基本接口]
向指定宇宙中注册优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_pre_for_worlds_job_register(
    je_game_universe_t* universe,
    je_job_for_worlds_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_pre_call_once_job_register [基本接口]
向指定宇宙中注册优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_pre_call_once_job_register(
    je_game_universe_t* universe,
    je_job_call_once_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_for_worlds_job_register [基本接口]
向指定宇宙中注册普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_for_worlds_job_register(
    je_game_universe_t* universe,
    je_job_for_worlds_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_call_once_job_register [基本接口]
向指定宇宙中注册普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_call_once_job_register(
    je_game_universe_t* universe,
    je_job_call_once_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_after_for_worlds_job_register [基本接口]
向指定宇宙中注册延后遍历世界任务（Defer job for worlds）
*/
JE_API void je_ecs_universe_after_for_worlds_job_register(
    je_game_universe_t* universe,
    je_job_for_worlds_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_after_call_once_job_register [基本接口]
向指定宇宙中注册延后单独任务（Defer job for once）
*/
JE_API void je_ecs_universe_after_call_once_job_register(
    je_game_universe_t* universe,
    je_job_call_once_t job,
    /* OPTIONAL */ void* data,
    /* OPTIONAL */ void (*freefunc)(void*));

/*
je_ecs_universe_pre_for_worlds_job_unregister [基本接口]
从指定宇宙中取消优先遍历世界任务（Pre job for worlds）
*/
JE_API void je_ecs_universe_pre_for_worlds_job_unregister(
    je_game_universe_t* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_pre_call_once_job_unregister [基本接口]
从指定宇宙中取消优先单独任务（Pre job for once）
*/
JE_API void je_ecs_universe_pre_call_once_job_unregister(
    je_game_universe_t* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_for_worlds_job_unregister [基本接口]
从指定宇宙中取消普通遍历世界任务（Job for worlds）
*/
JE_API void je_ecs_universe_for_worlds_job_unregister(
    je_game_universe_t* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_call_once_job_unregister [基本接口]
从指定宇宙中取消普通单独任务（Job for once）
*/
JE_API void je_ecs_universe_call_once_job_unregister(
    je_game_universe_t* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_after_for_worlds_job_unregister [基本接口]
从指定宇宙中取消延后遍历世界任务（After job for worlds）
*/
JE_API void je_ecs_universe_after_for_worlds_job_unregister(
    je_game_universe_t* universe,
    je_job_for_worlds_t job);

/*
je_ecs_universe_after_call_once_job_unregister [基本接口]
从指定宇宙中取消延后单独任务（After job for once）
*/
JE_API void je_ecs_universe_after_call_once_job_unregister(
    je_game_universe_t* universe,
    je_job_call_once_t job);

/*
je_ecs_universe_frame_deltatime_get [基本接口]
获取当前宇宙的期待的帧更新间隔时间，默认为 1.0/60.0
请参见：
    je_ecs_universe_set_deltatime
*/
JE_API double je_ecs_universe_frame_deltatime_get(
    je_game_universe_t* universe);

/*
je_ecs_universe_frame_deltatime_set [基本接口]
设置当前宇宙的帧更新间隔时间
*/
JE_API void je_ecs_universe_frame_deltatime_set(
    je_game_universe_t* universe,
    double delta);

/*
je_ecs_universe_real_deltatime_get [基本接口]
获取当前宇宙的实际更新间隔，即距离上次更新的实际时间差异
*/
JE_API double je_ecs_universe_real_deltatime_get(je_game_universe_t* universe);

/*
je_ecs_universe_smooth_deltatime_get [基本接口]
获取当前宇宙的平滑更新间隔，是过去若干帧的间隔平均值
*/
JE_API double je_ecs_universe_smooth_deltatime_get(je_game_universe_t* universe);

/*
je_ecs_universe_max_deltatime_get [基本接口]
获取当前宇宙的最大时间间隔，deltatime的最大值即为此值
*/
JE_API double je_ecs_universe_max_deltatime_get(je_game_universe_t* universe);

/*
je_ecs_universe_max_deltatime_set [基本接口]
设置当前宇宙的最大时间间隔，deltatime的最大值即为此值
    * 此值仅约束缩放之前的deltatime
请参见：
    je_ecs_universe_time_scale_set
*/
JE_API void je_ecs_universe_max_deltatime_set(je_game_universe_t* universe, double val);

/*
je_ecs_universe_time_scale_set [基本接口]
设置当前宇宙的时间缩放系数
*/
JE_API void je_ecs_universe_time_scale_set(je_game_universe_t* universe, double scale);

/*
je_ecs_universe_time_scale_get [基本接口]
获取当前宇宙的时间缩放系数
*/
JE_API double je_ecs_universe_time_scale_get(je_game_universe_t* universe);

/*
je_ecs_world_in_universe [基本接口]
获取指定世界所属的宇宙
*/
JE_API je_game_universe_t* je_ecs_world_in_universe(je_game_world_t* world);

/*
je_ecs_world_create [基本接口]
在指定的宇宙中创建一个世界
    * 世界在创建之后，默认为非激活状态，请在初始化操作完成后调用
        je_ecs_world_enable_set 将世界激活
请参见：
    je_ecs_world_enable_set
*/
JE_API je_game_world_t* je_ecs_world_create(je_game_universe_t* in_universe);

/*
je_ecs_world_destroy [基本接口]
从指定的宇宙中销毁一个世界
    * 销毁世界不会立即生效，而是要等到下一次逻辑更新
    * 世界销毁时，会按照如下顺序执行所有销毁操作：
        0. 被标记为即将销毁
        1. 销毁所有系统
        2. 销毁所有实体
        3. 执行最后命令缓冲区更新
        4. 被宇宙从世界列表中移除
    * 向正在销毁中的世界创建实体或添加系统仍会成功，
        但它们会在下一次世界更新时随世界一并销毁
请参见：
    je_ecs_world_system_instance_add
    je_ecs_world_entity_create_with_components
*/
JE_API void je_ecs_world_destroy(je_game_world_t* world);

typedef enum je_component_requirement_kind_t
{
    JE_COMPONENT_REQUIRE_CONTAINS,  // Must have spcify component
    JE_COMPONENT_REQUIRE_MAYNOT,    // May have or not have
    JE_COMPONENT_REQUIRE_EXCEPT,    // Must not contain spcify component
    JE_COMPONENT_REQUIRE_ANYOF_0,   // Must have one of 'ANYOF' components

}je_component_requirement_kind_t;

typedef struct je_component_requirement_t {
    int /*je_component_requirement_kind_t*/ m_kind;
    je_type_id_t                           m_typeid;
}je_component_requirement_t;

typedef struct je_dependence_arch_infos_t
{
    je_archtype_t* m_arch;
    size_t  m_entity_count;

    // View detail.
    size_t* m_view_component_offset;
    size_t* m_view_component_size;

} je_dependence_arch_infos_t;

typedef struct je_collected_requirements_t je_collected_requirements_t;

typedef struct je_requirement_collection_t {
    je_dependence_arch_infos_t* m_cached_archs;
    size_t                      m_cached_arch_count;

    je_collected_requirements_t* m_collected_requirement;
}je_requirement_collection_t;

/*
je_ecs_requirements_collect [基本接口]
把一组组件需求整理为供切片查询使用的紧凑需求对象
    * requirements 的前 view_requiremnt_count 项作为视图需求，其余作为附加需求
    * requirements 为 nullptr 仅当总需求数为 0
请参见：
    je_ecs_world_collection_update
    je_ecs_world_slice_dependence_query
*/
JE_API je_collected_requirements_t* je_ecs_requirements_collect(
    /* OPTIONAL: If total requirement count is 0 */ const je_component_requirement_t* requirements,
    size_t view_requiremnt_count,
    size_t other_requiremnts_count);

/*
je_ecs_requirements_free [基本接口]
释放由 je_ecs_requirements_collect 生成的需求对象
请参见：
    je_ecs_requirements_collect
*/
JE_API void je_ecs_requirements_free(je_collected_requirements_t* requirements);

/*
je_ecs_world_collection_update [基本接口]
按世界当前的 arch-type 状态刷新需求集合的切片查询缓存
    * 通常配合 je_ecs_world_arch_change_version_get 判断是否需要刷新
    * collection 中的 m_collected_requirement 应当已经由 je_ecs_requirements_collect 填充
请参见：
    je_ecs_requirements_collect
    je_ecs_world_arch_change_version_get
*/
JE_API void je_ecs_world_collection_update(
    je_game_world_t* world,
    je_requirement_collection_t* collection);

/*
je_ecs_world_arch_change_version_get [基本接口]
返回世界内 arch-type 的变更版本号（单调递增）。每当世界中创建了新的 arch-type 时
该值递增。此函数为非消费式读取：调用不会影响 ecs_world::update 对内部系统切片
缓存的刷新逻辑。外部持久化的 je_requirement_collection_t 可通过比较两次观测值来
判断是否需要刷新（调用 je_ecs_world_collection_update）。
*/
JE_API size_t je_ecs_world_arch_change_version_get(je_game_world_t* world);

typedef void* je_system_t;

/*
je_ecs_world_system_instance_add [基本接口]
向指定世界中添加一个指定类型的系统实例，返回此实例的指针
每次更新时，一帧内`最后`执行的操作将会生效，如果生效的是添加系统操作，那么：
    1. 若此前世界中不存在同类型的系统，则添加
    2. 若此前世界中已经存在同类型系统，则替换

    * 若给定的类型不是一个系统类型，返回 nullptr
    * 向正在销毁中的世界添加系统实例仍会返回有效实例，
        但该系统会在下一次世界更新时随世界一并销毁，永远不会执行
*/
JE_API /* OPTIONAL */ je_system_t je_ecs_world_system_instance_add(
    je_game_world_t* world,
    je_type_id_t type);

/*
je_ecs_world_system_instance_get [基本接口]
从指定世界中获取一个指定类型的系统实例，返回此实例的指针
若世界中不存在此类型的系统，返回nullptr
*/
JE_API /* OPTIONAL */ je_system_t je_ecs_world_system_instance_get(
    je_game_world_t* world,
    je_type_id_t type);

/*
je_ecs_world_system_instance_remove [基本接口]
从指定世界中移除一个指定类型的系统实例
每次更新时，一帧内`最后`执行的操作将会生效，如果生效的是移除系统操作，那么：
    1. 若此前世界中不存在同类型的系统，则移除
    2. 若此前世界中已经存在同类型系统，则无事发生
*/
JE_API void je_ecs_world_system_instance_remove(
    je_game_world_t* world,
    je_type_id_t type);

/*
je_ecs_world_entity_create_with_components [基本接口]
向指定世界中创建一个用于指定组件集合的实体，创建结果通过参数 out_entity 返回
component_ids 应该指向一个储存有N个je_type_id_t实例的连续空间，
其中，N是组件种类数量（component_count）且不应该为0。
    * 世界销毁是异步的：向已提交销毁请求（正在销毁中）的世界创建实体仍会成功，
        但该实体会在下一次世界更新时随世界一并销毁；
请参见：
    je_type_id_t
    je_game_entity_t
    jeecs::game_world::add_entity
*/
JE_API void je_ecs_world_entity_create_with_components(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_type_id_t* component_ids,
    size_t component_count);

/*
je_ecs_world_prefab_create_with_components [基本接口]
向指定世界中创建一个用于指定组件集合的预设体，预设体的行为类似实体但不会被选择器获取，
也不能添加或者删除组件。
    * 预设体被用于快速实例化对象
请参见：
    je_ecs_world_entity_create_with_components
*/
JE_API void je_ecs_world_prefab_create_with_components(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_type_id_t* component_ids,
    size_t component_count);

/*
je_ecs_world_entity_create_with_prefab [基本接口]
向指定世界中创建一个用于指定组件集合的实体，该实体的组件及数据由指定的原件实体决定，
    * 原件实体可以由 je_ecs_world_entity_create_with_components，
        je_ecs_world_prefab_create_with_components，或
        je_ecs_world_entity_create_with_prefab 创建
    * 用于快速实例化对象
请参见：
    je_ecs_world_entity_create_with_components
    je_ecs_world_prefab_create_with_components
*/
JE_API void je_ecs_world_entity_create_with_prefab(
    je_game_world_t* world,
    je_game_entity_t* out_entity,
    const je_game_entity_t* prefab);

/*
je_ecs_world_entity_destroy [基本接口]
从世界中销毁一个实体索引指定的相关组件
若实体索引是`无效值`或已失效，则无事发生
请参见：
    je_game_entity_t::close
*/
JE_API void je_ecs_world_entity_destroy(
    je_game_world_t* world,
    const je_game_entity_t* entity);

/*
je_ecs_world_entity_component_add [基本接口]
向实体中添加一个组件，无论如何，总是返回一个新的组件实例的地址
若实体索引是`无效值`或已失效，则最终无事发生
引擎对于增加或移除组件的操作遵循最后生效原则，即每次更新时，针对一个特定组件类型一帧
内`最后`执行的操作将会生效，如果生效的是添加组件操作，那么：
    1. 若实体已经存在同类型组件，则替换之
    2. 若实体不存在同类型组件，则更新实体
请参见：
    je_game_entity_t::add_component
*/
JE_API /* OPTIONAL: If entity is invalid */ void* je_ecs_world_entity_component_add(
    const je_game_entity_t* entity,
    je_type_id_t type);

/*
je_ecs_world_entity_component_remove [基本接口]
从实体中移除一个组件
若实体索引是`无效值`或已失效，则最终无事发生
引擎对于增加或移除组件的操作遵循最后生效原则，即每次更新时，针对一个特定组件类型一帧
内`最后`执行的操作将会生效，如果生效的是移除组件操作，那么：
    1. 若实体已经存在同类型组件，则移除之
    2. 若实体不存在同类型组件，则无事发生
请参见：
    je_game_entity_t::remove_component
*/
JE_API void je_ecs_world_entity_component_remove(
    const je_game_entity_t* entity,
    je_type_id_t type);

/*
je_ecs_world_entity_component_get [基本接口]
从实体中获取一个组件
若实体索引是`无效值`或已失效，或者实体不存在指定类型的组件，则返回nullptr
请参见：
    je_game_entity_t::get_component
*/
JE_API /* OPTIONAL: If entity is invalid or no such component */ void* je_ecs_world_entity_component_get(
    const je_game_entity_t* entity,
    je_type_id_t type);

/*
je_ecs_world_of_entity [基本接口]
获取实体所在的世界
若实体索引是`无效值`，则返回nullptr
请参见：
    je_game_entity_t::game_world
*/
JE_API /* OPTIONAL: If entity is invalid */ je_game_world_t* je_ecs_world_of_entity(
    const je_game_entity_t* entity);

/*
je_ecs_world_enable_set [基本接口]
设置世界是否被激活
    * 若世界未激活，则实体组件系统更新将被暂停，世界任务亦将被跳过，仅响应
        世界销毁请求和激活世界请求
    * 世界在激活/取消激活时，所有系统的对应回调会被执行；如果系统实例创建时，
        世界尚未激活，则系统的回调函数不会被执行
*/
JE_API void je_ecs_world_enable_set(je_game_world_t* world, bool enable);

/*
je_ecs_world_slice_dependence_query [基本接口]
根据指定的系统实例和切片类型，获取切片查询缓存
  * 需要检查返回值，如果返回 false，应当由调用方初始化切片查询缓存的 requirements 字段并执行更新
  * 获取到的查询缓存仅在当前帧有效，不应当被持久化保存
*/
JE_API bool je_ecs_world_slice_dependence_query(
    je_game_world_t* world,
    jeecs::game_system* system_instance,
    je_type_hash_t slice_type_hash,
    je_requirement_collection_t** out_collection);

// ATTENTION: Following 2 functions have no thread-safe-promise:
/*
je_ecs_entity_name_get [基本接口]
获取实体的名称，一般只用于调试使用，不建议使用在实际项目中
若实体不包含 Editor::Name 或实体索引是`无效值`或已失效，则返回空字符串
从实体中取出的名称必须立即复制到其他位置或使用完毕，此函数直接
返回 Editor::Name 组件中的字符串地址，这意味着如果实体发生更新、重命名
或者其他操作，取出的字符串可能失效
*/
JE_API const char* je_ecs_entity_name_get(
    const je_game_entity_t* entity);

/*
je_ecs_entity_name_set [基本接口]
设置实体的名称，一般只用于调试使用，不建议使用在实际项目中
若实体索引是`无效值`或已失效，则返回空字符串，否则返回设置完成之后的新名称
实体的名称被储存在内置组件 Editor::Name 中，这意味着给实体命名将可能
使得实体索引失效
另外，从实体中取出的名称必须立即复制到其他位置或使用完毕，此函数直接
返回 Editor::Name 组件中的字符串地址，这意味着如果实体发生更新、重命名
或者其他操作，取出的字符串可能失效
若实体不包含 Editor::Name，则创建后再进行设置
*/
JE_API const char* je_ecs_entity_name_set(
    const je_game_entity_t* entity,
    const char* name);
typedef je_game_universe_t je_GameUniverse;
typedef je_game_world_t je_GameWorld;
typedef je_archtype_t je_Archtype;
typedef je_chunk_t je_Chunk;
typedef je_entity_id_in_chunk_t je_EntityIdInChunk;
typedef je_entity_stat_t je_EntityStat;
typedef je_game_entity_meta_t je_GameEntityMeta;
typedef je_game_entity_t je_GameEntity;
typedef je_job_for_worlds_t je_JobForWorlds;
typedef je_job_call_once_t je_JobCallOnce;
typedef je_component_requirement_kind_t je_ComponentRequirementKind;
typedef je_component_requirement_t je_ComponentRequirement;
typedef je_dependence_arch_infos_t je_DependenceArchInfos;
typedef je_collected_requirements_t je_CollectedRequirements;
typedef je_requirement_collection_t je_RequirementCollection;
typedef je_system_t je_System;

JE_FORCE_CAPI_END
