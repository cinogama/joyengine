#pragma once

// ============================================================================
// jeecs/clock.hpp —— 时钟：je_clock_* 时间与睡眠接口
// =============================================================================

#include "jeecs/prefix.hpp"

typedef uint64_t je_timestamp_ms_t;  // 毫秒时间戳
JE_FORCE_CAPI

/////////////////////////// Time&Sleep /////////////////////////////////

/*
je_clock_sleep_suppression_get [基本接口]
获取引擎的提前唤醒间隔，单位是秒
此值影响sleep操作，将会减少sleep的实际时间，这有助于提升画面的流畅度，
但会增加CPU空转导致的开销（虽然不影响实际性能）
请参见：
    je_clock_sleep_suppression_set
*/
JE_API double je_clock_sleep_suppression_get();

/*
je_clock_sleep_suppression_set [基本接口]
设置引擎的提前唤醒间隔，单位是秒
此值影响sleep操作，将会减少sleep的实际时间，这有助于提升画面的流畅度，
但会增加CPU空转导致的开销（虽然不影响实际性能）
请参见：
    je_clock_sleep_suppression_get
*/
JE_API void je_clock_sleep_suppression_set(double v);

/*
je_clock_time [基本接口]
获取引擎的运行时间戳，单位是秒
获取到的时间是引擎自启动到当前为止的时间
*/
JE_API double je_clock_time();

/*
je_clock_time_stamp [基本接口]
获取当前时间戳，单位是毫秒
    * 这个时间与 je_clock_time 获取到的时间不同，*不是* 引擎启动时起的计时
*/
JE_API je_timestamp_ms_t je_clock_time_stamp();

/*
je_clock_sleep_until [基本接口]
挂起当前线程直到指定时间点
*/
JE_API void je_clock_sleep_until(double time);

/*
je_clock_sleep_for [基本接口]
挂起当前线程若干秒
*/
JE_API void je_clock_sleep_for(double time);
typedef je_timestamp_ms_t je_TimestampMs;

JE_FORCE_CAPI_END
