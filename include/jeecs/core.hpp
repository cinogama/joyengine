#pragma once

// ============================================================================
// jeecs/core.hpp —— 核心：内存（je_mem_*）、引擎初始化（je_core_*）、构建信息（je_build_*）与入口脚本（je_core_main_script_entry）
// =============================================================================

#include "jeecs/prefix.hpp"

// 基础标量类型（实体索引版本、图形线程版本等场景共用）
typedef uint32_t je_version_t;      // 版本号（实体索引版本、图形线程版本等）
JE_FORCE_CAPI

/*
je_mem_alloc [基本接口]
引擎的统一内存申请函数。
*/
JE_API /* OPTIONAL: If allocation fails */ void* je_mem_alloc(size_t sz);

/*
je_mem_realloc [基本接口]
引擎的统一内存重申请函数。
*/
JE_API /* OPTIONAL: If allocation fails */ void* je_mem_realloc(/* OPTIONAL */ void* mem, size_t sz);

/*
je_mem_free [基本接口]
引擎的统一内存释放函数。
*/
JE_API void je_mem_free(/* OPTIONAL */ void* ptr);

/*
je_core_init [基本接口]
引擎的基本初始化操作，应当在所有其他引擎相关的操作之前调用
参数为应用程序启动时的命令行参数，若不希望提供，可以传入：
    argc = 0, argv = nullptr
请参见：
    je_core_finish
*/
JE_API void je_core_init(int argc, /* OPTIONAL: If argc == 0 */ char** argv);

/*
je_core_finish [基本接口]
引擎的退出操作，应当在所有其他引擎相关的操作之后调用
    * 引擎退出完毕之后可以重新调用 je_core_init 重新启动
请参见：
    je_core_init
*/
JE_API void je_core_finish(void);

/*
je_build_version [基本接口]
用于获取引擎编译时信息
*/
JE_API const char* je_build_version();

/*
je_build_commit [基本接口]
用于获取当前引擎所在的提交哈希
* 若使用的是手动构建的引擎，此函数将返回 "untracked"
*/
JE_API const char* je_build_commit();
/*
je_core_main_script_entry [基本接口]
运行入口脚本
    * 阻塞直到入口脚本运行完毕
    * 先尝试加载游戏入口 @/builtin/main.wo（不带缓存）
    * 若游戏入口不存在，则带缓存地加载编辑器入口 @/builtin/editor/main.wo
*/
JE_API bool je_core_main_script_entry();
typedef je_version_t je_Version;

JE_FORCE_CAPI_END
