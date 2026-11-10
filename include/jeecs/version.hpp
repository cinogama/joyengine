#pragma once

// ============================================================================
// jeecs/version.hpp —— 引擎核心版本号（.rc 与各入口点使用：先定义 JE_VERSION_WRAP 再包含）
// =============================================================================


// 使用方需自行定义 JE_VERSION_WRAP(A, B, C) 后再使用 JE_CORE_VERSION。
#define JE_CORE_VERSION JE_VERSION_WRAP(4, 9, 1)
