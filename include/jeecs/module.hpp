#pragma once

// ============================================================================
// jeecs/module.hpp —— 模块加载：je_module_* 动态库 C API
// =============================================================================

#include "jeecs/prefix.hpp"

JE_FORCE_CAPI

// Library / Module loader

/*
je_module_load [基本接口]
以name为名字，加载指定路径的动态库（遵循woolang规则）加载失败返回nullptr
*/
JE_API /* OPTIONAL: If load failed */ woort_Dylib* je_module_load(const char* name, const char* path);

/*
je_module_func [基本接口]
从动态库中加载某个函数，返回函数的地址
*/
JE_API /* OPTIONAL: If symbol not found */ void* je_module_func(woort_Dylib* lib, const char* funcname);

/*
je_module_unload [基本接口]
立即卸载某个动态库
*/
JE_API void je_module_unload(woort_Dylib* lib);

JE_FORCE_CAPI_END
