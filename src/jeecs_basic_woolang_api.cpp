#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/clock.hpp"
#include "jeecs/core.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/module.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/version.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

struct _jewo_log_channel
{
    atomic_flag                         m_mx;
    je_log_regid_t                      m_fact_log_id;
    list<pair<je_log_level_t, string>>  m_buffer;
};

WOORT_API woort_api wojeapi_get_current_platform(void)
{
    return woort_ret_int(JE4_CURRENT_PLATFORM);
}

WOORT_API woort_api wojeapi_generate_uid(void)
{
    typing::uid_t::generate().JEParseToScriptType(WOORT_RETURN_SLOT);
    return woort_ret();
}

WOORT_API woort_api wojeapi_build_version(void)
{
#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C
    return woort_ret_string(JE_CORE_VERSION);
#undef JE_VERSION_WRAP
}

WOORT_API woort_api wojeapi_build_version_info(void)
{
    return woort_ret_string(je_build_version());
}

WOORT_API woort_api wojeapi_build_commit(void)
{
    return woort_ret_string(je_build_commit());
}

WOORT_API woort_api wojeapi_get_sleep_suppression(void)
{
    return woort_ret_real(je_clock_sleep_suppression_get());
}

// Runtime path
WOORT_API woort_api wojeapi_get_runtime_path(void)
{
    return woort_ret_string(je_file_runtime_path_get());
}

WOORT_API woort_api wojeapi_set_runtime_path(void)
{
    je_file_runtime_path_set(woort_string(0));
    return woort_ret_void();
}

// Sleep suppression & toolchain versions & crc
WOORT_API woort_api wojeapi_set_sleep_suppression(void)
{
    je_clock_sleep_suppression_set(woort_real(0));
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_woolang_version(void)
{
    return woort_ret_string(wo_version());
}

WOORT_API woort_api wojeapi_woort_version(void)
{
    return woort_ret_string(woort_version());
}

WOORT_API woort_api wojeapi_crc64_file(void)
{
    const uint64_t result = wo_crc64_file_from_path(woort_string(0));
    if (result)
        return woort_ret_option_int(result);
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_crc64_string(void)
{
    const uint64_t result = wo_crc64_str(woort_string(0));
    return woort_ret_int(result);
}

WOORT_API woort_api wojeapi_register_log_callback(void)
{
    auto* channel = new _jewo_log_channel();
    channel->m_mx.clear();

    channel->m_fact_log_id = je_log_callback_register(
        [](je_log_level_t level, const char* msg, void* userdata)
        {
            auto* const channel = static_cast<_jewo_log_channel*>(userdata);
            while (channel->m_mx.test_and_set());
            do
            {
                channel->m_buffer.push_back({ level, msg });

            } while (0);
            channel->m_mx.clear();
        },
        channel);

    return woort_ret_pointer(channel);
}

WOORT_API woort_api wojeapi_unregister_log_callback(void)
{
    auto* const channel = static_cast<_jewo_log_channel*>(woort_pointer(0));
    (void)je_log_callback_unregister(channel->m_fact_log_id);

    delete channel;

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_get_all_logs(void)
{
    auto* const channel = static_cast<_jewo_log_channel*>(woort_pointer(0));

    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    woort_set_vec(result);

    list<pair<je_log_level_t, string>> logs;

    while (channel->m_mx.test_and_set());
    do
    {
        logs.swap(channel->m_buffer);
    } while (0);
    channel->m_mx.clear();

    for (auto& [i, s] : logs)
    {
        woort_set_struct(elem, 2);

        woort_struct_set_int(elem, 0, (int)i);
        woort_struct_set_string(elem, 1, s.c_str());

        woort_vec_push(result, elem);
    }
    return woort_ret_value(result);
}

// Native module load/unload
WOORT_API woort_api wojeapi_load_module(void)
{
    auto* const dylib_handle = je_module_load(woort_string(0), woort_string(1));

    if (dylib_handle == nullptr)
        return woort_ret_option_none();

    return woort_ret_option_pointer(dylib_handle);
}

WOORT_API woort_api wojeapi_unload_module(void)
{
    je_module_unload(static_cast<woort_Dylib*>(woort_pointer(0)));
    return woort_ret_void();
}

// LOG
static string _wojeapi_log_string(void)
{
    const size_t argc = (size_t)woort_int(0);
    string disp;

    for (size_t i = 0; i < argc; i++)
    {
        if (woort_unbox_type(i + 1) == WOORT_BOX_VALUE_TYPE_STRING)
            disp += woort_string(i + 1);
        else
        {
            char* str = woort_serialize_dynbox(i + 1, WOORT_SERIALIZE_FLAG_NONE);
            disp += str;
            woort_free(str);
        }
    }
    return disp;
}

WOORT_API woort_api wojeapi_log(void)
{
    debug::log("%s", _wojeapi_log_string().c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_loginfo(void)
{
    debug::loginfo("%s", _wojeapi_log_string().c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logwarn(void)
{
    debug::logwarn("%s", _wojeapi_log_string().c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logerr(void)
{
    debug::logerr("%s", _wojeapi_log_string().c_str());
    return woort_ret_void();
}

WOORT_API woort_api wojeapi_logfatal(void)
{
    debug::logfatal("%s", _wojeapi_log_string().c_str());
    return woort_ret_void();
}
