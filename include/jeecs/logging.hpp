#pragma once

// ============================================================================
// jeecs/logging.hpp —— 日志：je_log_* C API 与 jeecs::debug C++ 日志工具
// =============================================================================

#include "jeecs/prefix.hpp"

typedef uint64_t je_debug_eid_t;     // 调试用实体 ID（仅编辑器）
JE_FORCE_CAPI

/*
je_log_level_t [枚举类型]
日志等级，各等级取值固定为：
    JE_LOG_NORMAL  —— 通常（0）
    JE_LOG_INFO    —— 信息（1）
    JE_LOG_WARNING —— 警告（2）
    JE_LOG_ERROR   —— 错误（3）
    JE_LOG_FATAL   —— 致命错误（4）
请参见：
    je_log
*/
typedef enum je_log_level_t
{
    JE_LOG_NORMAL,
    JE_LOG_INFO,
    JE_LOG_WARNING,
    JE_LOG_ERROR,
    JE_LOG_FATAL,
} je_log_level_t;

typedef size_t je_log_regid_t;
typedef void (*je_log_callback_t)(je_log_level_t level, const char* msg, void* userdata);

/*
je_log_callback_register [基本接口]
用于注册一个日志发生时的回调函数，返回注册句柄。
回调在日志派发线程上按注册顺序被调用；
msg 缓冲仅在回调返回前有效，需要保留请自行拷贝。
请参见：
    je_log_callback_unregister
*/
JE_API je_log_regid_t je_log_callback_register(je_log_callback_t callback, /* OPTIONAL */ void* userdata);

/*
je_log_callback_unregister [基本接口]
用于释放（解除注册）一个之前注册的日志发生时回调函数，返回该回调注册时的 userdata。
*/
JE_API /* OPTIONAL: If registered userdata was null */ void* je_log_callback_unregister(je_log_regid_t regid);

/*
je_log [基本接口]
以指定的日志等级，向日志系统提交一条日志。
消息文本会自动加上 [时:分:秒.毫秒] 时间戳前缀，随后非阻塞入队，
由日志派发线程依次调用回调并输出到控制台（Windows 下同时输出到调试器）；
引擎 bootup 之前 / shutdown 之后的日志会在调用线程上直接派发。
*/
JE_API void je_log(je_log_level_t level, const char* format, ...);
typedef je_debug_eid_t je_DebugEid;
typedef je_log_level_t je_LogLevel;
typedef je_log_regid_t je_LogRegid;
typedef je_log_callback_t je_LogCallback;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::debug [命名空间]
    此处包含用于调试的工具类或工具函数
    */
    namespace debug
    {
        /*
        jeecs::debug::log [函数]
        用于产生一般日志
        */
        template <typename... ArgTs>
        inline void log(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }

        /*
        jeecs::debug::loginfo [函数]
        用于产生信息日志
        */
        template <typename... ArgTs>
        inline void loginfo(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }

        /*
        jeecs::debug::logwarn [函数]
        用于产生警告日志
        */
        template <typename... ArgTs>
        inline void logwarn(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }

        /*
        jeecs::debug::logerr [函数]
        用于产生错误日志
        */
        template <typename... ArgTs>
        inline void logerr(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }

        /*
        jeecs::debug::logfatal [函数]
        用于产生致命错误日志
        */
        template <typename... ArgTs>
        inline void logfatal(const char* format, ArgTs &&...args)
        {
            je_log(JE_LOG_FATAL, format, args...);
            woort_panic(WO_FAIL_JE_FATAL_ERROR, format, args...);
        }
    }
}
#endif // __cplusplus
