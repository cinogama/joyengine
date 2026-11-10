#pragma once

// ============================================================================
// jeecs/atomic.hpp —— 原子操作：je_atomic_* 宏生成 C API（JE_DECL_ATOMIC_OPERATOR_API）
// =============================================================================

#include "jeecs/prefix.hpp"

JE_FORCE_CAPI

// Atomic operator API
#define JE_DECL_ATOMIC_OPERATOR_API(TYPE)                                    \
    JE_API TYPE je_atomic_exchange_##TYPE(TYPE *aim, TYPE value);            \
    JE_API bool je_atomic_cas_##TYPE(TYPE *aim, TYPE *comparer, TYPE value); \
    JE_API TYPE je_atomic_fetch_add_##TYPE(TYPE *aim, TYPE value);           \
    JE_API TYPE je_atomic_fetch_sub_##TYPE(TYPE *aim, TYPE value);           \
    JE_API TYPE je_atomic_fetch_##TYPE(TYPE *aim);                           \
    JE_API void je_atomic_store_##TYPE(TYPE *aim, TYPE value)

JE_DECL_ATOMIC_OPERATOR_API(int8_t);
JE_DECL_ATOMIC_OPERATOR_API(uint8_t);
JE_DECL_ATOMIC_OPERATOR_API(int16_t);
JE_DECL_ATOMIC_OPERATOR_API(uint16_t);
JE_DECL_ATOMIC_OPERATOR_API(int32_t);
JE_DECL_ATOMIC_OPERATOR_API(uint32_t);
JE_DECL_ATOMIC_OPERATOR_API(int64_t);
JE_DECL_ATOMIC_OPERATOR_API(uint64_t);
JE_DECL_ATOMIC_OPERATOR_API(size_t);
JE_DECL_ATOMIC_OPERATOR_API(intptr_t);

#undef JE_DECL_ATOMIC_OPERATOR_API

JE_FORCE_CAPI_END
