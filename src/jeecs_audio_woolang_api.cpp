#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/audio.hpp"
#include "jeecs/basic.hpp"
#include "jeecs/file.hpp"
#include "jeecs/prefix.hpp"

#include "wo.h"

#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace jeecs;

WOORT_API woort_api wojeapi_audio_buffer_load(void)
{
    auto buffer = audio::buffer::load(woort_string(0));
    if (buffer)
    {
        return woort_ret_option_gchandle(
            new basic::resource<audio::buffer>(buffer.value()),
            WOORT_IGNORE,
            [](void* p)
            {
                delete static_cast<basic::resource<audio::buffer>*>(p);
            },
            nullptr);
    }
    return woort_ret_option_none();
}

WOORT_API woort_api wojeapi_audio_buffer_info(void)
{
    basic::resource<audio::buffer>* buffer =
        static_cast<basic::resource<audio::buffer> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 6);
    auto* buffer_instance = (*buffer)->handle();

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)buffer_instance->m_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 1, (woort_Int)buffer_instance->m_sample_rate);
    woort_struct_set_int(WOORT_RETURN_SLOT, 2, (woort_Int)buffer_instance->m_sample_size);
    woort_struct_set_int(WOORT_RETURN_SLOT, 3, (woort_Int)buffer_instance->m_byte_rate);
    woort_struct_set_int(WOORT_RETURN_SLOT, 4, (woort_Int)buffer_instance->m_format);
    woort_struct_set_bool(WOORT_RETURN_SLOT, 5, buffer_instance->m_streaming);

    return woort_ret();
}

// 确保 path 指向的音频已有可用的缓存 <path>.je4cache（裸 PCM 区 +
// 解码信息文档 + 可选 <path>.je4audio 的流式播放配置）；供导出流程
// 预生成缓存使用，引擎加载音频时走同一流程。
WOORT_API woort_api wojeapi_audio_cache_ensure(void)
{
    const string path = woort_string(0);

    je_file_t* cache = je_audio_cache_open_or_create(path.c_str());
    if (cache != nullptr)
    {
        je_file_close(cache);
        return woort_ret_bool(true);
    }

    return woort_ret_bool(false);
}

/*
extern("libjoyecs", "wojeapi_audio_filter_create")
    public func create()=> filter;

extern("libjoyecs", "wojeapi_audio_filter_info")
    public func info(self: filter)=> filter_data_t;

extern("libjoyecs", "wojeapi_audio_filter_update")
    func _update(self: filter, info: filter_data_t)=> void;

*/
WOORT_API woort_api wojeapi_audio_filter_create(void)
{
    return woort_ret_gchandle(
        new basic::resource<audio::filter>(audio::filter::create()),
        WOORT_IGNORE,
        [](void* p)
        {
            delete static_cast<basic::resource<audio::filter>*>(p);
        },
        nullptr);
}

WOORT_API woort_api wojeapi_audio_filter_info(void)
{
    basic::resource<audio::filter>* filter =
        static_cast<basic::resource<audio::filter> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 4);

    auto* filter_instance = (*filter)->handle();

    woort_struct_set_int(WOORT_RETURN_SLOT, 0, (woort_Int)filter_instance->m_type);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, filter_instance->m_gain);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, filter_instance->m_gain_lf);
    woort_struct_set_float(WOORT_RETURN_SLOT, 3, filter_instance->m_gain_hf);

    return woort_ret();
}

WOORT_API woort_api wojeapi_audio_filter_update(void)
{
    basic::resource<audio::filter>* filter =
        static_cast<basic::resource<audio::filter> *>(woort_gcpointer(0));
    const woort_value updated_info = 1;

    (*filter)->update([&](je_audio_filter_t* flt)
        {
            flt->m_type = (je_audio_filter_type_t)woort_struct_get_int(updated_info, 0);
            flt->m_gain = woort_struct_get_float(updated_info, 1);
            flt->m_gain_lf = woort_struct_get_float(updated_info, 2);
            flt->m_gain_hf = woort_struct_get_float(updated_info, 3); });

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_create(void)
{
    return woort_ret_gchandle(
        new basic::resource<audio::source>(audio::source::create()),
        WOORT_IGNORE,
        [](void* p)
        {
            delete static_cast<basic::resource<audio::source>*>(p);
        },
        nullptr);
}

WOORT_API woort_api wojeapi_audio_source_info(void)
{
    woort_value elem;
    if (!woort_push_reserve(1, &elem))
        return woort_ret_panic("Stack overflow.");

    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    woort_set_struct(WOORT_RETURN_SLOT, 15);

    auto* source_instance = (*source)->handle();

    woort_struct_set_bool(WOORT_RETURN_SLOT, 0, source_instance->m_loop);
    woort_struct_set_float(WOORT_RETURN_SLOT, 1, source_instance->m_gain);
    woort_struct_set_float(WOORT_RETURN_SLOT, 2, source_instance->m_pitch);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, source_instance->m_location[i]);
    }
    woort_struct_set(WOORT_RETURN_SLOT, 3, elem);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, source_instance->m_velocity[i]);
    }
    woort_struct_set(WOORT_RETURN_SLOT, 4, elem);

    woort_struct_set_float(WOORT_RETURN_SLOT, 5, source_instance->m_reference_distance);
    woort_struct_set_float(WOORT_RETURN_SLOT, 6, source_instance->m_max_distance);
    woort_struct_set_float(WOORT_RETURN_SLOT, 7, source_instance->m_rolloff);
    woort_struct_set_float(WOORT_RETURN_SLOT, 8, source_instance->m_min_gain);
    woort_struct_set_float(WOORT_RETURN_SLOT, 9, source_instance->m_max_gain);
    woort_struct_set_bool(WOORT_RETURN_SLOT, 10, source_instance->m_relative);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, source_instance->m_direction[i]);
    }
    woort_struct_set(WOORT_RETURN_SLOT, 11, elem);

    woort_struct_set_float(WOORT_RETURN_SLOT, 12, source_instance->m_cone_inner_angle);
    woort_struct_set_float(WOORT_RETURN_SLOT, 13, source_instance->m_cone_outer_angle);
    woort_struct_set_float(WOORT_RETURN_SLOT, 14, source_instance->m_cone_outer_gain);

    return woort_ret();
}

WOORT_API woort_api wojeapi_audio_source_update(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));
    const woort_value updated_info = 1;

    (*source)->update(
        [&](je_audio_source_t* src)
        {
            src->m_loop = woort_struct_get_bool(updated_info, 0);
            src->m_gain = woort_struct_get_float(updated_info, 1);
            src->m_pitch = woort_struct_get_float(updated_info, 2);

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                src->m_location[i] =
                    woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                src->m_velocity[i] =
                    woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            src->m_reference_distance = woort_struct_get_float(updated_info, 5);
            src->m_max_distance = woort_struct_get_float(updated_info, 6);
            src->m_rolloff = woort_struct_get_float(updated_info, 7);
            src->m_min_gain = woort_struct_get_float(updated_info, 8);
            src->m_max_gain = woort_struct_get_float(updated_info, 9);
            src->m_relative = woort_struct_get_bool(updated_info, 10);

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 11);
            for (uint16_t i = 0; i < 3; ++i)
            {
                src->m_direction[i] =
                    woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            src->m_cone_inner_angle = woort_struct_get_float(updated_info, 12);
            src->m_cone_outer_angle = woort_struct_get_float(updated_info, 13);
            src->m_cone_outer_gain = woort_struct_get_float(updated_info, 14);
        });

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_play(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    (*source)->play();

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_stop(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    (*source)->stop();

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_pause(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    (*source)->pause();

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_set_buffer(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));
    basic::resource<audio::buffer>* buffer =
        static_cast<basic::resource<audio::buffer> *>(woort_gcpointer(1));

    (*source)->set_playing_buffer(*buffer);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_set_filter(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    optional<basic::resource<audio::filter>> filter;
    if (woort_option_get(WOORT_RETURN_SLOT, 1))
    {
        filter.emplace(
            *static_cast<basic::resource<audio::filter> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }

    (*source)->set_filter(filter);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_set_bus(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    const char* bus_name = woort_string(1);
    (*source)->set_bus(*bus_name == '\0' ? nullptr : je_audio_bus_get(bus_name));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_get_bus(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    auto* bus = (*source)->handle()->m_bus;
    return woort_ret_string(bus != nullptr ? bus->m_name : "");
}

WOORT_API woort_api wojeapi_audio_source_bind_effect_slot_and_filter(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(woort_gcpointer(0));

    optional<basic::resource<audio::effect_slot>> effect_slot;
    optional<basic::resource<audio::filter>> filter;

    if (woort_option_get(WOORT_RETURN_SLOT, 1))
    {
        effect_slot.emplace(
            *static_cast<basic::resource<audio::effect_slot> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }
    if (woort_option_get(WOORT_RETURN_SLOT, 2))
    {
        filter.emplace(
            *static_cast<basic::resource<audio::filter> *>(
                woort_gcpointer(WOORT_RETURN_SLOT)));
    }

    (*source)->bind_effect_slot((size_t)woort_int(3), effect_slot, filter);

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_source_get_state(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(
            woort_gcpointer(0));

    return woort_ret_int((woort_Int)(*source)->get_state());
}

WOORT_API woort_api wojeapi_audio_source_get_offset(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(
            woort_gcpointer(0));

    return woort_ret_int((woort_Int)(*source)->get_playing_offset());
}

WOORT_API woort_api wojeapi_audio_source_set_offset(void)
{
    basic::resource<audio::source>* source =
        static_cast<basic::resource<audio::source> *>(
            woort_gcpointer(0));

    (*source)->set_playing_offset((size_t)woort_int(1));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_listener_info(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    auto* listener_instance = je_audio_listener_get();

    woort_set_struct(result, 8);

    woort_struct_set_float(result, 0, listener_instance->m_gain);
    woort_struct_set_float(result, 1, listener_instance->m_global_gain);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, listener_instance->m_location[i]);
    }
    woort_struct_set(result, 2, elem);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, listener_instance->m_velocity[i]);
    }
    woort_struct_set(result, 3, elem);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, listener_instance->m_forward[i]);
    }
    woort_struct_set(result, 4, elem);

    woort_set_struct(elem, 3);
    for (uint16_t i = 0; i < 3; ++i)
    {
        woort_struct_set_float(elem, i, listener_instance->m_upward[i]);
    }
    woort_struct_set(result, 5, elem);

    woort_struct_set_int(result, 6, (woort_Int)listener_instance->m_distance_model);
    woort_struct_set_float(result, 7, listener_instance->m_doppler_factor);

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_audio_listener_update(void)
{
    const woort_value updated_info = 0;

    audio::listener::update(
        [&](je_audio_listener_t* lstn)
        {
            lstn->m_gain = woort_struct_get_float(updated_info, 0);
            lstn->m_global_gain = woort_struct_get_float(updated_info, 1);

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 2);
            for (uint16_t i = 0; i < 3; ++i)
            {
                lstn->m_location[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 3);
            for (uint16_t i = 0; i < 3; ++i)
            {
                lstn->m_velocity[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 4);
            for (uint16_t i = 0; i < 3; ++i)
            {
                lstn->m_forward[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            woort_struct_get(WOORT_RETURN_SLOT, updated_info, 5);
            for (uint16_t i = 0; i < 3; ++i)
            {
                lstn->m_upward[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);
            }

            lstn->m_distance_model =
                (je_audio_distance_model_t)woort_struct_get_int(updated_info, 6);
            lstn->m_doppler_factor = woort_struct_get_float(updated_info, 7);
        });

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_bus_gain_get(void)
{
    je_audio_bus_t* bus = je_audio_bus_get(woort_string(0));

    return woort_ret_real(je_audio_bus_gain_get(bus));
}

WOORT_API woort_api wojeapi_audio_bus_gain_set(void)
{
    je_audio_bus_t* bus = je_audio_bus_get(woort_string(0));
    je_audio_bus_gain_set(bus, (float)woort_real(1));

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_oneshot_play(void)
{
    basic::resource<audio::buffer>* buffer =
        static_cast<basic::resource<audio::buffer> *>(woort_gcpointer(0));

    const woort_value desc_value = 1;

    je_audio_oneshot_desc_t desc;

    desc.m_buffer = (*buffer)->handle();

    desc.m_spatial = woort_struct_get_bool(desc_value, 0);

    woort_struct_get(WOORT_RETURN_SLOT, desc_value, 1);
    for (uint16_t i = 0; i < 3; ++i)
    {
        desc.m_location[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);
    }

    desc.m_gain = woort_struct_get_float(desc_value, 2);
    desc.m_pitch = woort_struct_get_float(desc_value, 3);
    desc.m_reference_distance = woort_struct_get_float(desc_value, 4);
    desc.m_max_distance = woort_struct_get_float(desc_value, 5);
    desc.m_rolloff = woort_struct_get_float(desc_value, 6);
    desc.m_lowpass_hf = woort_struct_get_float(desc_value, 7);

    const char* bus_name = woort_string(2);
    desc.m_bus = *bus_name == '\0' ? nullptr : je_audio_bus_get(bus_name);

    return woort_ret_bool(je_audio_oneshot_play(&desc));
}

enum class woolang_je_audio_effect_kind
{
    REVERB,
    CHORUS,
    DISTORTION,
    ECHO,
    FLANGER,
    FREQUENCY_SHIFTER,
    VOCAL_MORPHER,
    PITCH_SHIFTER,
    RING_MODULATOR,
    AUTOWAH,
    COMPRESSOR,
    EQUALIZER,
    EAXREVERB,

    UNKOWN,
};

#define wo_je_effect_res_t(effect_type_name) \
    basic::resource<audio::effect<effect_type_name>>

WOORT_API woort_api wojeapi_audio_effect_create(void)
{
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(0);
    switch (kind)
    {
#define woort_ret_jeal_new_effect(effect_type_name)                                                  \
    woort_ret_gchandle(                                                                              \
        new wo_je_effect_res_t(effect_type_name)(                                                    \
            audio::effect<effect_type_name>::create()),                                       \
        WOORT_IGNORE,                                                                                \
        [](void *p)                                                                                  \
        {                                                                                            \
            delete static_cast<basic::resource<audio::effect<effect_type_name>> *>(p); \
        },                                                                                           \
        nullptr)

    case woolang_je_audio_effect_kind::REVERB:
        return woort_ret_jeal_new_effect(je_audio_effect_reverb_t);
    case woolang_je_audio_effect_kind::CHORUS:
        return woort_ret_jeal_new_effect(je_audio_effect_chorus_t);
    case woolang_je_audio_effect_kind::DISTORTION:
        return woort_ret_jeal_new_effect(je_audio_effect_distortion_t);
    case woolang_je_audio_effect_kind::ECHO:
        return woort_ret_jeal_new_effect(je_audio_effect_echo_t);
    case woolang_je_audio_effect_kind::FLANGER:
        return woort_ret_jeal_new_effect(je_audio_effect_flanger_t);
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
        return woort_ret_jeal_new_effect(je_audio_effect_frequency_shifter_t);
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
        return woort_ret_jeal_new_effect(je_audio_effect_vocal_morpher_t);
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
        return woort_ret_jeal_new_effect(je_audio_effect_pitch_shifter_t);
    case woolang_je_audio_effect_kind::RING_MODULATOR:
        return woort_ret_jeal_new_effect(je_audio_effect_ring_modulator_t);
    case woolang_je_audio_effect_kind::AUTOWAH:
        return woort_ret_jeal_new_effect(je_audio_effect_autowah_t);
    case woolang_je_audio_effect_kind::COMPRESSOR:
        return woort_ret_jeal_new_effect(je_audio_effect_compressor_t);
    case woolang_je_audio_effect_kind::EQUALIZER:
        return woort_ret_jeal_new_effect(je_audio_effect_equalizer_t);
    case woolang_je_audio_effect_kind::EAXREVERB:
        return woort_ret_jeal_new_effect(je_audio_effect_eax_reverb_t);
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);

#undef woort_ret_jeal_new_effect
    }
}

WOORT_API woort_api wojeapi_audio_effect_info(void)
{
    woort_value s;
    if (!woort_push_reserve(2, &s))
        return woort_ret_panic("Stack overflow.");

    const woort_value result = s + 0;
    const woort_value elem = s + 1;

    void* effect_res_ptr = woort_gcpointer(0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(1);

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect = static_cast<wo_je_effect_res_t(je_audio_effect_reverb_t)*>(
            effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 13);

        woort_struct_set_float(result, 0, info->m_density);
        woort_struct_set_float(result, 1, info->m_diffusion);
        woort_struct_set_float(result, 2, info->m_gain);
        woort_struct_set_float(result, 3, info->m_gain_hf);
        woort_struct_set_float(result, 4, info->m_decay_time);
        woort_struct_set_float(result, 5, info->m_decay_hf_ratio);
        woort_struct_set_float(result, 6, info->m_reflections_gain);
        woort_struct_set_float(result, 7, info->m_reflections_delay);
        woort_struct_set_float(result, 8, info->m_late_reverb_gain);
        woort_struct_set_float(result, 9, info->m_late_reverb_delay);
        woort_struct_set_float(result, 10, info->m_air_absorption_gain_hf);
        woort_struct_set_float(result, 11, info->m_room_rolloff_factor);
        woort_struct_set_bool(result, 12, info->m_decay_hf_limit);

        break;
    }
    case woolang_je_audio_effect_kind::CHORUS:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_chorus_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 6);

        woort_struct_set_int(result, 0, (woort_Int)info->m_waveform);
        woort_struct_set_int(result, 1, (woort_Int)info->m_phase);
        woort_struct_set_float(result, 2, info->m_rate);
        woort_struct_set_float(result, 3, info->m_depth);
        woort_struct_set_float(result, 4, info->m_feedback);
        woort_struct_set_float(result, 5, info->m_delay);

        break;
    }
    case woolang_je_audio_effect_kind::DISTORTION:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_distortion_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 5);

        woort_struct_set_float(result, 0, info->m_edge);
        woort_struct_set_float(result, 1, info->m_gain);
        woort_struct_set_float(result, 2, info->m_lowpass_cutoff);
        woort_struct_set_float(result, 3, info->m_equalizer_center_freq);
        woort_struct_set_float(result, 4, info->m_equalizer_bandwidth);

        break;
    }
    case woolang_je_audio_effect_kind::ECHO:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_echo_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 5);

        woort_struct_set_float(result, 0, info->m_delay);
        woort_struct_set_float(result, 1, info->m_lr_delay);
        woort_struct_set_float(result, 2, info->m_damping);
        woort_struct_set_float(result, 3, info->m_feedback);
        woort_struct_set_float(result, 4, info->m_spread);

        break;
    }
    case woolang_je_audio_effect_kind::FLANGER:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_flanger_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 6);

        woort_struct_set_int(result, 0, (woort_Int)info->m_waveform);
        woort_struct_set_int(result, 1, (woort_Int)info->m_phase);
        woort_struct_set_float(result, 2, info->m_rate);
        woort_struct_set_float(result, 3, info->m_depth);
        woort_struct_set_float(result, 4, info->m_feedback);
        woort_struct_set_float(result, 5, info->m_delay);

        break;
    }
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_frequency_shifter_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 3);

        woort_struct_set_float(result, 0, info->m_frequency);
        woort_struct_set_int(result, 1, (woort_Int)info->m_left_direction);
        woort_struct_set_int(result, 2, (woort_Int)info->m_right_direction);

        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_vocal_morpher_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 6);

        woort_struct_set_int(result, 0, (woort_Int)info->m_phoneme_a);
        woort_struct_set_int(result, 1, (woort_Int)info->m_phoneme_a_coarse_tuning);
        woort_struct_set_int(result, 2, (woort_Int)info->m_phoneme_b);
        woort_struct_set_int(result, 3, (woort_Int)info->m_phoneme_b_coarse_tuning);
        woort_struct_set_int(result, 4, (woort_Int)info->m_waveform);
        woort_struct_set_float(result, 5, info->m_rate);

        break;
    }
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_pitch_shifter_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 2);

        woort_struct_set_int(result, 0, (woort_Int)info->m_coarse_tune);
        woort_struct_set_int(result, 1, (woort_Int)info->m_fine_tune);

        break;
    }
    case woolang_je_audio_effect_kind::RING_MODULATOR:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_ring_modulator_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 3);

        woort_struct_set_float(result, 0, info->m_frequency);
        woort_struct_set_float(result, 1, info->m_highpass_cutoff);
        woort_struct_set_int(result, 2, (woort_Int)info->m_waveform);

        break;
    }
    case woolang_je_audio_effect_kind::AUTOWAH:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_autowah_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 4);

        woort_struct_set_float(result, 0, info->m_attack_time);
        woort_struct_set_float(result, 1, info->m_release_time);
        woort_struct_set_float(result, 2, info->m_resonance);
        woort_struct_set_float(result, 3, info->m_peak_gain);

        break;
    }
    case woolang_je_audio_effect_kind::COMPRESSOR:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_compressor_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 1);

        woort_struct_set_bool(result, 0, info->m_enabled);

        break;
    }
    case woolang_je_audio_effect_kind::EQUALIZER:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_equalizer_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 10);

        woort_struct_set_float(result, 0, info->m_low_gain);
        woort_struct_set_float(result, 1, info->m_low_cutoff);
        woort_struct_set_float(result, 2, info->m_mid1_gain);
        woort_struct_set_float(result, 3, info->m_mid1_center);
        woort_struct_set_float(result, 4, info->m_mid1_width);
        woort_struct_set_float(result, 5, info->m_mid2_gain);
        woort_struct_set_float(result, 6, info->m_mid2_center);
        woort_struct_set_float(result, 7, info->m_mid2_width);
        woort_struct_set_float(result, 8, info->m_high_gain);
        woort_struct_set_float(result, 9, info->m_high_cutoff);

        break;
    }
    case woolang_je_audio_effect_kind::EAXREVERB:
    {
        auto* effect =
            static_cast<wo_je_effect_res_t(je_audio_effect_eax_reverb_t)*>(effect_res_ptr);

        auto* info = (*effect)->handle();

        woort_set_struct(result, 23);

        woort_struct_set_float(result, 0, info->m_density);
        woort_struct_set_float(result, 1, info->m_diffusion);
        woort_struct_set_float(result, 2, info->m_gain);
        woort_struct_set_float(result, 3, info->m_gain_hf);
        woort_struct_set_float(result, 4, info->m_gain_lf);
        woort_struct_set_float(result, 5, info->m_decay_time);
        woort_struct_set_float(result, 6, info->m_decay_hf_ratio);
        woort_struct_set_float(result, 7, info->m_decay_lf_ratio);
        woort_struct_set_float(result, 8, info->m_reflections_gain);
        woort_struct_set_float(result, 9, info->m_reflections_delay);

        woort_set_struct(elem, 3);
        for (uint16_t i = 0; i < 3; ++i)
            woort_struct_set_float(elem, i, info->m_reflections_pan_xyz[i]);
        woort_struct_set(result, 10, elem);

        woort_struct_set_float(result, 11, info->m_late_reverb_gain);
        woort_struct_set_float(result, 12, info->m_late_reverb_delay);

        woort_set_struct(elem, 3);
        for (uint16_t i = 0; i < 3; ++i)
            woort_struct_set_float(elem, i, info->m_late_reverb_pan_xyz[i]);
        woort_struct_set(result, 13, elem);

        woort_struct_set_float(result, 14, info->m_echo_time);
        woort_struct_set_float(result, 15, info->m_echo_depth);
        woort_struct_set_float(result, 16, info->m_modulation_time);
        woort_struct_set_float(result, 17, info->m_modulation_depth);
        woort_struct_set_float(result, 18, info->m_air_absorption_gain_hf);
        woort_struct_set_float(result, 19, info->m_hf_reference);
        woort_struct_set_float(result, 20, info->m_lf_reference);
        woort_struct_set_float(result, 21, info->m_room_rolloff_factor);
        woort_struct_set_bool(result, 22, info->m_decay_hf_limit);

        break;
    }
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }

    return woort_ret_value(result);
}

WOORT_API woort_api wojeapi_audio_effect_update(void)
{
    void* effect = woort_gcpointer(0);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(1);
    const woort_value updated_info = 2;

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_reverb_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_reverb_t* eff)
            {
                eff->m_density = woort_struct_get_float(updated_info, 0);
                eff->m_diffusion = woort_struct_get_float(updated_info, 1);
                eff->m_gain = woort_struct_get_float(updated_info, 2);
                eff->m_gain_hf = woort_struct_get_float(updated_info, 3);
                eff->m_decay_time = woort_struct_get_float(updated_info, 4);
                eff->m_decay_hf_ratio = woort_struct_get_float(updated_info, 5);
                eff->m_reflections_gain = woort_struct_get_float(updated_info, 6);
                eff->m_reflections_delay = woort_struct_get_float(updated_info, 7);
                eff->m_late_reverb_gain = woort_struct_get_float(updated_info, 8);
                eff->m_late_reverb_delay = woort_struct_get_float(updated_info, 9);
                eff->m_air_absorption_gain_hf = woort_struct_get_float(updated_info, 10);
                eff->m_room_rolloff_factor = woort_struct_get_float(updated_info, 11);
                eff->m_decay_hf_limit = woort_struct_get_bool(updated_info, 12); });
        break;
    }
    case woolang_je_audio_effect_kind::CHORUS:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_chorus_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_chorus_t* eff)
            {
                eff->m_waveform = (je_audio_effect_chorus_waveform_t)woort_struct_get_int(updated_info, 0);
                eff->m_phase = (int)woort_struct_get_int(updated_info, 1);
                eff->m_rate = woort_struct_get_float(updated_info, 2);
                eff->m_depth = woort_struct_get_float(updated_info, 3);
                eff->m_feedback = woort_struct_get_float(updated_info, 4);
                eff->m_delay = woort_struct_get_float(updated_info, 5); });
        break;
    }
    case woolang_je_audio_effect_kind::DISTORTION:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_distortion_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_distortion_t* eff)
            {
                eff->m_edge = woort_struct_get_float(updated_info, 0);
                eff->m_gain = woort_struct_get_float(updated_info, 1);
                eff->m_lowpass_cutoff = woort_struct_get_float(updated_info, 2);
                eff->m_equalizer_center_freq = woort_struct_get_float(updated_info, 3);
                eff->m_equalizer_bandwidth = woort_struct_get_float(updated_info, 4); });
        break;
    }
    case woolang_je_audio_effect_kind::ECHO:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_echo_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_echo_t* eff)
            {
                eff->m_delay = woort_struct_get_float(updated_info, 0);
                eff->m_lr_delay = woort_struct_get_float(updated_info, 1);
                eff->m_damping = woort_struct_get_float(updated_info, 2);
                eff->m_feedback = woort_struct_get_float(updated_info, 3);
                eff->m_spread = woort_struct_get_float(updated_info, 4); });
        break;
    }
    case woolang_je_audio_effect_kind::FLANGER:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_flanger_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_flanger_t* eff)
            {
                eff->m_waveform = (je_audio_effect_flanger_waveform_t)woort_struct_get_int(updated_info, 0);
                eff->m_phase = (int)woort_struct_get_int(updated_info, 1);
                eff->m_rate = woort_struct_get_float(updated_info, 2);
                eff->m_depth = woort_struct_get_float(updated_info, 3);
                eff->m_feedback = woort_struct_get_float(updated_info, 4);
                eff->m_delay = woort_struct_get_float(updated_info, 5); });
        break;
    }
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_frequency_shifter_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_frequency_shifter_t* eff)
            {
                eff->m_frequency = woort_struct_get_float(updated_info, 0);
                eff->m_left_direction = (je_audio_effect_frequency_shifter_direction_t)woort_struct_get_int(updated_info, 1);
                eff->m_right_direction = (je_audio_effect_frequency_shifter_direction_t)woort_struct_get_int(updated_info, 2); });
        break;
    }
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_vocal_morpher_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_vocal_morpher_t* eff)
            {
                eff->m_phoneme_a = (je_audio_effect_vocal_morpher_phoneme_t)woort_struct_get_int(updated_info, 0);
                eff->m_phoneme_a_coarse_tuning = (int)woort_struct_get_int(updated_info, 1);
                eff->m_phoneme_b = (je_audio_effect_vocal_morpher_phoneme_t)woort_struct_get_int(updated_info, 2);
                eff->m_phoneme_b_coarse_tuning = (int)woort_struct_get_int(updated_info, 3);
                eff->m_waveform = (je_audio_effect_vocal_morpher_waveform_t)woort_struct_get_int(updated_info, 4);
                eff->m_rate = woort_struct_get_float(updated_info, 5); });
        break;
    }
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_pitch_shifter_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_pitch_shifter_t* eff)
            {
                eff->m_coarse_tune = (int)woort_struct_get_int(updated_info, 0);
                eff->m_fine_tune = (int)woort_struct_get_int(updated_info, 1); });
        break;
    }
    case woolang_je_audio_effect_kind::RING_MODULATOR:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_ring_modulator_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_ring_modulator_t* eff)
            {
                eff->m_frequency = woort_struct_get_float(updated_info, 0);
                eff->m_highpass_cutoff = woort_struct_get_float(updated_info, 1);
                eff->m_waveform = (je_audio_effect_ring_modulator_waveform_t)woort_struct_get_int(updated_info, 2); });
        break;
    }
    case woolang_je_audio_effect_kind::AUTOWAH:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_autowah_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_autowah_t* eff)
            {
                eff->m_attack_time = woort_struct_get_float(updated_info, 0);
                eff->m_release_time = woort_struct_get_float(updated_info, 1);
                eff->m_resonance = woort_struct_get_float(updated_info, 2);
                eff->m_peak_gain = woort_struct_get_float(updated_info, 3); });
        break;
    }
    case woolang_je_audio_effect_kind::COMPRESSOR:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_compressor_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_compressor_t* eff)
            { eff->m_enabled = woort_struct_get_bool(updated_info, 0); });
        break;
    }
    case woolang_je_audio_effect_kind::EQUALIZER:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_equalizer_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_equalizer_t* eff)
            {
                eff->m_low_gain = woort_struct_get_float(updated_info, 0);
                eff->m_low_cutoff = woort_struct_get_float(updated_info, 1);
                eff->m_mid1_gain = woort_struct_get_float(updated_info, 2);
                eff->m_mid1_center = woort_struct_get_float(updated_info, 3);
                eff->m_mid1_width = woort_struct_get_float(updated_info, 4);
                eff->m_mid2_gain = woort_struct_get_float(updated_info, 5);
                eff->m_mid2_center = woort_struct_get_float(updated_info, 6);
                eff->m_mid2_width = woort_struct_get_float(updated_info, 7);
                eff->m_high_gain = woort_struct_get_float(updated_info, 8);
                eff->m_high_cutoff = woort_struct_get_float(updated_info, 9); });
        break;
    }
    case woolang_je_audio_effect_kind::EAXREVERB:
    {
        auto* effect_instance =
            static_cast<wo_je_effect_res_t(je_audio_effect_eax_reverb_t)*>(effect);

        (*effect_instance)->update([&](je_audio_effect_eax_reverb_t* eff)
            {
                eff->m_density = woort_struct_get_float(updated_info, 0);
                eff->m_diffusion = woort_struct_get_float(updated_info, 1);
                eff->m_gain = woort_struct_get_float(updated_info, 2);
                eff->m_gain_hf = woort_struct_get_float(updated_info, 3);
                eff->m_gain_lf = woort_struct_get_float(updated_info, 4);
                eff->m_decay_time = woort_struct_get_float(updated_info, 5);
                eff->m_decay_hf_ratio = woort_struct_get_float(updated_info, 6);
                eff->m_decay_lf_ratio = woort_struct_get_float(updated_info, 7);
                eff->m_reflections_gain = woort_struct_get_float(updated_info, 8);
                eff->m_reflections_delay = woort_struct_get_float(updated_info, 9);

                woort_struct_get(WOORT_RETURN_SLOT, updated_info, 10);
                for (uint16_t i = 0; i < 3; ++i)
                    eff->m_reflections_pan_xyz[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);

                eff->m_late_reverb_gain = woort_struct_get_float(updated_info, 11);
                eff->m_late_reverb_delay = woort_struct_get_float(updated_info, 12);

                woort_struct_get(WOORT_RETURN_SLOT, updated_info, 13);
                for (uint16_t i = 0; i < 3; ++i)
                    eff->m_late_reverb_pan_xyz[i] = woort_struct_get_float(WOORT_RETURN_SLOT, i);

                eff->m_echo_time = woort_struct_get_float(updated_info, 14);
                eff->m_echo_depth = woort_struct_get_float(updated_info, 15);
                eff->m_modulation_time = woort_struct_get_float(updated_info, 16);
                eff->m_modulation_depth = woort_struct_get_float(updated_info, 17);
                eff->m_air_absorption_gain_hf = woort_struct_get_float(updated_info, 18);
                eff->m_hf_reference = woort_struct_get_float(updated_info, 19);
                eff->m_lf_reference = woort_struct_get_float(updated_info, 20);
                eff->m_room_rolloff_factor = woort_struct_get_float(updated_info, 21);
                eff->m_decay_hf_limit = woort_struct_get_bool(updated_info, 22); });
        break;
    }
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }

    return woort_ret_void();
}

WOORT_API woort_api wojeapi_audio_effect_slot_create(void)
{
    return woort_ret_gchandle(
        new basic::resource<audio::effect_slot>(
            audio::effect_slot::create()),
        WOORT_IGNORE,
        [](void* p)
        {
            delete static_cast<basic::resource<audio::effect_slot>*>(p);
        },
        nullptr);
}

WOORT_API woort_api wojeapi_audio_effect_slot_bind_effect(void)
{
    auto* effect_slot =
        static_cast<basic::resource<audio::effect_slot> *>(
            woort_gcpointer(0));
    auto* effect = woort_gcpointer(1);
    woolang_je_audio_effect_kind kind = (woolang_je_audio_effect_kind)woort_int(2);

    switch (kind)
    {
    case woolang_je_audio_effect_kind::REVERB:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_reverb_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::CHORUS:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_chorus_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::DISTORTION:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_distortion_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::ECHO:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_echo_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::FLANGER:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_flanger_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::FREQUENCY_SHIFTER:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_frequency_shifter_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::VOCAL_MORPHER:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_vocal_morpher_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::PITCH_SHIFTER:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_pitch_shifter_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::RING_MODULATOR:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_ring_modulator_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::AUTOWAH:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_autowah_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::COMPRESSOR:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_compressor_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::EQUALIZER:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_equalizer_t)*>(effect));
        break;
    case woolang_je_audio_effect_kind::EAXREVERB:
        (*effect_slot)->bind_effect(*static_cast<wo_je_effect_res_t(je_audio_effect_eax_reverb_t)*>(effect));
        break;
    default:
        return woort_ret_panic("Unknown audio effect type: %d", (int)kind);
    }
    return woort_ret_void();
}

#undef wo_je_effect_res_t

WOORT_API woort_api wojeapi_audio_effect_slot_info(void)
{
    auto* effect_slot =
        static_cast<basic::resource<audio::effect_slot> *>(
            woort_gcpointer(0));

    auto effect_slot_info = (*effect_slot)->handle();

    woort_set_struct(WOORT_RETURN_SLOT, 1);

    woort_struct_set_float(WOORT_RETURN_SLOT, 0, effect_slot_info->m_gain);

    return woort_ret();
}

WOORT_API woort_api wojeapi_audio_effect_slot_update(void)
{
    auto* effect_slot =
        static_cast<basic::resource<audio::effect_slot> *>(
            woort_gcpointer(0));

    const woort_value info = 1;

    (*effect_slot)->update([&](je_audio_effect_slot_t* eff)
        { eff->m_gain = woort_struct_get_float(info, 0); });

    return woort_ret_void();
}
