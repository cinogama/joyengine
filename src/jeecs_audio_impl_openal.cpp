#define JE_IMPL
#include "jeecs.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <unordered_set>
#include <vector>
#include <mutex>

constexpr ALCint JEAL_MAX_SOURCE_BINDING_EFFECT_SLOT_COUNT = 8;

struct jeal_device
{
    bool        m_alive;
    const char* m_device_name;
    ALCdevice* m_openal_device;
};
struct jeal_capture_device
{
    bool        m_alive;
    const char* m_device_name;
    ALCdevice* m_capturing_device;
    size_t      m_size_per_sample;
};

struct jeal_effect
{
    ALenum m_effect_kind;

    bool m_effect_instance_exist;
    ALuint m_effect_instance;
};

struct jeal_effect_slot
{
    void* m_binded_effect_may_null;

    ALfloat m_gain;
    ALuint m_effect_slot_instance;
};

struct jeal_source
{
    ALuint m_openal_source;

    // 当前源最后播放的 buffer，不过需要注意，
    // 如果source的状态处于stopped，那么这个地方的buffer就应该被视为无效的
    jeal_buffer* m_last_played_buffer;
    jeal_effect_slot* m_binded_effect_slots_may_null[JEAL_MAX_SOURCE_BINDING_EFFECT_SLOT_COUNT];
};

struct jeal_buffer
{
    ALuint m_openal_buffer;
    const void* m_data;
    ALsizei m_size;
    ALsizei m_sample_rate;
    ALsizei m_byterate;
    ALenum m_format;
};

struct _jeal_static_context
{
    std::shared_mutex _jeal_context_mx;
    std::mutex _jeal_all_devices_mx;

    jeal_device* _jeal_current_device = nullptr;
    ALCint _jeal_efx_enabled_sends_for_each_source;

    LPALGENEFFECTS impl_alGenEffects = nullptr;
    LPALDELETEEFFECTS impl_alDeleteEffects = nullptr;
    LPALEFFECTI impl_alEffecti = nullptr;
    LPALEFFECTF impl_alEffectf = nullptr;
    LPALEFFECTFV impl_alEffectfv = nullptr;
    LPALGENAUXILIARYEFFECTSLOTS impl_alGenAuxiliaryEffectSlots = nullptr;
    LPALDELETEAUXILIARYEFFECTSLOTS impl_alDeleteAuxiliaryEffectSlots = nullptr;
    LPALAUXILIARYEFFECTSLOTI impl_alAuxiliaryEffectSloti = nullptr;
    LPALAUXILIARYEFFECTSLOTF impl_alAuxiliaryEffectSlotf = nullptr;
    LPALGENFILTERS impl_alGenFilters = nullptr;
    LPALDELETEFILTERS impl_alDeleteFilters = nullptr;
    LPALFILTERI impl_alFilteri = nullptr;
    LPALFILTERF impl_alFilterf = nullptr;

    std::vector<jeal_device*> _jeal_all_devices;
    std::vector<jeal_capture_device*> _jeal_all_capture_devices;

    std::mutex _jeal_all_sources_mx;
    std::unordered_set<jeal_source*> _jeal_all_sources;

    std::mutex _jeal_all_buffers_mx;
    std::unordered_set<jeal_buffer*> _jeal_all_buffers;

    std::mutex _jeal_all_effects_mx;
    std::unordered_map<void*, jeal_effect> _jeal_all_effects;

    std::mutex _jeal_all_effect_slots_mx;
    std::unordered_set<jeal_effect_slot*> _jeal_all_effect_slots;

    std::atomic<float> _jeal_listener_gain = 1.0f;
    std::atomic<float> _jeal_global_volume_gain = 1.0f;
};
_jeal_static_context _jeal_ctx;


////////////////////////////////////////////////////////

void _jeal_update_buffer_instance(jeal_buffer* buffer)
{
    alGenBuffers(1, &buffer->m_openal_buffer);

    //now we put our data into the openAL buffer and
    //check for success
    alBufferData(buffer->m_openal_buffer,
        buffer->m_format,
        buffer->m_data,
        buffer->m_size,
        buffer->m_sample_rate);
}
void _jeal_create_effect_instance(void* instance, ALenum effect_type)
{
    auto& effect = _jeal_ctx._jeal_all_effects[instance];
    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        effect.m_effect_kind = effect_type;

        _jeal_ctx.impl_alGenEffects(1, &effect.m_effect_instance);
        _jeal_ctx.impl_alEffecti(
            effect.m_effect_instance,
            AL_EFFECT_TYPE,
            effect.m_effect_kind);

        effect.m_effect_instance_exist = true;
    }
}

void _jeal_update_effect_instance(void* instance, jeal_effect* effect)
{
    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        assert(effect->m_effect_instance_exist);
        switch (effect->m_effect_kind)
        {
        case AL_EFFECT_REVERB:
        {
            const jeal_effect_reverb* instance_data =
                reinterpret_cast<jeal_effect_reverb*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_DENSITY, instance_data->m_density);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_DIFFUSION, instance_data->m_diffusion);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_GAIN, instance_data->m_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_GAINHF, instance_data->m_gain_hf);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_DECAY_TIME, instance_data->m_decay_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_DECAY_HFRATIO, instance_data->m_decay_hf_ratio);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_REFLECTIONS_GAIN, instance_data->m_reflections_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_REFLECTIONS_DELAY, instance_data->m_reflections_delay);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_LATE_REVERB_GAIN, instance_data->m_late_reverb_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_LATE_REVERB_DELAY, instance_data->m_late_reverb_delay);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_AIR_ABSORPTION_GAINHF, instance_data->m_air_absorption_gain_hf);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_REVERB_ROOM_ROLLOFF_FACTOR, instance_data->m_room_rolloff_factor);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_REVERB_DECAY_HFLIMIT, instance_data->m_decay_hf_limit ? AL_TRUE : AL_FALSE);

            break;
        }
        case AL_EFFECT_CHORUS:
        {
            const jeal_effect_chorus* instance_data =
                reinterpret_cast<jeal_effect_chorus*>(instance);

            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_CHORUS_WAVEFORM, (ALint)instance_data->m_waveform);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_CHORUS_PHASE, instance_data->m_phase);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_CHORUS_RATE, instance_data->m_rate);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_CHORUS_DEPTH, instance_data->m_depth);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_CHORUS_FEEDBACK, instance_data->m_feedback);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_CHORUS_DELAY, instance_data->m_delay);

            break;
        }
        case AL_EFFECT_DISTORTION:
        {
            const jeal_effect_distortion* instance_data =
                reinterpret_cast<jeal_effect_distortion*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_DISTORTION_EDGE, instance_data->m_edge);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_DISTORTION_GAIN, instance_data->m_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_DISTORTION_LOWPASS_CUTOFF, instance_data->m_lowpass_cutoff);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_DISTORTION_EQCENTER, instance_data->m_equalizer_center_freq);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_DISTORTION_EQBANDWIDTH, instance_data->m_equalizer_bandwidth);

            break;
        }
        case AL_EFFECT_ECHO:
        {
            const jeal_effect_echo* instance_data =
                reinterpret_cast<jeal_effect_echo*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_ECHO_DELAY, instance_data->m_delay);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_ECHO_LRDELAY, instance_data->m_lr_delay);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_ECHO_DAMPING, instance_data->m_damping);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_ECHO_FEEDBACK, instance_data->m_feedback);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_ECHO_SPREAD, instance_data->m_spread);

            break;
        }
        case AL_EFFECT_FLANGER:
        {
            const jeal_effect_flanger* instance_data =
                reinterpret_cast<jeal_effect_flanger*>(instance);

            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_FLANGER_WAVEFORM, (ALint)instance_data->m_waveform);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_FLANGER_PHASE, instance_data->m_phase);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_FLANGER_RATE, instance_data->m_rate);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_FLANGER_DEPTH, instance_data->m_depth);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_FLANGER_FEEDBACK, instance_data->m_feedback);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_FLANGER_DELAY, instance_data->m_delay);

            break;
        }
        case AL_EFFECT_FREQUENCY_SHIFTER:
        {
            const jeal_effect_frequency_shifter* instance_data =
                reinterpret_cast<jeal_effect_frequency_shifter*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_FREQUENCY_SHIFTER_FREQUENCY, instance_data->m_frequency);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_FREQUENCY_SHIFTER_LEFT_DIRECTION, (ALint)instance_data->m_left_direction);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION, (ALint)instance_data->m_right_direction);

            break;
        }
        case AL_EFFECT_VOCAL_MORPHER:
        {
            const jeal_effect_vocal_morpher* instance_data =
                reinterpret_cast<jeal_effect_vocal_morpher*>(instance);

            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_VOCAL_MORPHER_PHONEMEA, (ALint)instance_data->m_phoneme_a);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_VOCAL_MORPHER_PHONEMEA_COARSE_TUNING, instance_data->m_phoneme_a_coarse_tuning);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_VOCAL_MORPHER_PHONEMEB, (ALint)instance_data->m_phoneme_b);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_VOCAL_MORPHER_PHONEMEB_COARSE_TUNING, instance_data->m_phoneme_b_coarse_tuning);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_VOCAL_MORPHER_WAVEFORM, (ALint)instance_data->m_waveform);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_VOCAL_MORPHER_RATE, instance_data->m_rate);

            break;
        }
        case AL_EFFECT_PITCH_SHIFTER:
        {
            const jeal_effect_pitch_shifter* instance_data =
                reinterpret_cast<jeal_effect_pitch_shifter*>(instance);

            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_PITCH_SHIFTER_COARSE_TUNE, instance_data->m_coarse_tune);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_PITCH_SHIFTER_FINE_TUNE, instance_data->m_fine_tune);

            break;
        }
        case AL_EFFECT_RING_MODULATOR:
        {
            const jeal_effect_ring_modulator* instance_data =
                reinterpret_cast<jeal_effect_ring_modulator*>(instance);
            
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_RING_MODULATOR_FREQUENCY, instance_data->m_frequency);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_RING_MODULATOR_HIGHPASS_CUTOFF, instance_data->m_highpass_cutoff);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_RING_MODULATOR_WAVEFORM, (ALint)instance_data->m_waveform);

            break;
        }
        case AL_EFFECT_AUTOWAH:
        {
            const jeal_effect_autowah* instance_data =
                reinterpret_cast<jeal_effect_autowah*>(instance);
            
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_AUTOWAH_ATTACK_TIME, instance_data->m_attack_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_AUTOWAH_RELEASE_TIME, instance_data->m_release_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_AUTOWAH_RESONANCE, instance_data->m_resonance);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_AUTOWAH_PEAK_GAIN, instance_data->m_peak_gain);

            break;
        }
        case AL_EFFECT_COMPRESSOR:
        {
            const jeal_effect_compressor* instance_data =
                reinterpret_cast<jeal_effect_compressor*>(instance);

            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_COMPRESSOR_ONOFF,  instance_data->m_enabled ? AL_TRUE : AL_FALSE);
         
            break;
        }
        case AL_EFFECT_EQUALIZER:
        {
            const jeal_effect_equalizer* instance_data =
                reinterpret_cast<jeal_effect_equalizer*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_LOW_GAIN, instance_data->m_low_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_LOW_CUTOFF, instance_data->m_low_cutoff);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID1_GAIN, instance_data->m_mid1_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID1_CENTER, instance_data->m_mid1_center);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID1_WIDTH, instance_data->m_mid1_width);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID2_GAIN, instance_data->m_mid2_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID2_CENTER, instance_data->m_mid2_center);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_MID2_WIDTH, instance_data->m_mid2_width);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_HIGH_GAIN, instance_data->m_high_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EQUALIZER_HIGH_CUTOFF, instance_data->m_high_cutoff);

            break;
        }
        case AL_EFFECT_EAXREVERB:
        {
            const jeal_effect_eaxreverb* instance_data =
                reinterpret_cast<jeal_effect_eaxreverb*>(instance);

            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_DENSITY, instance_data->m_density);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_DIFFUSION, instance_data->m_diffusion);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_GAIN, instance_data->m_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_GAINHF, instance_data->m_gain_hf);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_GAINLF, instance_data->m_gain_lf);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_DECAY_TIME, instance_data->m_decay_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_DECAY_HFRATIO, instance_data->m_decay_hf_ratio);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_DECAY_LFRATIO, instance_data->m_decay_lf_ratio);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_REFLECTIONS_GAIN, instance_data->m_reflections_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_REFLECTIONS_DELAY, instance_data->m_reflections_delay);

            const float reflection_pan[3] = {
                 instance_data->m_reflections_pan_xyz[0],
                 instance_data->m_reflections_pan_xyz[1],
                 -instance_data->m_reflections_pan_xyz[2],
            };
            _jeal_ctx.impl_alEffectfv(effect->m_effect_instance, AL_EAXREVERB_REFLECTIONS_PAN, reflection_pan);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_LATE_REVERB_GAIN, instance_data->m_late_reverb_gain);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_LATE_REVERB_DELAY, instance_data->m_late_reverb_delay);

            const float reverb_pan[3] = {
                instance_data->m_late_reverb_pan_xyz[0],
                instance_data->m_late_reverb_pan_xyz[1],
                -instance_data->m_late_reverb_pan_xyz[2],
            };
            _jeal_ctx.impl_alEffectfv(effect->m_effect_instance, AL_EAXREVERB_LATE_REVERB_PAN, reverb_pan);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_ECHO_TIME, instance_data->m_echo_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_ECHO_DEPTH, instance_data->m_echo_depth);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_MODULATION_TIME, instance_data->m_modulation_time);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_MODULATION_DEPTH, instance_data->m_modulation_depth);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, instance_data->m_air_absorption_gain_hf);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_HFREFERENCE, instance_data->m_hf_reference);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_LFREFERENCE, instance_data->m_lf_reference);
            _jeal_ctx.impl_alEffectf(effect->m_effect_instance, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, instance_data->m_room_rolloff_factor);
            _jeal_ctx.impl_alEffecti(effect->m_effect_instance, AL_EAXREVERB_DECAY_HFLIMIT, instance_data->m_decay_hf_limiter ? AL_TRUE : AL_FALSE);

            break;
        }
        default:
            jeecs::debug::logfatal("Unknown audio effect: %d when trying to update.",
                (int)effect->m_effect_kind);
        }
    }
}

void _jeal_source_bind_effect_slot(
    jeal_source* source, jeal_effect_slot* slot_may_null, size_t pass)
{
    if (pass < JEAL_MAX_SOURCE_BINDING_EFFECT_SLOT_COUNT)
    {
        source->m_binded_effect_slots_may_null[pass] = slot_may_null;
        if (pass < _jeal_ctx._jeal_efx_enabled_sends_for_each_source)
        {
            alSource3i(
                source->m_openal_source,
                AL_AUXILIARY_SEND_FILTER,
                slot_may_null == nullptr ? AL_EFFECTSLOT_NULL : slot_may_null->m_effect_slot_instance,
                (ALint)pass,
                AL_FILTER_NULL);
        }
    }
}

void _jeal_update_source(jeal_source* source)
{
    alGenSources(1, &source->m_openal_source);

    for (size_t i = 0; i < JEAL_MAX_SOURCE_BINDING_EFFECT_SLOT_COUNT; i++)
    {
        auto& effect_slot = source->m_binded_effect_slots_may_null[i];
        // 此处需要检查 effect_slot 是否依然有效；因为slot可能在未解除绑定的情况下被删除
        if (effect_slot == nullptr
            || _jeal_ctx._jeal_all_effect_slots.find(effect_slot) != _jeal_ctx._jeal_all_effect_slots.end())
        {
            _jeal_source_bind_effect_slot(source, effect_slot, i);
        }
        else
        {
            jeecs::debug::logwarn("Found invalid effect slot in source: %p.", source);
            effect_slot = nullptr;
        }
    }
}

struct _jeal_global_context
{
    struct source_restoring_information
    {
        jeal_source* m_source;
        jeecs::math::vec3 m_position;
        jeecs::math::vec3 m_velocity;
        size_t m_offset;
        float m_pitch;
        float m_volume;
        jeal_state m_state;
        jeal_buffer* m_playing_buffer;
        ALint m_loop;
    };
    source_restoring_information listener_information = {};
    float listener_orientation[6] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

    std::vector<source_restoring_information> sources_information;
};

void _jeal_effect_drop(void* instance)
{
    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        auto& effect = _jeal_ctx._jeal_all_effects.at(instance);
        assert(effect.m_effect_instance_exist);

        _jeal_ctx.impl_alDeleteEffects(1, &effect.m_effect_instance);
        effect.m_effect_instance_exist = false;
    }
}

bool _jeal_shutdown_current_device()
{
    _jeal_ctx._jeal_current_device = nullptr;
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        for (auto* src : _jeal_ctx._jeal_all_sources)
            alDeleteSources(1, &src->m_openal_source);

        for (auto* buf : _jeal_ctx._jeal_all_buffers)
            alDeleteBuffers(1, &buf->m_openal_buffer);

        for (auto& effect_slot : _jeal_ctx._jeal_all_effect_slots)
        {
            if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
            {
                _jeal_ctx.impl_alDeleteAuxiliaryEffectSlots(
                    1, &effect_slot->m_effect_slot_instance);
            }
        }

        for (auto& [instance, effect] : _jeal_ctx._jeal_all_effects)
            _jeal_effect_drop(instance);

        if (AL_FALSE == alcMakeContextCurrent(nullptr))
            jeecs::debug::logerr("Failed to clear current context.");
        alcDestroyContext(current_context);
        return true;
    }
    return false;
}
bool _jeal_startup_specify_device(jeal_device* device)
{
    assert(device != nullptr);
    auto* current_context = alcCreateContext(device->m_openal_device, nullptr);
    if (current_context == nullptr)
    {
        jeecs::debug::logerr("Failed to create context for device: %s.", device->m_device_name);
        return false;
    }
    else if (AL_FALSE == alcMakeContextCurrent(current_context))
    {
        jeecs::debug::logerr("Failed to active context for device: %s.", device->m_device_name);
        return false;
    }
    jeecs::debug::loginfo("Audio device: %s, has been enabled.", device->m_device_name);

    if (alcIsExtensionPresent(device->m_openal_device, ALC_EXT_EFX_NAME))
    {
        alcGetIntegerv(
            device->m_openal_device,
            ALC_MAX_AUXILIARY_SENDS,
            1,
            &_jeal_ctx._jeal_efx_enabled_sends_for_each_source);

        _jeal_ctx._jeal_efx_enabled_sends_for_each_source = std::min(
            _jeal_ctx._jeal_efx_enabled_sends_for_each_source,
            JEAL_MAX_SOURCE_BINDING_EFFECT_SLOT_COUNT);
    }
    else
        _jeal_ctx._jeal_efx_enabled_sends_for_each_source = 0;

    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source == 0)
        jeecs::debug::logwarn(
            "Audio device: %s, does not support EFX.",
            device->m_device_name);
    else
    {
        jeecs::debug::loginfo(
            "Audio device: %s, support EFX with max %d sends for each source.",
            device->m_device_name,
            (int)_jeal_ctx._jeal_efx_enabled_sends_for_each_source);

#define JEAL_LOAD_FUNC(name) \
        _jeal_ctx.impl_##name = (decltype(_jeal_ctx.impl_##name))alcGetProcAddress(device->m_openal_device, #name); \
        if (_jeal_ctx.impl_##name == nullptr) \
        { \
            jeecs::debug::logfatal("Failed to load al function: %s.", #name); \
            return false; \
        }

        JEAL_LOAD_FUNC(alGenEffects);
        JEAL_LOAD_FUNC(alDeleteEffects);
        JEAL_LOAD_FUNC(alEffecti);
        JEAL_LOAD_FUNC(alEffectf);
        JEAL_LOAD_FUNC(alEffectfv);
        JEAL_LOAD_FUNC(alGenAuxiliaryEffectSlots);
        JEAL_LOAD_FUNC(alDeleteAuxiliaryEffectSlots);
        JEAL_LOAD_FUNC(alAuxiliaryEffectSloti);
        JEAL_LOAD_FUNC(alAuxiliaryEffectSlotf);
        JEAL_LOAD_FUNC(alGenFilters);
        JEAL_LOAD_FUNC(alDeleteFilters);
        JEAL_LOAD_FUNC(alFilteri);
        JEAL_LOAD_FUNC(alFilterf);
    }

    _jeal_ctx._jeal_current_device = device;

    return true;
}
void _jeal_store_current_context(_jeal_global_context* out_context)
{
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        // 保存listener信息
        alGetListener3f(AL_POSITION,
            &out_context->listener_information.m_position.x,
            &out_context->listener_information.m_position.y,
            &out_context->listener_information.m_position.z);
        alGetListener3f(AL_VELOCITY,
            &out_context->listener_information.m_velocity.x,
            &out_context->listener_information.m_velocity.y,
            &out_context->listener_information.m_velocity.z);
        alGetListenerfv(AL_ORIENTATION, out_context->listener_orientation);
        alGetListenerf(AL_GAIN,
            &out_context->listener_information.m_volume);

        // 遍历所有 source，获取相关信息，在创建新的上下文之后恢复
        for (auto* source : _jeal_ctx._jeal_all_sources)
        {
            _jeal_global_context::source_restoring_information src_info;
            src_info.m_source = source;
            alGetSource3f(source->m_openal_source, AL_POSITION,
                &src_info.m_position.x,
                &src_info.m_position.y,
                &src_info.m_position.z);
            alGetSource3f(source->m_openal_source, AL_VELOCITY,
                &src_info.m_velocity.x,
                &src_info.m_velocity.y,
                &src_info.m_velocity.z);

            ALint byte_offset;
            alGetSourcei(source->m_openal_source, AL_BYTE_OFFSET, &byte_offset);
            src_info.m_offset = (size_t)byte_offset;

            alGetSourcef(source->m_openal_source, AL_PITCH,
                &src_info.m_pitch);
            alGetSourcef(source->m_openal_source, AL_GAIN,
                &src_info.m_volume);
            alGetSourcei(source->m_openal_source, AL_LOOPING,
                &src_info.m_loop);

            ALint state;
            alGetSourcei(source->m_openal_source, AL_SOURCE_STATE, &state);

            switch (state)
            {
            case AL_PLAYING:
                src_info.m_state = jeal_state::JE_AUDIO_STATE_PLAYING; break;
            case AL_PAUSED:
                src_info.m_state = jeal_state::JE_AUDIO_STATE_PAUSED; break;
            default:
                assert(state == AL_INITIAL || state == AL_STOPPED);
                src_info.m_state = jeal_state::JE_AUDIO_STATE_STOPPED; break;
            }
            src_info.m_playing_buffer = source->m_last_played_buffer;

            out_context->sources_information.emplace_back(src_info);
        }
    }
    else
    {
        out_context->listener_information.m_volume = 1.0f;
    }
}

void _jeal_effect_slot_set_effect(jeal_effect_slot* slot, void* effect_may_null)
{
    slot->m_binded_effect_may_null = effect_may_null;

    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        if (effect_may_null != nullptr)
        {
            auto& effect = _jeal_ctx._jeal_all_effects.at(effect_may_null);
            _jeal_ctx.impl_alAuxiliaryEffectSloti(
                slot->m_effect_slot_instance,
                AL_EFFECTSLOT_EFFECT,
                effect.m_effect_instance);
        }
        else
        {
            _jeal_ctx.impl_alAuxiliaryEffectSloti(
                slot->m_effect_slot_instance,
                AL_EFFECTSLOT_EFFECT,
                AL_EFFECTSLOT_NULL);
        }
    }
}

void _jeal_restore_context(const _jeal_global_context* context)
{
    // OK, Restore buffer, listener and source.
    for (auto* buffer : _jeal_ctx._jeal_all_buffers)
        _jeal_update_buffer_instance(buffer);

    for (auto& [instance, effect] : _jeal_ctx._jeal_all_effects)
    {
        if (!effect.m_effect_instance_exist)
            _jeal_create_effect_instance(instance, effect.m_effect_kind);

        _jeal_update_effect_instance(instance, &effect);
    }

    for (auto& effect_slot : _jeal_ctx._jeal_all_effect_slots)
    {
        if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
        {
            _jeal_ctx.impl_alGenAuxiliaryEffectSlots(
                1, &effect_slot->m_effect_slot_instance);
            _jeal_ctx.impl_alAuxiliaryEffectSlotf(
                effect_slot->m_effect_slot_instance,
                AL_EFFECTSLOT_GAIN,
                effect_slot->m_gain);
        }

        // 此处需要检查 effect 是否依然有效；因为 effect 可能在未解除绑定的情况下被删除
        if (effect_slot->m_binded_effect_may_null == nullptr
            || _jeal_ctx._jeal_all_effects.end() != _jeal_ctx._jeal_all_effects.find(effect_slot->m_binded_effect_may_null))
        {
            _jeal_effect_slot_set_effect(effect_slot, effect_slot->m_binded_effect_may_null);
        }
    }

    alListener3f(AL_POSITION,
        context->listener_information.m_position.x,
        context->listener_information.m_position.y,
        context->listener_information.m_position.z);
    alListener3f(AL_VELOCITY,
        context->listener_information.m_velocity.x,
        context->listener_information.m_velocity.y,
        context->listener_information.m_velocity.z);
    float orientation[] = {
        context->listener_orientation[0],
        context->listener_orientation[1],
        context->listener_orientation[2],
        context->listener_orientation[3],
        context->listener_orientation[4],
        context->listener_orientation[5]
    };
    alListenerfv(AL_ORIENTATION, orientation);
    alListenerf(AL_GAIN, context->listener_information.m_volume);

    for (auto& src_info : context->sources_information)
    {
        _jeal_update_source(src_info.m_source);

        alSource3f(src_info.m_source->m_openal_source, AL_POSITION,
            src_info.m_position.x,
            src_info.m_position.y,
            src_info.m_position.z);
        alSource3f(src_info.m_source->m_openal_source, AL_VELOCITY,
            src_info.m_velocity.x,
            src_info.m_velocity.y,
            src_info.m_velocity.z);
        alSourcef(src_info.m_source->m_openal_source, AL_PITCH, src_info.m_pitch);
        alSourcef(src_info.m_source->m_openal_source, AL_GAIN, src_info.m_volume);
        alSourcei(src_info.m_source->m_openal_source, AL_LOOPING, src_info.m_loop != 0 ? 1 : 0);

        if (src_info.m_state != jeal_state::JE_AUDIO_STATE_STOPPED)
        {
            assert(src_info.m_playing_buffer != nullptr);

            alSourcei(src_info.m_source->m_openal_source, AL_BUFFER, src_info.m_playing_buffer->m_openal_buffer);
            alSourcei(src_info.m_source->m_openal_source, AL_BYTE_OFFSET, (ALint)src_info.m_offset);

            if (src_info.m_state == jeal_state::JE_AUDIO_STATE_PAUSED)
                alSourcePause(src_info.m_source->m_openal_source);
            else
                alSourcePlay(src_info.m_source->m_openal_source);
        }
    }
}
jeal_device** _jeal_update_refetch_devices(size_t* out_len)
{
    auto old_devices = _jeal_ctx._jeal_all_devices;
    _jeal_ctx._jeal_all_devices.clear();

    for (auto* old_device : old_devices)
    {
        // 将所有旧设备标记为已经断开，等待后续检查
        old_device->m_alive = false;
    }

    // 如果支持枚举所有设备，就把枚举所有设备并全部打开
    const char* device_names = nullptr;

    // 枚举播放设备：
    auto enum_device_enabled = alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT");
    if (enum_device_enabled)
        device_names = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
    else
        device_names = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

    const char* current_device_name = device_names;

    while (current_device_name && *current_device_name != 0)
    {
        jeal_device* current_device = nullptr;

        auto fnd = std::find_if(old_devices.begin(), old_devices.end(),
            [current_device_name](jeal_device* device)
            {
                return 0 == strcmp(current_device_name, device->m_device_name);
            });

        // 检查，如果这是一个新枚举产生的设备名，那么就打开这个设备
        if (fnd == old_devices.end())
        {
        label_reconnect_device:
            auto* device_instance = alcOpenDevice(current_device_name);

            jeecs::debug::loginfo("Found audio device: %s.", current_device_name);

            if (device_instance == nullptr)
                jeecs::debug::logerr("Failed to open device: '%s'.", current_device_name);
            else
            {
                current_device = new jeal_device;
                current_device->m_device_name = jeecs::basic::make_new_string(current_device_name);
                current_device->m_openal_device = device_instance;
            }
        }
        else
        {
            // 这是一个已经存在的设备。
            current_device = *fnd;

            // 检查其是否曾经断开，然后重新链接上的。
            ALCint device_connected;
            alcGetIntegerv(current_device->m_openal_device, ALC_CONNECTED, 1, &device_connected);

            if (device_connected == ALC_FALSE)
            {
                // 之前创建的设备已经断开，需要重新创建
                goto label_reconnect_device;
            }
        }

        if (current_device != nullptr)
        {
            // 将设备添加到设备列表中
            current_device->m_alive = true;
            _jeal_ctx._jeal_all_devices.push_back(current_device);
        }

        if (!enum_device_enabled)
            break;

        current_device_name += strlen(current_device_name) + 1;
    }

    if (_jeal_ctx._jeal_current_device != nullptr
        && _jeal_ctx._jeal_current_device->m_alive == false)
        _jeal_shutdown_current_device();

    for (auto* closed_device : old_devices)
    {
        if (closed_device->m_alive == false)
        {
            assert(closed_device != nullptr);
            alcCloseDevice(closed_device->m_openal_device);

            jeecs::debug::loginfo("Audio device: %s closed.", closed_device->m_device_name);
            je_mem_free((void*)closed_device->m_device_name);

            delete closed_device;
        }
    }

    if (_jeal_ctx._jeal_all_devices.size() == 0)
        jeecs::debug::logwarn("No audio device found.");

    *out_len = _jeal_ctx._jeal_all_devices.size();
    return _jeal_ctx._jeal_all_devices.data();
}
jeal_capture_device** _jeal_update_refetch_capture_devices(size_t* out_len)
{
    auto old_capture_devices = _jeal_ctx._jeal_all_capture_devices;
    _jeal_ctx._jeal_all_capture_devices.clear();

    // 如果支持枚举所有设备，就把枚举所有设备并全部打开
    const char* device_names = nullptr;

    // 枚举录音设备：
    auto enum_device_enabled = alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT");
    if (enum_device_enabled)
        device_names = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    else
        device_names = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);

    const char* current_device_name = device_names;

    while (current_device_name && *current_device_name != 0)
    {
        jeal_capture_device* current_device = nullptr;

        auto fnd = std::find_if(old_capture_devices.begin(), old_capture_devices.end(),
            [current_device_name](jeal_capture_device* device)
            {
                return 0 == strcmp(current_device_name, device->m_device_name);
            });

        // 检查，如果这是一个新枚举产生的设备名，那么就直接创建实例
        if (fnd == old_capture_devices.end())
        {
            jeecs::debug::loginfo("Found audio capture device: %s.", current_device_name);

            current_device = new jeal_capture_device;
            current_device->m_device_name = jeecs::basic::make_new_string(current_device_name);
        }
        else
        {
            // 设备已经存在，如果 m_capturing_device 非空，说明此设备此前正在录音
            // 根据约定，所有录音设备在更新操作发生时都应该停止录音（即设备应该被假定失效）
            current_device = *fnd;

            if (current_device->m_capturing_device)
                alcCaptureCloseDevice(current_device->m_capturing_device);
        }

        // Make sure the device is alive.
        current_device->m_alive = true;

        current_device->m_capturing_device = nullptr;
        current_device->m_size_per_sample = 0;

        _jeal_ctx._jeal_all_capture_devices.push_back(current_device);

        if (!enum_device_enabled)
            break;

        current_device_name += strlen(current_device_name) + 1;
    }

    for (auto* closed_device : old_capture_devices)
    {
        if (closed_device->m_alive == false)
        {
            if (closed_device->m_capturing_device != nullptr)
                alcCaptureCloseDevice(closed_device->m_capturing_device);

            jeecs::debug::loginfo("Audio capture device: %s closed.", closed_device->m_device_name);
            je_mem_free((void*)closed_device->m_device_name);

            delete closed_device;
        }
    }

    if (_jeal_ctx._jeal_all_capture_devices.size() == 0)
        jeecs::debug::logwarn("No audio capture device found.");

    *out_len = _jeal_ctx._jeal_all_capture_devices.size();
    return _jeal_ctx._jeal_all_capture_devices.data();
}

jeal_device** jeal_get_all_devices(size_t* out_len)
{
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);
    std::lock_guard g9(_jeal_ctx._jeal_context_mx);

    _jeal_global_context context;

    bool need_try_restart_device = false;
    if (_jeal_ctx._jeal_current_device != nullptr)
    {
        _jeal_store_current_context(&context);
        need_try_restart_device = true;
    }

    // NOTE: 若当前设备在枚举过程中寄了，那么当前设备会被 _jeal_update_refetch_devices
    //       负责关闭，同时_jeal_ctx._jeal_current_device被置为nullptr
    jeal_device** result = _jeal_update_refetch_devices(out_len);

    if (need_try_restart_device && _jeal_ctx._jeal_current_device == nullptr)
    {
        // 之前的设备寄了，但是设备已经指定，从列表里捞一个
        if (*out_len != 0)
        {
            _jeal_startup_specify_device(result[0]);
            _jeal_restore_context(&context);
        }
    }
    return result;
}

jeal_capture_device** jeal_refetch_all_capture_devices(size_t* out_len)
{
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);
    std::lock_guard g9(_jeal_ctx._jeal_context_mx);

    jeal_capture_device** result = _jeal_update_refetch_capture_devices(out_len);
    return result;
}

ALenum _jeal_engine_format_to_al_format(jeal_format format, size_t* out_byte_per_sample)
{
    switch (format)
    {
    case JE_AUDIO_FORMAT_MONO8:
        *out_byte_per_sample = 1;
        return AL_FORMAT_MONO8;
    case JE_AUDIO_FORMAT_MONO16:
        *out_byte_per_sample = 2;
        return AL_FORMAT_MONO16;
    case JE_AUDIO_FORMAT_MONO32F:
        *out_byte_per_sample = 4;
        return AL_FORMAT_MONO_FLOAT32;
    case JE_AUDIO_FORMAT_STEREO8:
        *out_byte_per_sample = 2;
        return AL_FORMAT_STEREO8;
    case JE_AUDIO_FORMAT_STEREO16:
        *out_byte_per_sample = 4;
        return AL_FORMAT_STEREO16;
    case JE_AUDIO_FORMAT_STEREO32F:
        *out_byte_per_sample = 8;
        return AL_FORMAT_STEREO_FLOAT32;
    default:
        *out_byte_per_sample = 0;
        return AL_NONE;
    }
}
size_t jeal_capture_device_open(
    jeal_capture_device* device,
    size_t buffer_len,
    size_t sample_rate,
    jeal_format format)
{
    size_t byte_per_sample;
    auto al_format = _jeal_engine_format_to_al_format(format, &byte_per_sample);
    if (al_format == AL_NONE)
    {
        jeecs::debug::logerr("Bad audio buffer format: %d.", (int)format);
        return 0;
    }
    if (buffer_len % byte_per_sample)
    {
        jeecs::debug::logerr("Buffer length is not aligned.");
        return 0;
    }

    std::lock_guard g9(_jeal_ctx._jeal_context_mx);

    if (device->m_capturing_device)
    {
        // 捕获设备已经开启，关掉它
        if (AL_FALSE == alcCaptureCloseDevice(device->m_capturing_device))
        {
            // Some error happend.
            jeecs::debug::logerr("Failed to restart capture device `%s`: %d.",
                device->m_device_name, (int)alcGetError(device->m_capturing_device));
            return 0;
        }

        device->m_capturing_device = nullptr;
    }

    device->m_capturing_device = alcCaptureOpenDevice(
        device->m_device_name,
        sample_rate,
        al_format,
        buffer_len);
    if (device->m_capturing_device == nullptr)
    {
        jeecs::debug::logerr("Failed to open capture device `%s`.", device->m_device_name);
        return 0;
    }
    return (device->m_size_per_sample = byte_per_sample);
}
void jeal_capture_device_start(jeal_capture_device* device)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    if (device->m_capturing_device == nullptr)
    {
        jeecs::debug::logerr("Capture device `%s` is not opened.", device->m_device_name);
        return;
    }
    alcCaptureStart(device->m_capturing_device);
}
void jeal_capture_device_stop(jeal_capture_device* device)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    if (device->m_capturing_device == nullptr)
    {
        jeecs::debug::logerr("Capture device `%s` is not opened.", device->m_device_name);
        return;
    }
    alcCaptureStop(device->m_capturing_device);
}
bool jeal_capture_device_sample(
    jeal_capture_device* device,
    void* out_buffer,
    size_t buffer_len,
    size_t* out_sampled_len,
    size_t* out_remaining_len)
{
    if (device->m_capturing_device == nullptr)
    {
        jeecs::debug::logerr("Capture device `%s` is not opened.", device->m_device_name);
        return false;
    }

    if (buffer_len % device->m_size_per_sample != 0)
    {
        jeecs::debug::logerr("Buffer length is not aligned.");
        return false;
    }

    std::lock_guard g9(_jeal_ctx._jeal_context_mx);

    ALCint samples_available;
    alcGetIntegerv(device->m_capturing_device, ALC_CAPTURE_SAMPLES, 1, &samples_available);

    const size_t available_sample_len = (size_t)samples_available * device->m_size_per_sample;
    *out_sampled_len = std::min(buffer_len, available_sample_len);
    *out_remaining_len = available_sample_len - *out_sampled_len;

    if (*out_sampled_len == 0)
        return true;

    alcCaptureSamples(device->m_capturing_device, out_buffer, *out_sampled_len / device->m_size_per_sample);
    auto device_error = alcGetError(device->m_capturing_device);
    if (device_error != AL_NO_ERROR)
    {
        // Some error happend.
        jeecs::debug::logerr("Failed to capture samples from device `%s`: %d.",
            device->m_device_name, (int)device_error);
        return false;
    }

    return true;
}
void jeal_init()
{
    size_t device_count = 0;
    auto** devices = jeal_get_all_devices(&device_count);
    if (device_count != 0)
        // Using first device as default.
        jeal_using_device(devices[0]);
}

void jeal_finish()
{
    // 在此检查是否有尚未关闭的声源和声音缓存，按照规矩，此处应该全部清退干净
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);
    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::lock_guard g2(_jeal_ctx._jeal_all_buffers_mx);
    std::lock_guard g9(_jeal_ctx._jeal_context_mx);

    assert(_jeal_ctx._jeal_all_sources.empty());
    assert(_jeal_ctx._jeal_all_buffers.empty());

    _jeal_shutdown_current_device();
    for (auto* source : _jeal_ctx._jeal_all_sources)
    {
        delete source;
    }
    for (auto* buffer : _jeal_ctx._jeal_all_buffers)
    {
        delete buffer;
    }
    for (auto& [effect, instance] : _jeal_ctx._jeal_all_effects)
    {
        assert(!instance.m_effect_instance_exist);
        free(effect);
    }
    for (auto* effect_slot : _jeal_ctx._jeal_all_effect_slots)
    {
        delete effect_slot;
    }
    for (auto* device : _jeal_ctx._jeal_all_devices)
    {
        assert(device != nullptr);
        alcCloseDevice(device->m_openal_device);

        jeecs::debug::loginfo("Audio device: %s closed.", device->m_device_name);
        je_mem_free((void*)device->m_device_name);

        delete device;
    }
    for (auto* device : _jeal_ctx._jeal_all_capture_devices)
    {
        if (device->m_capturing_device)
            alcCaptureCloseDevice(device->m_capturing_device);

        jeecs::debug::loginfo("Audio record device: %s closed.", device->m_device_name);
        je_mem_free((void*)device->m_device_name);

        delete device;
    }
    _jeal_ctx._jeal_all_devices.clear();
}

const char* jeal_device_name(jeal_device* device)
{
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);

    assert(device != nullptr);
    return device->m_device_name;
}

const char* jeal_captuer_device_name(jeal_capture_device* device)
{
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);

    assert(device != nullptr);
    return device->m_device_name;
}

bool jeal_using_device(jeal_device* device)
{
    std::lock_guard g0(_jeal_ctx._jeal_all_devices_mx);
    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::lock_guard g2(_jeal_ctx._jeal_all_buffers_mx);

    auto fnd = std::find(_jeal_ctx._jeal_all_devices.begin(), _jeal_ctx._jeal_all_devices.end(), device);
    if (fnd == _jeal_ctx._jeal_all_devices.end())
    {
        jeecs::debug::logerr("Trying to use invalid device: %p", device);
        return false;
    }
    if (device != _jeal_ctx._jeal_current_device)
    {
        std::lock_guard g9(_jeal_ctx._jeal_context_mx);

        _jeal_global_context context;
        _jeal_store_current_context(&context);
        _jeal_shutdown_current_device();
        _jeal_startup_specify_device(device);
        _jeal_restore_context(&context);
    }
    return true;
}

jeal_source* jeal_open_source()
{
    jeal_source* audio_source = new jeal_source;

    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_sources.insert(audio_source);

    audio_source->m_last_played_buffer = nullptr;
    for (auto& effect_slot : audio_source->m_binded_effect_slots_may_null)
        effect_slot = nullptr;

    _jeal_update_source(audio_source);

    return audio_source;
}

void jeal_close_source(jeal_source* source)
{
    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    {
        _jeal_ctx._jeal_all_sources.erase(source);
    }
    alDeleteSources(1, &source->m_openal_source);
    delete source;
}

jeal_buffer* jeal_load_buffer_from_wav(const char* filename)
{
    //Wav文件数据体模块
    struct WAVE_Data {
        char subChunkID[4]; //should contain the word data
        int32_t subChunk2Size; //Stores the size of the data block
    };
    //wav文件数据参数类型
    struct WAVE_Format {
        char subChunkID[4];
        int32_t subChunkSize;
        int16_t audioFormat;
        int16_t numChannels;
        int32_t sampleRate;
        int32_t byteRate;
        int16_t blockAlign;
        int16_t bitsPerSample;
    };
    //RIFF块标准模型
    struct RIFF_Header {
        char chunkID[4];
        int32_t chunkSize;//size not including chunkSize or chunkID
        char format[4];
    };

    WAVE_Format wave_format;
    RIFF_Header riff_header;
    WAVE_Data wave_data;

    jeecs_file* wav_file = jeecs_file_open(filename);
    if (wav_file == nullptr)
    {
        jeecs::debug::logerr("Failed to open file: '%s', unable to create audio buffer.", filename);
        return nullptr;
    }
    // Read in the first chunk into the struct

    jeecs_file_read(&riff_header, sizeof(RIFF_Header), 1, wav_file);

    //check for RIFF and WAVE tag in memeory
    if ((riff_header.chunkID[0] != 'R' ||
        riff_header.chunkID[1] != 'I' ||
        riff_header.chunkID[2] != 'F' ||
        riff_header.chunkID[3] != 'F') ||
        (riff_header.format[0] != 'W' ||
            riff_header.format[1] != 'A' ||
            riff_header.format[2] != 'V' ||
            riff_header.format[3] != 'E'))
    {
        jeecs::debug::logerr("Invalid wav file: '%s', bad format tag.", filename);
        jeecs_file_close(wav_file);
        return nullptr;
    }

    //Read in the 2nd chunk for the wave info
    jeecs_file_read(&wave_format, sizeof(WAVE_Format), 1, wav_file);
    //check for fmt tag in memory
    if (wave_format.subChunkID[0] != 'f' ||
        wave_format.subChunkID[1] != 'm' ||
        wave_format.subChunkID[2] != 't' ||
        wave_format.subChunkID[3] != ' ')
    {
        jeecs::debug::logerr("Invalid wav file: '%s', bad format head.", filename);
        jeecs_file_close(wav_file);
        return nullptr;
    }

    //check for extra parameters;
    if (wave_format.subChunkSize > 16)
    {
        int16_t useless;
        jeecs_file_read(&useless, sizeof(int16_t), 1, wav_file);
    }

    //Read in the the last byte of data before the sound file
    jeecs_file_read(&wave_data, sizeof(WAVE_Data), 1, wav_file);
    //check for data tag in memory
    if (wave_data.subChunkID[0] != 'd' ||
        wave_data.subChunkID[1] != 'a' ||
        wave_data.subChunkID[2] != 't' ||
        wave_data.subChunkID[3] != 'a')
    {
        jeecs::debug::logerr("Invalid wav file: '%s', bad data head.", filename);
        jeecs_file_close(wav_file);
        return nullptr;
    }

    //Allocate memory for data
    void* data = malloc(wave_data.subChunk2Size);

    // Read in the sound data into the soundData variable
    if (!jeecs_file_read(data, wave_data.subChunk2Size, 1, wav_file))
    {
        jeecs::debug::logerr("Invalid wav file: '%s', bad data size.", filename);
        free(data);
        jeecs_file_close(wav_file);
        return nullptr;
    }

    jeecs_file_close(wav_file);

    if ((wave_format.byteRate !=
        wave_format.numChannels * wave_format.bitsPerSample * wave_format.sampleRate / 8)
        ||
        (wave_format.blockAlign !=
            wave_format.numChannels * wave_format.bitsPerSample / 8))
    {
        jeecs::debug::logerr("Invalid wav file: '%s', bad data format.", filename);
        free(data);
        return nullptr;
    }

    //Now we set the variables that we passed in with the
    //data from the structs
    jeal_buffer* audio_buffer = new jeal_buffer;
    audio_buffer->m_data = data;
    audio_buffer->m_size = wave_data.subChunk2Size;
    audio_buffer->m_sample_rate = wave_format.sampleRate;
    audio_buffer->m_byterate = wave_format.byteRate;
    audio_buffer->m_format = AL_NONE;

    //The format is worked out by looking at the number of
    //channels and the bits per sample.
    if (wave_format.numChannels == 1)
    {
        if (wave_format.bitsPerSample == 8)
            audio_buffer->m_format = AL_FORMAT_MONO8;
        else if (wave_format.bitsPerSample == 16)
            audio_buffer->m_format = AL_FORMAT_MONO16;
        else if (wave_format.bitsPerSample == 32)
            audio_buffer->m_format = AL_FORMAT_MONO_FLOAT32;
    }
    else if (wave_format.numChannels == 2)
    {
        if (wave_format.bitsPerSample == 8)
            audio_buffer->m_format = AL_FORMAT_STEREO8;
        else if (wave_format.bitsPerSample == 16)
            audio_buffer->m_format = AL_FORMAT_STEREO16;
        else if (wave_format.bitsPerSample == 32)
            audio_buffer->m_format = AL_FORMAT_STEREO_FLOAT32;
    }

    std::lock_guard g1(_jeal_ctx._jeal_all_buffers_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

jeal_buffer* jeal_create_buffer(
    const void* buffer_data,
    size_t buffer_data_len,
    size_t sample_rate,
    jeal_format format)
{
    size_t byte_per_sample;
    auto al_format = _jeal_engine_format_to_al_format(format, &byte_per_sample);
    if (al_format == AL_NONE)
    {
        jeecs::debug::logerr("Bad audio buffer format: %d.", (int)format);
        return nullptr;
    }

    if (buffer_data_len % byte_per_sample != 0)
    {
        jeecs::debug::logerr("Audio buffer size is not aligned.");
        return nullptr;
    }

    jeal_buffer* audio_buffer = new jeal_buffer;

    void* copy_buffer_data = malloc(buffer_data_len);
    memcpy(copy_buffer_data, buffer_data, buffer_data_len);

    audio_buffer->m_data = copy_buffer_data;
    audio_buffer->m_size = buffer_data_len;
    audio_buffer->m_sample_rate = sample_rate;
    audio_buffer->m_format = al_format;
    audio_buffer->m_byterate = byte_per_sample * audio_buffer->m_sample_rate;

    std::lock_guard g1(_jeal_ctx._jeal_all_buffers_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

void jeal_close_buffer(jeal_buffer* buffer)
{
    std::lock_guard g1(_jeal_ctx._jeal_all_buffers_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    {
        _jeal_ctx._jeal_all_buffers.erase(buffer);
    }
    alDeleteBuffers(1, &buffer->m_openal_buffer);
    free((void*)buffer->m_data);
    delete buffer;
}

size_t jeal_buffer_byte_size(jeal_buffer* buffer)
{
    return buffer->m_size;
}

size_t jeal_buffer_byte_rate(jeal_buffer* buffer)
{
    return buffer->m_byterate;
}

void jeal_source_set_buffer(jeal_source* source, jeal_buffer* buffer)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    source->m_last_played_buffer = buffer;
    alSourcei(source->m_openal_source, AL_BUFFER, buffer->m_openal_buffer);
}

void jeal_source_loop(jeal_source* source, bool loop)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcei(source->m_openal_source, AL_LOOPING, loop ? 1 : 0);
}

void jeal_source_play(jeal_source* source)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcePlay(source->m_openal_source);
}

void jeal_source_pause(jeal_source* source)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcePause(source->m_openal_source);
}

void jeal_source_stop(jeal_source* source)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourceStop(source->m_openal_source);
}

void jeal_source_position(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSource3f(source->m_openal_source, AL_POSITION, x, y, -z);
}
void jeal_source_velocity(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSource3f(source->m_openal_source, AL_VELOCITY, x, y, -z);
}

size_t jeal_source_get_byte_offset(jeal_source* source)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    ALint byte_offset;
    alGetSourcei(source->m_openal_source, AL_BYTE_OFFSET, &byte_offset);
    return byte_offset;
}

void jeal_source_set_byte_offset(jeal_source* source, size_t byteoffset)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcei(source->m_openal_source, AL_BYTE_OFFSET, (ALint)byteoffset);
}

void jeal_source_pitch(jeal_source* source, float playspeed)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcef(source->m_openal_source, AL_PITCH, playspeed);
}

void jeal_source_volume(jeal_source* source, float volume)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alSourcef(source->m_openal_source, AL_GAIN, volume);
}

jeal_state jeal_source_get_state(jeal_source* source)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    ALint state;
    alGetSourcei(source->m_openal_source, AL_SOURCE_STATE, &state);

    switch (state)
    {
    case AL_PLAYING:
        return jeal_state::JE_AUDIO_STATE_PLAYING;
    case AL_PAUSED:
        return jeal_state::JE_AUDIO_STATE_PAUSED;
    default:
        assert(state == AL_INITIAL || state == AL_STOPPED);
        return jeal_state::JE_AUDIO_STATE_STOPPED;
    }
}

void jeal_listener_position(float x, float y, float z)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alListener3f(AL_POSITION, x, y, -z);
}

void jeal_listener_velocity(float x, float y, float z)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    alListener3f(AL_VELOCITY, x, y, -z);
}

void jeal_listener_direction(
    float face_x, float face_y, float face_z,
    float top_x, float top_y, float top_z)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    float orientation[] = {
       face_x, face_y, -face_z,
       top_x, top_y, -top_z,
    };

    alListenerfv(AL_ORIENTATION, orientation);
}

void jeal_listener_volume(float volume)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_listener_gain.store(volume);

    float global_gain;
    do
    {
        global_gain = _jeal_ctx._jeal_global_volume_gain.load();
        alListenerf(AL_GAIN, _jeal_ctx._jeal_listener_gain.load() * global_gain);
    } while (_jeal_ctx._jeal_global_volume_gain.load() != global_gain);
}

void jeal_global_volume(float volume)
{
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_global_volume_gain.store(volume);

    float listener_gain;
    do
    {
        listener_gain = _jeal_ctx._jeal_listener_gain.load();
        alListenerf(AL_GAIN, _jeal_ctx._jeal_global_volume_gain.load() * listener_gain);
    } while (_jeal_ctx._jeal_listener_gain.load() != listener_gain);
}

void jeal_effect_close(void* effect)
{
    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_effect_drop(effect);

    assert(!_jeal_ctx._jeal_all_effects[effect].m_effect_instance_exist);

    free(effect);
    _jeal_ctx._jeal_all_effects.erase(effect);
}

jeal_effect_reverb* jeal_create_effect_reverb()
{
    jeal_effect_reverb* instance = (jeal_effect_reverb*)malloc(sizeof(jeal_effect_reverb));
    assert(instance != nullptr);

    instance->m_density = AL_REVERB_DEFAULT_DENSITY;
    instance->m_diffusion = AL_REVERB_DEFAULT_DIFFUSION;
    instance->m_gain = AL_REVERB_DEFAULT_GAIN;
    instance->m_gain_hf = AL_REVERB_DEFAULT_GAINHF;
    instance->m_decay_time = AL_REVERB_DEFAULT_DECAY_TIME;
    instance->m_decay_hf_ratio = AL_REVERB_DEFAULT_DECAY_HFRATIO;
    instance->m_reflections_gain = AL_REVERB_DEFAULT_REFLECTIONS_GAIN;
    instance->m_reflections_delay = AL_REVERB_DEFAULT_REFLECTIONS_DELAY;
    instance->m_late_reverb_gain = AL_REVERB_DEFAULT_LATE_REVERB_GAIN;
    instance->m_late_reverb_delay = AL_REVERB_DEFAULT_LATE_REVERB_DELAY;
    instance->m_air_absorption_gain_hf = AL_REVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
    instance->m_room_rolloff_factor = AL_REVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
    instance->m_decay_hf_limit = (bool)AL_REVERB_DEFAULT_DECAY_HFLIMIT;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_REVERB);

    return instance;
}
jeal_effect_chorus* jeal_create_effect_chorus()
{
    jeal_effect_chorus* instance = (jeal_effect_chorus*)malloc(sizeof(jeal_effect_chorus));
    assert(instance != nullptr);

    instance->m_waveform = (jeal_effect_chorus::wavefrom)AL_CHORUS_DEFAULT_WAVEFORM;
    instance->m_phase = AL_CHORUS_DEFAULT_PHASE;
    instance->m_rate = AL_CHORUS_DEFAULT_RATE;
    instance->m_depth = AL_CHORUS_DEFAULT_DEPTH;
    instance->m_feedback = AL_CHORUS_DEFAULT_FEEDBACK;
    instance->m_delay = AL_CHORUS_DEFAULT_DELAY;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_CHORUS);

    return instance;
}

jeal_effect_distortion* jeal_create_effect_distortion()
{
    jeal_effect_distortion* instance = (jeal_effect_distortion*)malloc(sizeof(jeal_effect_distortion));
    assert(instance != nullptr);

    instance->m_edge = AL_DISTORTION_DEFAULT_EDGE;
    instance->m_gain = AL_DISTORTION_DEFAULT_GAIN;
    instance->m_lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
    instance->m_equalizer_center_freq = AL_DISTORTION_DEFAULT_EQCENTER;
    instance->m_equalizer_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_DISTORTION);

    return instance;
}

jeal_effect_echo* jeal_create_effect_echo()
{
    jeal_effect_echo* instance = (jeal_effect_echo*)malloc(sizeof(jeal_effect_echo));
    assert(instance != nullptr);

    instance->m_delay = AL_ECHO_DEFAULT_DELAY;
    instance->m_lr_delay = AL_ECHO_DEFAULT_LRDELAY;
    instance->m_damping = AL_ECHO_DEFAULT_DAMPING;
    instance->m_feedback = AL_ECHO_DEFAULT_FEEDBACK;
    instance->m_spread = AL_ECHO_DEFAULT_SPREAD;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_ECHO);

    return instance;
}

jeal_effect_flanger* jeal_create_effect_flanger()
{
    jeal_effect_flanger* instance = (jeal_effect_flanger*)malloc(sizeof(jeal_effect_flanger));
    assert(instance != nullptr);

    instance->m_waveform = (jeal_effect_flanger::wavefrom)AL_FLANGER_DEFAULT_WAVEFORM;
    instance->m_phase = AL_FLANGER_DEFAULT_PHASE;
    instance->m_rate = AL_FLANGER_DEFAULT_RATE;
    instance->m_depth = AL_FLANGER_DEFAULT_DEPTH;
    instance->m_feedback = AL_FLANGER_DEFAULT_FEEDBACK;
    instance->m_delay = AL_FLANGER_DEFAULT_DELAY;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_FLANGER);

    return instance;
}

jeal_effect_frequency_shifter* jeal_create_effect_frequency_shifter()
{
    jeal_effect_frequency_shifter* instance = (jeal_effect_frequency_shifter*)malloc(sizeof(jeal_effect_frequency_shifter));
    assert(instance != nullptr);

    instance->m_frequency = AL_FREQUENCY_SHIFTER_DEFAULT_FREQUENCY;
    instance->m_left_direction = 
        (jeal_effect_frequency_shifter::direction)AL_FREQUENCY_SHIFTER_DEFAULT_LEFT_DIRECTION;
    instance->m_right_direction = 
        (jeal_effect_frequency_shifter::direction)AL_FREQUENCY_SHIFTER_DEFAULT_RIGHT_DIRECTION;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_create_effect_instance(instance, AL_EFFECT_FREQUENCY_SHIFTER);

    return instance;
}

jeal_effect_vocal_morpher* jeal_create_effect_vocal_morpher()
{
    jeal_effect_vocal_morpher* instance = (jeal_effect_vocal_morpher*)malloc(sizeof(jeal_effect_vocal_morpher));
    assert(instance != nullptr);

    instance->m_phoneme_a = (jeal_effect_vocal_morpher::phoneme)AL_VOCAL_MORPHER_DEFAULT_PHONEMEA;
    instance->m_phoneme_a_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEA_COARSE_TUNING;
    instance->m_phoneme_b = (jeal_effect_vocal_morpher::phoneme)AL_VOCAL_MORPHER_DEFAULT_PHONEMEB;
    instance->m_phoneme_b_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEB_COARSE_TUNING;
    instance->m_waveform = (jeal_effect_vocal_morpher::wavefrom)AL_VOCAL_MORPHER_DEFAULT_WAVEFORM;
    instance->m_rate = AL_VOCAL_MORPHER_DEFAULT_RATE;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_VOCAL_MORPHER);
    return instance;
}

jeal_effect_pitch_shifter* jeal_create_effect_pitch_shifter()
{
    jeal_effect_pitch_shifter* instance = (jeal_effect_pitch_shifter*)malloc(sizeof(jeal_effect_pitch_shifter));
    assert(instance != nullptr);

    instance->m_coarse_tune = AL_PITCH_SHIFTER_DEFAULT_COARSE_TUNE;
    instance->m_fine_tune = AL_PITCH_SHIFTER_DEFAULT_FINE_TUNE;
    
    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_PITCH_SHIFTER);
    return instance;
}

jeal_effect_ring_modulator* jeal_create_effect_ring_modulator()
{
    jeal_effect_ring_modulator* instance = (jeal_effect_ring_modulator*)malloc(sizeof(jeal_effect_ring_modulator));
    assert(instance != nullptr);

    instance->m_frequency = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
    instance->m_highpass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
    instance->m_waveform = (jeal_effect_ring_modulator::wavefrom)AL_RING_MODULATOR_DEFAULT_WAVEFORM;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_RING_MODULATOR);
    return instance;
}

jeal_effect_autowah* jeal_create_effect_autowah()
{
    jeal_effect_autowah* instance = (jeal_effect_autowah*)malloc(sizeof(jeal_effect_autowah));
    assert(instance != nullptr);

    instance->m_attack_time = AL_AUTOWAH_DEFAULT_ATTACK_TIME;
    instance->m_release_time = AL_AUTOWAH_DEFAULT_RELEASE_TIME;
    instance->m_resonance = AL_AUTOWAH_DEFAULT_RESONANCE;
    instance->m_peak_gain = AL_AUTOWAH_DEFAULT_PEAK_GAIN;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_AUTOWAH);
    return instance;
}

jeal_effect_compressor* jeal_create_effect_compressor()
{
    jeal_effect_compressor* instance = (jeal_effect_compressor*)malloc(sizeof(jeal_effect_compressor));
    assert(instance != nullptr);

    instance->m_enabled = AL_COMPRESSOR_DEFAULT_ONOFF;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_COMPRESSOR);
    return instance;
}

jeal_effect_equalizer* jeal_create_effect_equalizer()
{
    jeal_effect_equalizer* instance = (jeal_effect_equalizer*)malloc(sizeof(jeal_effect_equalizer));
    assert(instance != nullptr);

    instance->m_low_gain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
    instance->m_low_cutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
    instance->m_mid1_gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
    instance->m_mid1_center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
    instance->m_mid1_width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
    instance->m_mid2_gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
    instance->m_mid2_center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
    instance->m_mid2_width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
    instance->m_high_gain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
    instance->m_high_cutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_EQUALIZER);
    return instance;
}

jeal_effect_eaxreverb* jeal_create_effect_eaxreverb()
{
    jeal_effect_eaxreverb* instance = (jeal_effect_eaxreverb*)malloc(sizeof(jeal_effect_eaxreverb));
    assert(instance != nullptr);

    instance->m_density = AL_EAXREVERB_DEFAULT_DENSITY;
    instance->m_diffusion = AL_EAXREVERB_DEFAULT_DIFFUSION;
    instance->m_gain = AL_EAXREVERB_DEFAULT_GAIN;
    instance->m_gain_hf = AL_EAXREVERB_DEFAULT_GAINHF;
    instance->m_gain_lf = AL_EAXREVERB_DEFAULT_GAINLF;
    instance->m_decay_time = AL_EAXREVERB_DEFAULT_DECAY_TIME;
    instance->m_decay_hf_ratio = AL_EAXREVERB_DEFAULT_DECAY_HFRATIO;
    instance->m_decay_lf_ratio = AL_EAXREVERB_DEFAULT_DECAY_LFRATIO;
    instance->m_reflections_gain = AL_EAXREVERB_DEFAULT_REFLECTIONS_GAIN;
    instance->m_reflections_delay = AL_EAXREVERB_DEFAULT_REFLECTIONS_DELAY;
    instance->m_reflections_pan_xyz[0] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
    instance->m_reflections_pan_xyz[1] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
    instance->m_reflections_pan_xyz[2] = -AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
    instance->m_late_reverb_gain = AL_EAXREVERB_DEFAULT_LATE_REVERB_GAIN;
    instance->m_late_reverb_delay = AL_EAXREVERB_DEFAULT_LATE_REVERB_DELAY;
    instance->m_late_reverb_pan_xyz[0] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
    instance->m_late_reverb_pan_xyz[1] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
    instance->m_late_reverb_pan_xyz[2] = -AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
    instance->m_echo_time = AL_EAXREVERB_DEFAULT_ECHO_TIME;
    instance->m_echo_depth = AL_EAXREVERB_DEFAULT_ECHO_DEPTH;
    instance->m_modulation_time = AL_EAXREVERB_DEFAULT_MODULATION_TIME;
    instance->m_modulation_depth = AL_EAXREVERB_DEFAULT_MODULATION_DEPTH;
    instance->m_air_absorption_gain_hf = AL_EAXREVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
    instance->m_hf_reference = AL_EAXREVERB_DEFAULT_HFREFERENCE;
    instance->m_lf_reference = AL_EAXREVERB_DEFAULT_LFREFERENCE;
    instance->m_room_rolloff_factor = AL_EAXREVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
    instance->m_decay_hf_limiter = (bool)AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT;

    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);
    _jeal_create_effect_instance(instance, AL_EFFECT_EAXREVERB);
    return instance;
}

void jeal_effect_update(void* effect)
{
    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_update_effect_instance(effect, &_jeal_ctx._jeal_all_effects.at(effect));
}

jeal_effect_slot* jeal_create_effect_slot()
{
    jeal_effect_slot* slot = new jeal_effect_slot;

    slot->m_binded_effect_may_null = nullptr;
    slot->m_gain = 1.0f;

    std::lock_guard g4(_jeal_ctx._jeal_all_effect_slots_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        _jeal_ctx.impl_alGenAuxiliaryEffectSlots(
            1, &slot->m_effect_slot_instance);
        _jeal_ctx.impl_alAuxiliaryEffectSlotf(
            slot->m_effect_slot_instance,
            AL_EFFECTSLOT_GAIN,
            slot->m_gain);
    }

    _jeal_ctx._jeal_all_effect_slots.insert(slot);
    return slot;
}

void jeal_effect_slot_close(jeal_effect_slot* slot)
{
    std::lock_guard g4(_jeal_ctx._jeal_all_effect_slots_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_effect_slots.erase(slot);
    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        _jeal_ctx.impl_alDeleteAuxiliaryEffectSlots(
            1, &slot->m_effect_slot_instance);
    }
    delete slot;
}

void jeal_effect_slot_set_effect(jeal_effect_slot* slot, void* effect_may_null)
{
    std::lock_guard g3(_jeal_ctx._jeal_all_effects_mx);
    std::lock_guard g4(_jeal_ctx._jeal_all_effect_slots_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_effect_slot_set_effect(slot, effect_may_null);
}

void jeal_effect_slot_gain(jeal_effect_slot* slot, float gain)
{
    std::lock_guard g4(_jeal_ctx._jeal_all_effect_slots_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    slot->m_gain = gain;
    if (_jeal_ctx._jeal_efx_enabled_sends_for_each_source != 0)
    {
        _jeal_ctx.impl_alAuxiliaryEffectSlotf(
            slot->m_effect_slot_instance, AL_EFFECTSLOT_GAIN, gain);
    }
}

void jeal_source_bind_effect_slot(
    jeal_source* source, jeal_effect_slot* slot_may_null, size_t pass)
{
    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::lock_guard g4(_jeal_ctx._jeal_all_effect_slots_mx);
    std::shared_lock g9(_jeal_ctx._jeal_context_mx);

    _jeal_source_bind_effect_slot(source, slot_may_null, pass);
}
