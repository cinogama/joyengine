#pragma once

// ============================================================================
// jeecs_basic_type_mgr.hpp —— 类型管理器的内部接口
//
// 公共的注册/查询走 je_typing_* C API（include/jeecs/typing.hpp）；
// 此处只放引擎内部（towoo 内核等）使用的补充入口。实现位于
// jeecs_basic_type_mgr.cpp。
// ============================================================================

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/typing.hpp"

namespace jeecs::typing
{
    // 重置指定类型的大小、对齐与构造/析构函数（原 je_typing_reset C API
    // 的内部化版本，towoo 内核在引擎关闭/重启周期里复用类型槽位时使用）。
    // 成员字段将被重置，请重新注册。
    void reset_type(
        const je_type_info_t* tinfo,
        size_t size,
        size_t align,
        je_construct_func_t constructor,
        je_destruct_func_t destructor,
        je_copy_construct_func_t copy_constructor,
        je_move_construct_func_t move_constructor);
}
