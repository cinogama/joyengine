#define JE_IMPL
#include "jeecs.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <unordered_set>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <chrono>

#define JE_AL_DEVICE(p) reinterpret_cast<ALCdevice *>(p)
#define AL_JE_DEVICE(p) reinterpret_cast<je_audio_native_play_device_instance_t *>(p)
#define JE_EFFECT_HEAD(v) (AudioEffectHead *)(reinterpret_cast<intptr_t>(v) - sizeof(AudioEffectHead))
#define JE_EFFECT_DATA(t, v) (t *)(reinterpret_cast<intptr_t>(v) + sizeof(AudioEffectHead))

using namespace std;
using namespace jeecs;

namespace jeecs
{
    namespace
    {
        struct AudioNativeEffectInstance
        {
            ALuint m_effect_id;

            struct dump_data_t
            {
                // Empty
            };
            optional<dump_data_t> m_dump;
        };
        struct alignas(8) AudioEffectHead
        {
            AudioNativeEffectInstance* m_effect_instance;
            ALenum m_effect_kind;

            size_t m_references;
        };
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_reverb_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_chorus_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_distortion_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_echo_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_flanger_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_frequency_shifter_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_vocal_morpher_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_pitch_shifter_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_ring_modulator_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_autowah_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_compressor_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_equalizer_t) == 0);
        static_assert(sizeof(AudioEffectHead) % alignof(je_audio_effect_eax_reverb_t) == 0);

        class AudioContextHelper
        {
        public:
            static void increase_ref(const je_audio_buffer_t* buffer);
            static void decrease_ref(const je_audio_buffer_t* buffer);
            static void increase_ref(AudioEffectHead* effect);
            static void decrease_ref(AudioEffectHead* effect);
            static void increase_ref(je_audio_effect_slot_t* effect_slot);
            static void decrease_ref(je_audio_effect_slot_t* effect_slot);
            static void increase_ref(je_audio_filter_t* filter);
            static void decrease_ref(je_audio_filter_t* filter);

            static void effect_init_default(AudioEffectHead* head, je_audio_effect_reverb_t* reverb);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_chorus_t* chorus);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_distortion_t* distortion);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_echo_t* echo);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_flanger_t* flanger);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_frequency_shifter_t* shifter);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_vocal_morpher_t* morpher);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_pitch_shifter_t* shifter);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_ring_modulator_t* modulator);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_autowah_t* wah);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_compressor_t* compressor);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_equalizer_t* equalizer);
            static void effect_init_default(AudioEffectHead* head, je_audio_effect_eax_reverb_t* eaxreverb);

            template <typename T>
            class Holder
            {
                JECS_DISABLE_MOVE_AND_COPY(Holder);

                optional<T*> m_res;

            public:
                Holder() = default;
                ~Holder()
                {
                    if (m_res.has_value())
                        decrease_ref(m_res.value());
                }

                bool has_value() const
                {
                    return m_res.has_value();
                }
                T* value() const
                {
                    return m_res.value();
                }

                void set_res(T* res)
                {
                    assert(res != nullptr);
                    increase_ref(res);

                    if (m_res.has_value())
                        decrease_ref(m_res.value());

                    m_res = res;
                }
                void set_res_may_null(T* res)
                {
                    if (res != nullptr)
                        set_res(res);
                    else
                        reset();
                }
                void reset()
                {
                    if (m_res.has_value())
                        decrease_ref(m_res.value());

                    m_res.reset();
                }
            };
        };
    }
}

struct je_audio_native_buffer_instance_t
{
    ALuint m_buffer_id;

    struct dump_data_t
    {
    };
    optional<dump_data_t> m_dump;
};
struct je_audio_native_source_instance_t
{
    ALuint m_source_id;
    AudioContextHelper::Holder<const je_audio_buffer_t> m_playing_buffer;
    AudioContextHelper::Holder<je_audio_filter_t> m_playing_filter;

    struct playing_effect_slot_and_filter
    {
        AudioContextHelper::Holder<je_audio_effect_slot_t>    m_slot;
        AudioContextHelper::Holder<je_audio_filter_t>         m_filter;
    };
    playing_effect_slot_and_filter m_playing_effect_slot_and_filters[audio::MAX_AUXILIARY_SENDS];

    // Dump data:
    struct dump_data_t
    {
        je_audio_state_t m_play_state;
        ALint m_play_process;
    };
    optional<dump_data_t> m_dump;
};
struct je_audio_native_effect_slot_instance_t
{
    ALuint m_effect_slot_id;
    AudioContextHelper::Holder<AudioEffectHead> m_binding_effect;

    struct dump_data_t
    {
    };
    optional<dump_data_t> m_dump;
};
struct je_audio_native_filter_instance_t
{
    ALuint m_filter_id;

    struct dump_data_t
    {
        // Empty
    };
    optional<dump_data_t> m_dump;
};

namespace jeecs
{
    namespace
    {
        class AudioContext
        {
            JECS_DISABLE_MOVE_AND_COPY(AudioContext);

            friend class AudioContextHelper;

            const bool m_openal_ALC_ENUMERATE_ALL_EXT_enabled;

            bool m_context_updating_thread_quit;
            optional<je_audio_play_device_t*> m_current_play_device;

            shared_mutex m_context_mx; // g0
            condition_variable_any m_context_cv_for_device_updating;
            thread m_context_updating_thread;
            struct device_updating_shared_unique_lock
            {
                JECS_DISABLE_MOVE_AND_COPY(device_updating_shared_unique_lock);

                enum class lock_state
                {
                    UNLOCKED,
                    SHARED_LOCKED,
                    LOCKED,
                };

                shared_mutex* m_sgmx;
                bool m_locked;

                device_updating_shared_unique_lock(shared_mutex& smx)
                    : m_sgmx(&smx)
                    , m_locked(true)
                {
                    m_sgmx->lock_shared();
                }
                ~device_updating_shared_unique_lock()
                {
                    if (m_locked)
                        m_sgmx->unlock_shared();
                }

                void lock()
                {
                    m_sgmx->lock_shared();

                    assert(m_locked == false);
                    m_locked = true;
                }
                void unlock()
                {
                    assert(m_locked == true);
                    m_locked = false;

                    m_sgmx->unlock_shared();
                }
            };

            vector<je_audio_play_device_t> m_enumed_play_devices;

            mutex m_sources_mx; // g1
            unordered_set<je_audio_source_t*> m_sources;
            mutex m_buffers_mx; // g2
            unordered_set<je_audio_buffer_t*> m_buffers;
            mutex m_effects_mx; // g3
            unordered_set<AudioEffectHead*> m_effects;
            mutex m_effect_slots_mx; // g4
            unordered_set<je_audio_effect_slot_t*> m_effect_slots;
            mutex m_filters_mx; // g5
            unordered_set<je_audio_filter_t*> m_filters;

            je_audio_listener_t m_listener;

            struct OpenALEfx
            {
                LPALGENEFFECTS alGenEffects = nullptr;
                LPALDELETEEFFECTS alDeleteEffects = nullptr;
                LPALEFFECTI alEffecti = nullptr;
                LPALEFFECTF alEffectf = nullptr;
                LPALEFFECTFV alEffectfv = nullptr;
                LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots = nullptr;
                LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots = nullptr;
                LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti = nullptr;
                LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf = nullptr;
                LPALGENFILTERS alGenFilters = nullptr;
                LPALDELETEFILTERS alDeleteFilters = nullptr;
                LPALFILTERI alFilteri = nullptr;
                LPALFILTERF alFilterf = nullptr;

                JECS_DISABLE_MOVE_AND_COPY(OpenALEfx);

                OpenALEfx(je_audio_native_play_device_instance_t* device)
                {
                    auto* aldevice = JE_AL_DEVICE(device);

#define JEAL_LOAD_FUNC(name)                                              \
    name = (decltype(name))alcGetProcAddress(aldevice, #name);            \
    if (name == nullptr)                                                  \
    {                                                                     \
        debug::logfatal("Failed to load al function: %s.", #name); \
        return;                                                           \
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

#undef JEAL_LOAD_FUNC
                }
            };
            optional<OpenALEfx> m_alext_efx = nullopt;

            struct jeal_play_device_states_dump
            {
                unordered_map<je_audio_buffer_t*, je_audio_native_buffer_instance_t*>
                    m_buffers_dump;
                unordered_map<je_audio_source_t*, je_audio_native_source_instance_t*>
                    m_sources_dump;
                unordered_map<AudioEffectHead*, AudioNativeEffectInstance*>
                    m_effects_dump;
                unordered_map<je_audio_effect_slot_t*, je_audio_native_effect_slot_instance_t*>
                    m_effect_slots_dump;
                unordered_map<je_audio_filter_t*, je_audio_native_filter_instance_t*>
                    m_filters_dump;
            };
            optional<jeal_play_device_states_dump> m_play_state_dump = nullopt;

            static bool check_device_instance_connected(je_audio_native_play_device_instance_t* device)
            {
                ALCint device_connected;
                alcGetIntegerv(reinterpret_cast<ALCdevice*>(device), ALC_CONNECTED, 1, &device_connected);

                if (device_connected == ALC_FALSE)
                    return false;
                return true;
            }

            void dump_and_close_playing_states()
            {
                assert(!m_play_state_dump.has_value());
                assert(m_current_play_device.has_value());

                auto& states = m_play_state_dump.emplace();

                for (auto* source : m_sources)
                {
                    assert(source->m_source_instance != nullptr);

                    auto& dump = source->m_source_instance->m_dump.emplace();

                    // NOTE: Source 需要转储播放状态和播放进度
                    ALenum play_state;
                    alGetSourcei(
                        source->m_source_instance->m_source_id,
                        AL_SOURCE_STATE,
                        &play_state);

                    alGetSourcei(
                        source->m_source_instance->m_source_id,
                        AL_BYTE_OFFSET,
                        &dump.m_play_process);

                    switch (play_state)
                    {
                    case AL_PLAYING:
                        dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_PLAYING;
                        break;
                    case AL_PAUSED:
                        dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_PAUSED;
                        break;
                    default:
                        assert(play_state == AL_STOPPED || play_state == AL_INITIAL);
                        dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_STOPPED;
                        break;
                    }

                    states.m_sources_dump[source] = shutdown_source_native_instance(source);
                }

                for (auto* buffer : m_buffers)
                {
                    // NOTE: 转储时，设备必然存在，所以instance也必然存在，下同
                    assert(buffer->m_buffer_instance != nullptr);

                    auto& dump = buffer->m_buffer_instance->m_dump.emplace();
                    (void)dump;

                    // NOTE: Buffer 没有什么特别需要转储的状态，此处只保留 native instance 的实例
                    states.m_buffers_dump[buffer] = shutdown_buffer_native_instance(buffer);
                }

                for (auto* effect_slot : m_effect_slots)
                {
                    assert(effect_slot->m_effect_slot_instance != nullptr);

                    auto& dump = effect_slot->m_effect_slot_instance->m_dump.emplace();
                    (void)dump;

                    states.m_effect_slots_dump[effect_slot] =
                        shutdown_effect_slot_native_instance(effect_slot);
                }

                for (auto* effect : m_effects)
                {
                    assert(effect->m_effect_instance != nullptr);

                    auto& dump = effect->m_effect_instance->m_dump.emplace();
                    (void)dump;

                    states.m_effects_dump[effect] = shutdown_effect_native_instance(effect);
                }

                for (auto* filter : m_filters)
                {
                    assert(filter->m_filter_instance != nullptr);
                    auto& dump = filter->m_filter_instance->m_dump.emplace();
                    (void)dump;
                    states.m_filters_dump[filter] = shutdown_filter_native_instance(filter);
                }
            }
            void restore_playing_states()
            {
                assert(m_play_state_dump.has_value());
                assert(m_current_play_device.has_value());

                auto& states = m_play_state_dump.value();

                for (auto* filter : m_filters)
                {
                    auto fnd = states.m_filters_dump.find(filter);
                    if (fnd != states.m_filters_dump.end())
                    {
                        // 有之前留下的转储信息，更新之
                        assert(filter->m_filter_instance == nullptr);
                        filter->m_filter_instance = fnd->second;
                        if (m_alext_efx.has_value())
                            m_alext_efx.value().alGenFilters(1, &filter->m_filter_instance->m_filter_id);
                        fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                    }
                    // else
                    // ; // 这是一个新的 filter 实例，在转储期间被创建出来的。

                    update_filter_lockfree(filter);
                }
                for (auto& [_, filter_instance] : states.m_filters_dump)
                {
                    // 释放无主的 filter 转储实例
                    if (filter_instance != nullptr)
                        delete filter_instance;
                }

                /////////

                for (auto* effect : m_effects)
                {
                    auto fnd = states.m_effects_dump.find(effect);
                    if (fnd != states.m_effects_dump.end())
                    {
                        // 有之前留下的转储信息，更新之
                        assert(effect->m_effect_instance == nullptr);

                        effect->m_effect_instance = fnd->second;
                        if (m_alext_efx.has_value())
                            m_alext_efx.value().alGenEffects(1, &effect->m_effect_instance->m_effect_id);

                        fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                    }
                    // else
                    // ; // 这是一个新的 effect 实例，在转储期间被创建出来的。

                    update_effect_lockfree(effect);
                }
                for (auto& [_, effect_instance] : states.m_effects_dump)
                {
                    // 释放无主的 effect 转储实例
                    if (effect_instance != nullptr)
                        delete effect_instance;
                }

                /////////

                for (auto* effect_slot : m_effect_slots)
                {
                    auto fnd = states.m_effect_slots_dump.find(effect_slot);
                    if (fnd != states.m_effect_slots_dump.end())
                    {
                        // 有之前留下的转储信息，更新之
                        assert(effect_slot->m_effect_slot_instance == nullptr);

                        effect_slot->m_effect_slot_instance = fnd->second;
                        if (m_alext_efx.has_value())
                            m_alext_efx.value().alGenAuxiliaryEffectSlots(1, &effect_slot->m_effect_slot_instance->m_effect_slot_id);

                        fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                    }
                    // else
                    // ; // 这是一个新的 effect slot 实例，在转储期间被创建出来的。

                    update_effect_slot_lockfree(effect_slot);
                }
                for (auto& [_, effect_slot_instance] : states.m_effect_slots_dump)
                {
                    // 释放无主的 effect slot 转储实例
                    if (effect_slot_instance != nullptr)
                        delete effect_slot_instance;
                }

                /////////

                for (auto* buffer : m_buffers)
                {
                    auto fnd = states.m_buffers_dump.find(buffer);
                    if (fnd != states.m_buffers_dump.end())
                    {
                        // 有之前留下的转储信息，更新之
                        assert(buffer->m_buffer_instance == nullptr);

                        buffer->m_buffer_instance = fnd->second;
                        alGenBuffers(1, &buffer->m_buffer_instance->m_buffer_id);

                        fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                    }
                    // else
                    // ; // 这是一个新的 buffer 实例，在转储期间被创建出来的。

                    update_buffer_lockfree(buffer);
                }
                for (auto& [_, buffer_instance] : states.m_buffers_dump)
                {
                    // 释放无主的 buffer 转储实例
                    if (buffer_instance != nullptr)
                        delete buffer_instance;
                }

                /////////

                for (auto* source : m_sources)
                {
                    auto fnd = states.m_sources_dump.find(source);
                    if (fnd != states.m_sources_dump.end())
                    {
                        // 有之前留下的转储信息，更新之
                        assert(source->m_source_instance == nullptr);

                        source->m_source_instance = fnd->second;
                        alGenSources(1, &source->m_source_instance->m_source_id);

                        fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                    }
                    // else
                    // ; // 这是一个新的 source 实例，在转储期间被创建出来的。

                    update_source_lockfree(source);
                }
                for (auto& [_, source_instance] : states.m_sources_dump)
                {
                    // 释放无主的 source 转储实例
                    if (source_instance != nullptr)
                        delete source_instance;
                }

                m_play_state_dump.reset();
            }
            void try_restore_playing_states()
            {
                if (m_play_state_dump.has_value())
                    restore_playing_states();

                assert(!m_play_state_dump.has_value());
            }

            void deactive_using_device(je_audio_play_device_t* device)
            {
                assert(device->m_active);
                assert(device->m_device_instance != nullptr);

                debug::loginfo("Deactive audio device: %s.", device->m_name);

                // 被关闭的设备此前正在使用中，需要保存当前的音频播放状态以供恢复
                dump_and_close_playing_states();
                m_current_play_device.reset();
                device->m_active = false;

                ALCcontext* current_context = alcGetCurrentContext();
                assert(current_context != nullptr);

                // 关闭当前设备上下文
                if (AL_FALSE == alcMakeContextCurrent(nullptr))
                    debug::logfatal("Failed to clear current audio context.");

                alcDestroyContext(current_context);
            }
            void active_using_device(je_audio_play_device_t* device)
            {
                assert(!device->m_active);
                assert(device->m_device_instance != nullptr);

                debug::loginfo("Active audio device: %s.", device->m_name);

                auto* current_context = alcCreateContext(JE_AL_DEVICE(device->m_device_instance), nullptr);
                if (current_context == nullptr || AL_FALSE == alcMakeContextCurrent(current_context))
                {
                    debug::logfatal("Failed to create context for device: %s.", device->m_name);
                    return;
                }

                if (device->m_max_auxiliary_sends <= 0)
                {
                    debug::logwarn("Audio device: %s, does not support EFX.", device->m_name);

                    // 关闭 EFX 拓展
                    m_alext_efx.reset();
                }
                else
                {
                    debug::loginfo("Audio device: %s, support EFX with max %d sends for each source.",
                        device->m_name, (int)device->m_max_auxiliary_sends);

                    // 开启 EFX 拓展
                    m_alext_efx.emplace(device->m_device_instance);
                }

                // 设备及上下文创建完成，恢复播放状态（如果有）
                m_current_play_device.emplace(device);
                try_restore_playing_states();
                device->m_active = true;
            }

            void update_enumed_play_devices(vector<je_audio_play_device_t>&& new_opened_devices)
            {
                // 关闭之前的设备实例
                for (auto& device : m_enumed_play_devices)
                {
                    if (device.m_device_instance != nullptr)
                    {
                        assert(device.m_name != nullptr);

                        if (device.m_active)
                        {
                            // 关闭当前设备上下文
                            deactive_using_device(&device);
                        }

                        alcCloseDevice(JE_AL_DEVICE(device.m_device_instance));
                        free((void*)device.m_name);
                    }
                }

                // 交换新旧设备列表
                m_enumed_play_devices.swap(new_opened_devices);

                // 检查或更新默认设备
                if (!m_enumed_play_devices.empty())
                {
                    auto fnd = find_if(
                        m_enumed_play_devices.begin(),
                        m_enumed_play_devices.end(),
                        [](je_audio_play_device_t& device)
                        {
                            return device.m_active;
                        });

                    // 记录第一个设备为默认设备
                    m_enumed_play_devices.front().m_is_default = true;

                    if (fnd == m_enumed_play_devices.end())
                    {
                        // 没有正在使用中的设备，说明之前使用的设备已经断开或不存在。
                        // 重新创建一个默认设备
                        active_using_device(&m_enumed_play_devices.front());
                    }
                    else
                        // m_current_play_device 储存的是交换前的 m_enumed_play_devices 地址
                        // 需要在此恢复回来。
                        m_current_play_device = &*fnd;
                }
            }

            const char* get_device_names()
            {
                if (m_openal_ALC_ENUMERATE_ALL_EXT_enabled)
                    return alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
                else
                    return alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
            }

            void refetch_and_update_enumed_play_devices()
            {
                const char* device_names = nullptr;

                if (m_openal_ALC_ENUMERATE_ALL_EXT_enabled)
                    device_names = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
                else
                    device_names = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

                vector<je_audio_play_device_t> new_opened_devices;

                const char* current_device_name = device_names;
                while (current_device_name && *current_device_name != 0)
                {
                    auto fnd = find_if(
                        m_enumed_play_devices.begin(),
                        m_enumed_play_devices.end(),
                        [current_device_name](je_audio_play_device_t& device)
                        {
                            if (device.m_name != nullptr)
                                return strcmp(device.m_name, current_device_name) == 0;

                            return false;
                        });

                    if (fnd != m_enumed_play_devices.end())
                    {
                        // 同名设备已经存在于之前枚举的列表中，说明之前这个设备已经连接
                        // 考虑以下情况：
                        //   1) 之前链接的设备实例已经断开，这次枚举到的是新的同名设备

                        // 检查旧设备是否仍然链接，如果是，则此次枚举到的就是旧设备
                        auto& old_connected_device = *fnd;
                        if (check_device_instance_connected(old_connected_device.m_device_instance))
                        {
                            // 这是旧设备，迁移到新设备列表中
                            new_opened_devices.emplace_back(old_connected_device);

                            old_connected_device.m_is_default = false;
                            old_connected_device.m_device_instance = nullptr;
                            old_connected_device.m_name = nullptr;
                        }
                        else
                            // 这是新设备，按照新设备流程处理
                            goto _label_new_device_instance_creating;
                    }
                    else
                    {
                    _label_new_device_instance_creating:
                        // 新设备，创建新的设备实例

                        auto* device_instance = alcOpenDevice(current_device_name);

                        debug::loginfo("Found audio device: %s.", current_device_name);

                        if (device_instance == nullptr)
                        {
                            debug::logerr("Failed to open device: '%s'.", current_device_name);
                        }
                        else
                        {
                            // 创建新的设备实例
                            auto& new_device = new_opened_devices.emplace_back();

                            // 检查当前设备是否支持 EFX 扩展
                            ALCint device_max_auxiliary_sends = 0;

                            if (alcIsExtensionPresent(device_instance, ALC_EXT_EFX_NAME))
                            {
                                alcGetIntegerv(
                                    device_instance,
                                    ALC_MAX_AUXILIARY_SENDS,
                                    1,
                                    &device_max_auxiliary_sends);

                                new_device.m_max_auxiliary_sends = min(
                                    device_max_auxiliary_sends,
                                    (ALCint)audio::MAX_AUXILIARY_SENDS);
                            }

                            new_device.m_device_instance = AL_JE_DEVICE(device_instance);
                            new_device.m_name = strdup(current_device_name);
                            new_device.m_active = false;
                            new_device.m_is_default = false;
                            new_device.m_max_auxiliary_sends = (int)device_max_auxiliary_sends;
                        }
                    }

                    // 继续枚举下一个设备
                    if (!m_openal_ALC_ENUMERATE_ALL_EXT_enabled)
                        break;

                    current_device_name += strlen(current_device_name) + 1;
                }

                update_enumed_play_devices(move(new_opened_devices));

                if (m_enumed_play_devices.empty())
                    debug::logwarn("No play devices found.");
            }

            static ALenum je_al_format(je_audio_format_t format, size_t* out_byte_per_sample)
            {
                switch (format)
                {
                case JE_AUDIO_FORMAT_MONO8:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 1;
                    return AL_FORMAT_MONO8;
                case JE_AUDIO_FORMAT_MONO16:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 2;
                    return AL_FORMAT_MONO16;
                case JE_AUDIO_FORMAT_MONO32F:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 4;
                    return AL_FORMAT_MONO_FLOAT32;
                case JE_AUDIO_FORMAT_STEREO8:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 2;
                    return AL_FORMAT_STEREO8;
                case JE_AUDIO_FORMAT_STEREO16:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 4;
                    return AL_FORMAT_STEREO16;
                case JE_AUDIO_FORMAT_STEREO32F:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 8;
                    return AL_FORMAT_STEREO_FLOAT32;
                default:
                    if (out_byte_per_sample != nullptr)
                        *out_byte_per_sample = 0;
                    return AL_NONE;
                }
            }

            // Buffer
            void update_buffer_lockfree(je_audio_buffer_t* buffer)
            {
                if (!m_current_play_device.has_value())
                {
                    // 没有设备，实例亦不应当存在（已被转储）
                    assert(buffer->m_buffer_instance == nullptr);
                    return;
                }

                if (buffer->m_buffer_instance == nullptr)
                {
                    buffer->m_buffer_instance = new je_audio_native_buffer_instance_t;
                    alGenBuffers(1, &buffer->m_buffer_instance->m_buffer_id);
                }

                alBufferData(
                    buffer->m_buffer_instance->m_buffer_id,
                    je_al_format(buffer->m_format, nullptr),
                    buffer->m_data,
                    buffer->m_size,
                    buffer->m_sample_rate);

                if (buffer->m_buffer_instance->m_dump.has_value())
                {
                    buffer->m_buffer_instance->m_dump.reset();
                }
            }
            const je_audio_buffer_t* instance_buffer(
                const void* new_allocated_buffer_data,
                size_t buffer_data_len,
                size_t sample_rate,
                je_audio_format_t format)
            {
                size_t byte_per_sample;
                auto al_format = je_al_format(format, &byte_per_sample);
                if (al_format == AL_NONE)
                {
                    debug::logerr("Bad audio buffer format: %d.", (int)format);
                    return nullptr;
                }

                if (buffer_data_len % byte_per_sample != 0)
                {
                    debug::logerr("Audio buffer size is not aligned.");
                    return nullptr;
                }

                je_audio_buffer_t* audio_buffer = new je_audio_buffer_t;

                audio_buffer->m_buffer_instance = nullptr;

                audio_buffer->m_references = 1;
                audio_buffer->m_data = new_allocated_buffer_data;
                audio_buffer->m_size = buffer_data_len;
                audio_buffer->m_sample_rate = sample_rate;
                audio_buffer->m_sample_size = byte_per_sample;
                audio_buffer->m_format = format;
                audio_buffer->m_byte_rate = byte_per_sample * audio_buffer->m_sample_rate;

                m_buffers.emplace(audio_buffer);
                update_buffer_lockfree(audio_buffer);

                return audio_buffer;
            }
            je_audio_native_buffer_instance_t* shutdown_buffer_native_instance(je_audio_buffer_t* buffer_instance)
            {
                if (buffer_instance->m_buffer_instance != nullptr)
                {
                    auto* instance = buffer_instance->m_buffer_instance;
                    alDeleteBuffers(1, &instance->m_buffer_id);

                    buffer_instance->m_buffer_instance = nullptr;
                    return instance;
                }
                return nullptr;
            }
            void shutdown_buffer(je_audio_buffer_t* buffer)
            {
                assert(buffer != nullptr);

                auto* native_instance = shutdown_buffer_native_instance(buffer);
                if (native_instance == nullptr && m_play_state_dump.has_value())
                {
                    // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                    //  需要从转储中删除此buffer

                    // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                    auto& dump = m_play_state_dump.value().m_buffers_dump;
                    auto fnd = dump.find(buffer);
                    if (fnd != dump.end())
                    {
                        delete fnd->second;
                        dump.erase(fnd);
                    }
                }
                else
                    delete native_instance;

                free(const_cast<void*>(buffer->m_data));

                delete buffer;
            }

            // Source
            void update_source_lockfree(je_audio_source_t* source)
            {
                if (!m_current_play_device.has_value())
                {
                    // 此时应该没有任何可用设备
                    assert(source->m_source_instance == nullptr);
                    return;
                }

                if (source->m_source_instance == nullptr)
                {
                    source->m_source_instance = new je_audio_native_source_instance_t;
                    alGenSources(1, &source->m_source_instance->m_source_id);

                    assert(!source->m_source_instance->m_playing_buffer.has_value());
                    assert(!source->m_source_instance->m_dump.has_value());
                }

                alSourcei(source->m_source_instance->m_source_id, AL_LOOPING, source->m_loop ? AL_TRUE : AL_FALSE);
                alSourcef(source->m_source_instance->m_source_id, AL_GAIN, source->m_gain);
                alSourcef(source->m_source_instance->m_source_id, AL_PITCH, source->m_pitch);
                alSource3f(source->m_source_instance->m_source_id, AL_POSITION,
                    source->m_location[0], source->m_location[1], -source->m_location[2]);
                alSource3f(source->m_source_instance->m_source_id, AL_VELOCITY,
                    source->m_velocity[0], source->m_velocity[1], -source->m_velocity[2]);

                if (source->m_source_instance->m_dump.has_value())
                {
                    auto& dump = source->m_source_instance->m_dump.value();

                    if (source->m_source_instance->m_playing_buffer.has_value())
                    {
                        // 设置播放的buffer实例
                        auto* buffer = source->m_source_instance->m_playing_buffer.value();

                        alSourcei(
                            source->m_source_instance->m_source_id,
                            AL_BUFFER,
                            buffer->m_buffer_instance->m_buffer_id);

                        if (dump.m_play_state != je_audio_state_t::JE_AUDIO_STATE_STOPPED)
                        {
                            // 恢复播放状态
                            alSourcei(
                                source->m_source_instance->m_source_id,
                                AL_BYTE_OFFSET,
                                dump.m_play_process);

                            if (dump.m_play_state == je_audio_state_t::JE_AUDIO_STATE_PAUSED)
                                alSourcePause(source->m_source_instance->m_source_id);
                            else
                            {
                                assert(dump.m_play_state == je_audio_state_t::JE_AUDIO_STATE_PLAYING);
                                alSourcePlay(source->m_source_instance->m_source_id);
                            }
                        }
                    }

                    if (m_alext_efx.has_value())
                    {
                        ALint filter_id = AL_FILTER_NULL;
                        if (source->m_source_instance->m_playing_filter.has_value())
                            filter_id = source->m_source_instance->m_playing_filter.value()->m_filter_instance->m_filter_id;

                        alSourcei(source->m_source_instance->m_source_id, AL_DIRECT_FILTER, filter_id);

                        const size_t max_auxiliary_sends = m_current_play_device.value()->m_max_auxiliary_sends;
                        for (size_t i = 0; i < max_auxiliary_sends; i += 1)
                        {
                            auto& binded_effect_slot_and_filter =
                                source->m_source_instance->m_playing_effect_slot_and_filters[i];

                            ALint slot_id = AL_EFFECTSLOT_NULL;
                            ALint filter_id = AL_FILTER_NULL;

                            if (binded_effect_slot_and_filter.m_slot.has_value())
                                slot_id = binded_effect_slot_and_filter.m_slot.value()->m_effect_slot_instance->m_effect_slot_id;
                            if (binded_effect_slot_and_filter.m_filter.has_value())
                                filter_id = binded_effect_slot_and_filter.m_filter.value()->m_filter_instance->m_filter_id;

                            alSource3i(source->m_source_instance->m_source_id,
                                AL_AUXILIARY_SEND_FILTER,
                                slot_id,
                                (ALint)i,
                                filter_id);
                        }
                    }
                    source->m_source_instance->m_dump.reset();
                }
            }
            je_audio_native_source_instance_t* shutdown_source_native_instance(je_audio_source_t* source_instance)
            {
                if (source_instance->m_source_instance != nullptr)
                {
                    auto* instance = source_instance->m_source_instance;
                    alDeleteSources(1, &instance->m_source_id);

                    source_instance->m_source_instance = nullptr;
                    return instance;
                }
                return nullptr;
            }
            void shutdown_source(je_audio_source_t* source)
            {
                assert(source != nullptr);

                auto* native_instance = shutdown_source_native_instance(source);
                if (native_instance == nullptr && m_play_state_dump.has_value())
                {
                    // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                    //  需要从转储中删除此source

                    // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                    auto& dump = m_play_state_dump.value().m_sources_dump;
                    auto fnd = dump.find(source);
                    if (fnd != dump.end())
                    {
                        delete fnd->second;
                        dump.erase(fnd);
                    }
                }
                else
                    delete native_instance;

                delete source;
            }

            je_audio_native_source_instance_t* require_dump_for_no_device_context_source(
                je_audio_source_t* source)
            {
                assert(source != nullptr);

                if (!m_play_state_dump.has_value())
                    m_play_state_dump.emplace();

                auto& dump = m_play_state_dump.value();

                auto fnd = dump.m_sources_dump.find(source);
                if (fnd != dump.m_sources_dump.end())
                {
                    // 此 source 已经存在于转储中
                    return fnd->second;
                }

                je_audio_native_source_instance_t* new_dump_for_no_device_context_source =
                    new je_audio_native_source_instance_t;

                auto& source_dump = new_dump_for_no_device_context_source->m_dump.emplace();
                assert(!new_dump_for_no_device_context_source->m_playing_buffer.has_value());
                source_dump.m_play_process = 0;
                source_dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_STOPPED;

                // NOTE: 只有完全没有设备的情况下才需要 require_dump_for_no_device_context_source，此时
                //      源 ID 已经可有可无，此处不需要设置
                new_dump_for_no_device_context_source->m_source_id = 0;

                dump.m_sources_dump.emplace(
                    source, new_dump_for_no_device_context_source);

                return new_dump_for_no_device_context_source;
            }

            je_audio_native_effect_slot_instance_t* require_dump_for_no_device_context_effect_slot(
                je_audio_effect_slot_t* slot)
            {
                assert(slot != nullptr);

                if (!m_play_state_dump.has_value())
                    m_play_state_dump.emplace();

                auto& dump = m_play_state_dump.value();

                auto fnd = dump.m_effect_slots_dump.find(slot);
                if (fnd != dump.m_effect_slots_dump.end())
                {
                    // 此 source 已经存在于转储中
                    return fnd->second;
                }

                je_audio_native_effect_slot_instance_t* new_dump_for_no_device_context_effect_slot =
                    new je_audio_native_effect_slot_instance_t;

                auto& effect_slot_dump = new_dump_for_no_device_context_effect_slot->m_dump.emplace();
                (void)effect_slot_dump;

                // NOTE: 只有完全没有设备的情况下才需要 require_dump_for_no_device_context_effect_slot，此时
                //      源 ID 已经可有可无，此处不需要设置
                new_dump_for_no_device_context_effect_slot->m_effect_slot_id = 0;

                dump.m_effect_slots_dump.emplace(
                    slot, new_dump_for_no_device_context_effect_slot);

                return new_dump_for_no_device_context_effect_slot;
            }

            void close_buffer_lockfree(const je_audio_buffer_t* buffer)
            {
                auto* rw_buffer = const_cast<je_audio_buffer_t*>(buffer);

                m_buffers.erase(rw_buffer);
                shutdown_buffer(rw_buffer);
            }

            void update_effect_lockfree(AudioEffectHead* effect)
            {
                if (!m_current_play_device.has_value())
                {
                    // 此时应该没有任何可用设备
                    assert(effect->m_effect_instance == nullptr);
                    return;
                }

                auto* efx = m_alext_efx.has_value() ? &m_alext_efx.value() : nullptr;

                if (effect->m_effect_instance == nullptr)
                {
                    effect->m_effect_instance = new AudioNativeEffectInstance;
                    if (efx != nullptr)
                        efx->alGenEffects(1, &effect->m_effect_instance->m_effect_id);
                }

                if (efx != nullptr)
                {
                    ALuint effect_id = effect->m_effect_instance->m_effect_id;

                    efx->alEffecti(effect_id, AL_EFFECT_TYPE, effect->m_effect_kind);

                    switch (effect->m_effect_kind)
                    {
                    case AL_EFFECT_REVERB:
                    {
                        const je_audio_effect_reverb_t* instance_data = JE_EFFECT_DATA(je_audio_effect_reverb_t, effect);

                        efx->alEffectf(effect_id, AL_REVERB_DENSITY, instance_data->m_density);
                        efx->alEffectf(effect_id, AL_REVERB_DIFFUSION, instance_data->m_diffusion);
                        efx->alEffectf(effect_id, AL_REVERB_GAIN, instance_data->m_gain);
                        efx->alEffectf(effect_id, AL_REVERB_GAINHF, instance_data->m_gain_hf);
                        efx->alEffectf(effect_id, AL_REVERB_DECAY_TIME, instance_data->m_decay_time);
                        efx->alEffectf(effect_id, AL_REVERB_DECAY_HFRATIO, instance_data->m_decay_hf_ratio);
                        efx->alEffectf(effect_id, AL_REVERB_REFLECTIONS_GAIN, instance_data->m_reflections_gain);
                        efx->alEffectf(effect_id, AL_REVERB_REFLECTIONS_DELAY, instance_data->m_reflections_delay);
                        efx->alEffectf(effect_id, AL_REVERB_LATE_REVERB_GAIN, instance_data->m_late_reverb_gain);
                        efx->alEffectf(effect_id, AL_REVERB_LATE_REVERB_DELAY, instance_data->m_late_reverb_delay);
                        efx->alEffectf(effect_id, AL_REVERB_AIR_ABSORPTION_GAINHF, instance_data->m_air_absorption_gain_hf);
                        efx->alEffectf(effect_id, AL_REVERB_ROOM_ROLLOFF_FACTOR, instance_data->m_room_rolloff_factor);
                        efx->alEffecti(effect_id, AL_REVERB_DECAY_HFLIMIT, instance_data->m_decay_hf_limit ? AL_TRUE : AL_FALSE);

                        break;
                    }
                    case AL_EFFECT_CHORUS:
                    {
                        const je_audio_effect_chorus_t* instance_data = JE_EFFECT_DATA(je_audio_effect_chorus_t, effect);

                        efx->alEffecti(effect_id, AL_CHORUS_WAVEFORM, (ALint)instance_data->m_waveform);
                        efx->alEffecti(effect_id, AL_CHORUS_PHASE, instance_data->m_phase);
                        efx->alEffectf(effect_id, AL_CHORUS_RATE, instance_data->m_rate);
                        efx->alEffectf(effect_id, AL_CHORUS_DEPTH, instance_data->m_depth);
                        efx->alEffectf(effect_id, AL_CHORUS_FEEDBACK, instance_data->m_feedback);
                        efx->alEffectf(effect_id, AL_CHORUS_DELAY, instance_data->m_delay);

                        break;
                    }
                    case AL_EFFECT_DISTORTION:
                    {
                        const je_audio_effect_distortion_t* instance_data = JE_EFFECT_DATA(je_audio_effect_distortion_t, effect);

                        efx->alEffectf(effect_id, AL_DISTORTION_EDGE, instance_data->m_edge);
                        efx->alEffectf(effect_id, AL_DISTORTION_GAIN, instance_data->m_gain);
                        efx->alEffectf(effect_id, AL_DISTORTION_LOWPASS_CUTOFF, instance_data->m_lowpass_cutoff);
                        efx->alEffectf(effect_id, AL_DISTORTION_EQCENTER, instance_data->m_equalizer_center_freq);
                        efx->alEffectf(effect_id, AL_DISTORTION_EQBANDWIDTH, instance_data->m_equalizer_bandwidth);

                        break;
                    }
                    case AL_EFFECT_ECHO:
                    {
                        const je_audio_effect_echo_t* instance_data = JE_EFFECT_DATA(je_audio_effect_echo_t, effect);

                        efx->alEffectf(effect_id, AL_ECHO_DELAY, instance_data->m_delay);
                        efx->alEffectf(effect_id, AL_ECHO_LRDELAY, instance_data->m_lr_delay);
                        efx->alEffectf(effect_id, AL_ECHO_DAMPING, instance_data->m_damping);
                        efx->alEffectf(effect_id, AL_ECHO_FEEDBACK, instance_data->m_feedback);
                        efx->alEffectf(effect_id, AL_ECHO_SPREAD, instance_data->m_spread);

                        break;
                    }
                    case AL_EFFECT_FLANGER:
                    {
                        const je_audio_effect_flanger_t* instance_data = JE_EFFECT_DATA(je_audio_effect_flanger_t, effect);

                        efx->alEffecti(effect_id, AL_FLANGER_WAVEFORM, (ALint)instance_data->m_waveform);
                        efx->alEffecti(effect_id, AL_FLANGER_PHASE, instance_data->m_phase);
                        efx->alEffectf(effect_id, AL_FLANGER_RATE, instance_data->m_rate);
                        efx->alEffectf(effect_id, AL_FLANGER_DEPTH, instance_data->m_depth);
                        efx->alEffectf(effect_id, AL_FLANGER_FEEDBACK, instance_data->m_feedback);
                        efx->alEffectf(effect_id, AL_FLANGER_DELAY, instance_data->m_delay);

                        break;
                    }
                    case AL_EFFECT_FREQUENCY_SHIFTER:
                    {
                        const je_audio_effect_frequency_shifter_t* instance_data = JE_EFFECT_DATA(je_audio_effect_frequency_shifter_t, effect);

                        efx->alEffectf(effect_id, AL_FREQUENCY_SHIFTER_FREQUENCY, instance_data->m_frequency);
                        efx->alEffecti(effect_id, AL_FREQUENCY_SHIFTER_LEFT_DIRECTION, (ALint)instance_data->m_left_direction);
                        efx->alEffecti(effect_id, AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION, (ALint)instance_data->m_right_direction);

                        break;
                    }
                    case AL_EFFECT_VOCAL_MORPHER:
                    {
                        const je_audio_effect_vocal_morpher_t* instance_data = JE_EFFECT_DATA(je_audio_effect_vocal_morpher_t, effect);

                        efx->alEffecti(effect_id, AL_VOCAL_MORPHER_PHONEMEA, (ALint)instance_data->m_phoneme_a);
                        efx->alEffecti(effect_id, AL_VOCAL_MORPHER_PHONEMEA_COARSE_TUNING, instance_data->m_phoneme_a_coarse_tuning);
                        efx->alEffecti(effect_id, AL_VOCAL_MORPHER_PHONEMEB, (ALint)instance_data->m_phoneme_b);
                        efx->alEffecti(effect_id, AL_VOCAL_MORPHER_PHONEMEB_COARSE_TUNING, instance_data->m_phoneme_b_coarse_tuning);
                        efx->alEffecti(effect_id, AL_VOCAL_MORPHER_WAVEFORM, (ALint)instance_data->m_waveform);
                        efx->alEffectf(effect_id, AL_VOCAL_MORPHER_RATE, instance_data->m_rate);

                        break;
                    }
                    case AL_EFFECT_PITCH_SHIFTER:
                    {
                        const je_audio_effect_pitch_shifter_t* instance_data = JE_EFFECT_DATA(je_audio_effect_pitch_shifter_t, effect);

                        efx->alEffecti(effect_id, AL_PITCH_SHIFTER_COARSE_TUNE, instance_data->m_coarse_tune);
                        efx->alEffecti(effect_id, AL_PITCH_SHIFTER_FINE_TUNE, instance_data->m_fine_tune);

                        break;
                    }
                    case AL_EFFECT_RING_MODULATOR:
                    {
                        const je_audio_effect_ring_modulator_t* instance_data = JE_EFFECT_DATA(je_audio_effect_ring_modulator_t, effect);

                        efx->alEffectf(effect_id, AL_RING_MODULATOR_FREQUENCY, instance_data->m_frequency);
                        efx->alEffectf(effect_id, AL_RING_MODULATOR_HIGHPASS_CUTOFF, instance_data->m_highpass_cutoff);
                        efx->alEffecti(effect_id, AL_RING_MODULATOR_WAVEFORM, (ALint)instance_data->m_waveform);

                        break;
                    }
                    case AL_EFFECT_AUTOWAH:
                    {
                        const je_audio_effect_autowah_t* instance_data = JE_EFFECT_DATA(je_audio_effect_autowah_t, effect);

                        efx->alEffectf(effect_id, AL_AUTOWAH_ATTACK_TIME, instance_data->m_attack_time);
                        efx->alEffectf(effect_id, AL_AUTOWAH_RELEASE_TIME, instance_data->m_release_time);
                        efx->alEffectf(effect_id, AL_AUTOWAH_RESONANCE, instance_data->m_resonance);
                        efx->alEffectf(effect_id, AL_AUTOWAH_PEAK_GAIN, instance_data->m_peak_gain);

                        break;
                    }
                    case AL_EFFECT_COMPRESSOR:
                    {
                        const je_audio_effect_compressor_t* instance_data = JE_EFFECT_DATA(je_audio_effect_compressor_t, effect);

                        efx->alEffecti(effect_id, AL_COMPRESSOR_ONOFF, instance_data->m_enabled ? AL_TRUE : AL_FALSE);

                        break;
                    }
                    case AL_EFFECT_EQUALIZER:
                    {
                        const je_audio_effect_equalizer_t* instance_data = JE_EFFECT_DATA(je_audio_effect_equalizer_t, effect);

                        efx->alEffectf(effect_id, AL_EQUALIZER_LOW_GAIN, instance_data->m_low_gain);
                        efx->alEffectf(effect_id, AL_EQUALIZER_LOW_CUTOFF, instance_data->m_low_cutoff);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID1_GAIN, instance_data->m_mid1_gain);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID1_CENTER, instance_data->m_mid1_center);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID1_WIDTH, instance_data->m_mid1_width);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID2_GAIN, instance_data->m_mid2_gain);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID2_CENTER, instance_data->m_mid2_center);
                        efx->alEffectf(effect_id, AL_EQUALIZER_MID2_WIDTH, instance_data->m_mid2_width);
                        efx->alEffectf(effect_id, AL_EQUALIZER_HIGH_GAIN, instance_data->m_high_gain);
                        efx->alEffectf(effect_id, AL_EQUALIZER_HIGH_CUTOFF, instance_data->m_high_cutoff);

                        break;
                    }
                    case AL_EFFECT_EAXREVERB:
                    {
                        const je_audio_effect_eax_reverb_t* instance_data = JE_EFFECT_DATA(je_audio_effect_eax_reverb_t, effect);

                        efx->alEffectf(effect_id, AL_EAXREVERB_DENSITY, instance_data->m_density);
                        efx->alEffectf(effect_id, AL_EAXREVERB_DIFFUSION, instance_data->m_diffusion);
                        efx->alEffectf(effect_id, AL_EAXREVERB_GAIN, instance_data->m_gain);
                        efx->alEffectf(effect_id, AL_EAXREVERB_GAINHF, instance_data->m_gain_hf);
                        efx->alEffectf(effect_id, AL_EAXREVERB_GAINLF, instance_data->m_gain_lf);
                        efx->alEffectf(effect_id, AL_EAXREVERB_DECAY_TIME, instance_data->m_decay_time);
                        efx->alEffectf(effect_id, AL_EAXREVERB_DECAY_HFRATIO, instance_data->m_decay_hf_ratio);
                        efx->alEffectf(effect_id, AL_EAXREVERB_DECAY_LFRATIO, instance_data->m_decay_lf_ratio);
                        efx->alEffectf(effect_id, AL_EAXREVERB_REFLECTIONS_GAIN, instance_data->m_reflections_gain);
                        efx->alEffectf(effect_id, AL_EAXREVERB_REFLECTIONS_DELAY, instance_data->m_reflections_delay);

                        const float reflection_pan[3] = {
                            instance_data->m_reflections_pan_xyz[0],
                            instance_data->m_reflections_pan_xyz[1],
                            -instance_data->m_reflections_pan_xyz[2],
                        };
                        efx->alEffectfv(effect_id, AL_EAXREVERB_REFLECTIONS_PAN, reflection_pan);
                        efx->alEffectf(effect_id, AL_EAXREVERB_LATE_REVERB_GAIN, instance_data->m_late_reverb_gain);
                        efx->alEffectf(effect_id, AL_EAXREVERB_LATE_REVERB_DELAY, instance_data->m_late_reverb_delay);

                        const float reverb_pan[3] = {
                            instance_data->m_late_reverb_pan_xyz[0],
                            instance_data->m_late_reverb_pan_xyz[1],
                            -instance_data->m_late_reverb_pan_xyz[2],
                        };
                        efx->alEffectfv(effect_id, AL_EAXREVERB_LATE_REVERB_PAN, reverb_pan);
                        efx->alEffectf(effect_id, AL_EAXREVERB_ECHO_TIME, instance_data->m_echo_time);
                        efx->alEffectf(effect_id, AL_EAXREVERB_ECHO_DEPTH, instance_data->m_echo_depth);
                        efx->alEffectf(effect_id, AL_EAXREVERB_MODULATION_TIME, instance_data->m_modulation_time);
                        efx->alEffectf(effect_id, AL_EAXREVERB_MODULATION_DEPTH, instance_data->m_modulation_depth);
                        efx->alEffectf(effect_id, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, instance_data->m_air_absorption_gain_hf);
                        efx->alEffectf(effect_id, AL_EAXREVERB_HFREFERENCE, instance_data->m_hf_reference);
                        efx->alEffectf(effect_id, AL_EAXREVERB_LFREFERENCE, instance_data->m_lf_reference);
                        efx->alEffectf(effect_id, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, instance_data->m_room_rolloff_factor);
                        efx->alEffecti(effect_id, AL_EAXREVERB_DECAY_HFLIMIT, instance_data->m_decay_hf_limit ? AL_TRUE : AL_FALSE);

                        break;
                    }
                    default:
                        debug::logfatal("Unknown audio effect: %d when trying to update.",
                            (int)effect->m_effect_kind);
                    }
                }

                if (effect->m_effect_instance->m_dump.has_value())
                {
                    // Nothing todo.

                    effect->m_effect_instance->m_dump.reset();
                }
            }
            AudioNativeEffectInstance* shutdown_effect_native_instance(AudioEffectHead* effect)
            {
                if (effect->m_effect_instance != nullptr)
                {
                    auto* instance = effect->m_effect_instance;

                    if (m_alext_efx.has_value())
                        m_alext_efx.value().alDeleteEffects(1, &instance->m_effect_id);

                    effect->m_effect_instance = nullptr;
                    return instance;
                }
                return nullptr;
            }
            void shutdown_effect(AudioEffectHead* effect)
            {
                assert(effect != nullptr);

                auto* native_instance = shutdown_effect_native_instance(effect);
                if (native_instance == nullptr && m_play_state_dump.has_value())
                {
                    // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                    //  需要从转储中删除此 effect

                    // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                    auto& dump = m_play_state_dump.value().m_effects_dump;
                    auto fnd = dump.find(effect);
                    if (fnd != dump.end())
                    {
                        delete fnd->second;
                        dump.erase(fnd);
                    }
                }
                else
                    delete native_instance;

                free(effect);
            }
            void close_effect_lockfree(AudioEffectHead* effect)
            {
                m_effects.erase(effect);
                shutdown_effect(effect);
            }

            void update_effect_slot_lockfree(je_audio_effect_slot_t* slot)
            {
                if (!m_current_play_device.has_value())
                {
                    // 此时应该没有任何可用设备
                    assert(slot->m_effect_slot_instance == nullptr);
                    return;
                }

                auto* efx = m_alext_efx.has_value() ? &m_alext_efx.value() : nullptr;
                if (slot->m_effect_slot_instance == nullptr)
                {
                    slot->m_effect_slot_instance = new je_audio_native_effect_slot_instance_t;
                    if (efx != nullptr)
                        efx->alGenAuxiliaryEffectSlots(1, &slot->m_effect_slot_instance->m_effect_slot_id);
                }

                if (efx != nullptr)
                {
                    ALuint slot_id = slot->m_effect_slot_instance->m_effect_slot_id;

                    if (slot->m_effect_slot_instance->m_binding_effect.has_value())
                    {
                        AudioEffectHead* effect = slot->m_effect_slot_instance->m_binding_effect.value();
                        efx->alAuxiliaryEffectSloti(
                            slot_id, AL_EFFECTSLOT_EFFECT, effect->m_effect_instance->m_effect_id);
                    }
                    else
                        efx->alAuxiliaryEffectSloti(
                            slot_id, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);

                    efx->alAuxiliaryEffectSlotf(
                        slot_id,
                        AL_EFFECTSLOT_GAIN,
                        slot->m_gain);
                }

                if (slot->m_effect_slot_instance->m_dump.has_value())
                {
                    // Do nothing.
                    slot->m_effect_slot_instance->m_dump.reset();
                }
            }
            je_audio_native_effect_slot_instance_t* shutdown_effect_slot_native_instance(je_audio_effect_slot_t* slot)
            {
                if (slot->m_effect_slot_instance != nullptr)
                {
                    auto* instance = slot->m_effect_slot_instance;

                    if (m_alext_efx.has_value())
                        m_alext_efx.value().alDeleteAuxiliaryEffectSlots(
                            1, &instance->m_effect_slot_id);

                    slot->m_effect_slot_instance = nullptr;
                    return instance;
                }
                return nullptr;
            }
            void shutdown_effect_slot(je_audio_effect_slot_t* slot)
            {
                assert(slot != nullptr);

                auto* native_instance = shutdown_effect_slot_native_instance(slot);
                if (native_instance == nullptr && m_play_state_dump.has_value())
                {
                    // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                    //  需要从转储中删除此 effect slot

                    // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                    auto& dump = m_play_state_dump.value().m_effect_slots_dump;
                    auto fnd = dump.find(slot);
                    if (fnd != dump.end())
                    {
                        delete fnd->second;
                        dump.erase(fnd);
                    }
                }
                else
                    delete native_instance;

                delete slot;
            }
            void close_effect_slot_lockfree(je_audio_effect_slot_t* slot)
            {
                m_effect_slots.erase(slot);
                shutdown_effect_slot(slot);
            }

            void update_filter_lockfree(je_audio_filter_t* filter)
            {
                if (!m_current_play_device.has_value())
                {
                    // 此时应该没有任何可用设备
                    assert(filter->m_filter_instance == nullptr);
                    return;
                }

                auto* efx = m_alext_efx.has_value() ? &m_alext_efx.value() : nullptr;
                if (filter->m_filter_instance == nullptr)
                {
                    filter->m_filter_instance = new je_audio_native_filter_instance_t;
                    if (efx != nullptr)
                        efx->alGenFilters(1, &filter->m_filter_instance->m_filter_id);

                    assert(!filter->m_filter_instance->m_dump.has_value());
                }

                if (efx != nullptr)
                {
                    ALuint filter_id = filter->m_filter_instance->m_filter_id;
                    switch (filter->m_type)
                    {
                    case je_audio_filter_type_t::JE_AUDIO_FILTER_LOWPASS:
                        efx->alFilteri(filter->m_filter_instance->m_filter_id, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
                        efx->alFilterf(filter_id, AL_LOWPASS_GAIN, filter->m_gain);
                        efx->alFilterf(filter_id, AL_LOWPASS_GAINHF, filter->m_gain_hf);
                        break;
                    case je_audio_filter_type_t::JE_AUDIO_FILTER_HIGHPASS:
                        efx->alFilteri(filter->m_filter_instance->m_filter_id, AL_FILTER_TYPE, AL_FILTER_HIGHPASS);
                        efx->alFilterf(filter_id, AL_HIGHPASS_GAIN, filter->m_gain);
                        efx->alFilterf(filter_id, AL_HIGHPASS_GAINLF, filter->m_gain_lf);
                        break;
                    case je_audio_filter_type_t::JE_AUDIO_FILTER_BANDPASS:
                        efx->alFilteri(filter->m_filter_instance->m_filter_id, AL_FILTER_TYPE, AL_FILTER_BANDPASS);
                        efx->alFilterf(filter_id, AL_BANDPASS_GAIN, filter->m_gain);
                        efx->alFilterf(filter_id, AL_BANDPASS_GAINLF, filter->m_gain_lf);
                        efx->alFilterf(filter_id, AL_BANDPASS_GAINHF, filter->m_gain_hf);
                        break;
                    default:
                        debug::logfatal("Unknown audio filter: %d when trying to update.",
                            (int)filter->m_type);
                    }
                }

                if (filter->m_filter_instance->m_dump.has_value())
                {
                    // Do nothing.
                    filter->m_filter_instance->m_dump.reset();
                }
            }
            je_audio_native_filter_instance_t* shutdown_filter_native_instance(je_audio_filter_t* filter)
            {
                if (filter->m_filter_instance != nullptr)
                {
                    auto* instance = filter->m_filter_instance;
                    if (m_alext_efx.has_value())
                        m_alext_efx.value().alDeleteFilters(1, &instance->m_filter_id);
                    filter->m_filter_instance = nullptr;
                    return instance;
                }
                return nullptr;
            }
            void shutdown_filter(je_audio_filter_t* filter)
            {
                assert(filter != nullptr);
                auto* native_instance = shutdown_filter_native_instance(filter);
                if (native_instance == nullptr && m_play_state_dump.has_value())
                {
                    // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                    //  需要从转储中删除此 filter
                    // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                    auto& dump = m_play_state_dump.value().m_filters_dump;
                    auto fnd = dump.find(filter);
                    if (fnd != dump.end())
                    {
                        delete fnd->second;
                        dump.erase(fnd);
                    }
                }
                else
                    delete native_instance;
                delete filter;
            }
            void close_filter_lockfree(je_audio_filter_t* filter)
            {
                m_filters.erase(filter);
                shutdown_filter(filter);
            }

        public:
            je_audio_filter_t* create_filter()
            {
                je_audio_filter_t* filter = new je_audio_filter_t;

                filter->m_filter_instance = nullptr;

                filter->m_references = 1;

                filter->m_type = je_audio_filter_type_t::JE_AUDIO_FILTER_LOWPASS;
                filter->m_gain = 1.0f;
                filter->m_gain_hf = 0.5f;
                filter->m_gain_lf = 0.5f;

                lock_guard g5(m_filters_mx);       // g5
                shared_lock g0(m_context_mx);      // g0

                m_filters.emplace(filter);
                update_filter_lockfree(filter);

                return filter;
            }
            void update_filter(je_audio_filter_t* filter)
            {
                lock_guard g5(m_filters_mx);       // g5
                shared_lock g0(m_context_mx);      // g0

                update_filter_lockfree(filter);
            }
            void close_filter(je_audio_filter_t* filter)
            {
                lock_guard g5(m_filters_mx);       // g5
                shared_lock g0(m_context_mx);      // g0

                AudioContextHelper::decrease_ref(filter);
            }

            je_audio_effect_slot_t* create_effect_slot()
            {
                je_audio_effect_slot_t* slot = new je_audio_effect_slot_t;

                slot->m_effect_slot_instance = nullptr;
                slot->m_references = 1;
                slot->m_gain = 1.0f;

                lock_guard g4(m_effect_slots_mx); // g4
                shared_lock g0(m_context_mx);     // g0

                m_effect_slots.emplace(slot);
                update_effect_slot_lockfree(slot);

                return slot;
            }
            void update_effect_slot(je_audio_effect_slot_t* slot)
            {
                lock_guard g4(m_effect_slots_mx); // g4
                shared_lock g0(m_context_mx);     // g0

                update_effect_slot_lockfree(slot);
            }
            void close_effect_slot(je_audio_effect_slot_t* slot)
            {
                lock_guard g4(m_effect_slots_mx); // g4
                shared_lock g0(m_context_mx);     // g0

                AudioContextHelper::decrease_ref(slot);
            }
            void set_slot_effect(je_audio_effect_slot_t* slot, AudioEffectHead* effect_may_null)
            {
                lock_guard g4(m_effect_slots_mx); // g4
                lock_guard g3(m_effects_mx);      // g3

                for (;;)
                {
                    if (slot->m_effect_slot_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (slot->m_effect_slot_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        assert(effect_may_null == nullptr || effect_may_null->m_effect_instance != nullptr);

                        slot->m_effect_slot_instance->m_binding_effect.set_res_may_null(effect_may_null);

                        if (m_alext_efx.has_value())
                        {
                            ALuint slot_id = slot->m_effect_slot_instance->m_effect_slot_id;

                            if (effect_may_null != nullptr)
                                m_alext_efx.value().alAuxiliaryEffectSloti(
                                    slot_id, AL_EFFECTSLOT_EFFECT, effect_may_null->m_effect_instance->m_effect_id);
                            else
                                m_alext_efx.value().alAuxiliaryEffectSloti(
                                    slot_id, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
                        }
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (slot->m_effect_slot_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_effect_slot(slot);

                        dump_instance->m_binding_effect.set_res_may_null(effect_may_null);
                    }
                    break;
                }
            }

            template <typename T>
            T* create_effect()
            {
                void* buf = malloc(sizeof(AudioEffectHead) + sizeof(T));

                AudioEffectHead* head = static_cast<AudioEffectHead*>(buf);
                T* effect = JE_EFFECT_DATA(T, buf);

                head->m_effect_instance = nullptr;
                head->m_references = 1;

                AudioContextHelper::effect_init_default(head, effect);

                lock_guard g3(m_effects_mx);  // g3
                shared_lock g0(m_context_mx); // g0

                m_effects.emplace(head);
                update_effect_lockfree(head);

                return effect;
            }
            void update_effect(void* effect_data)
            {
                lock_guard g3(m_effects_mx);  // g3
                shared_lock g0(m_context_mx); // g0

                update_effect_lockfree(JE_EFFECT_HEAD(effect_data));
            }

            void close_effect(void* effect_data)
            {
                lock_guard g3(m_effects_mx);  // g3
                shared_lock g0(m_context_mx); // g0

                AudioContextHelper::decrease_ref(JE_EFFECT_HEAD(effect_data));
            }

            const je_audio_buffer_t* create_buffer(
                const void* data,
                size_t buffer_data_len,
                size_t sample_rate,
                je_audio_format_t format)
            {
                void* buffer_data = malloc(buffer_data_len);
                assert(buffer_data != nullptr);

                memcpy(buffer_data, data, buffer_data_len);

                lock_guard g2(m_buffers_mx);  // g2
                shared_lock g0(m_context_mx); // g0
                return instance_buffer(
                    buffer_data,
                    buffer_data_len,
                    sample_rate,
                    format);
            }
            const je_audio_buffer_t* load_buffer_wav(const char* path)
            {
                // Wav文件数据体模块
                struct WAVE_Data
                {
                    char subChunkID[4];    // should contain the word data
                    int32_t subChunk2Size; // Stores the size of the data block
                };
                // wav文件数据参数类型
                struct WAVE_Format
                {
                    char subChunkID[4];
                    int32_t subChunkSize;
                    int16_t audioFormat;
                    int16_t numChannels;
                    int32_t sampleRate;
                    int32_t byteRate;
                    int16_t blockAlign;
                    int16_t bitsPerSample;
                };
                // RIFF块标准模型
                struct RIFF_Header
                {
                    char chunkID[4];
                    int32_t chunkSize; // size not including chunkSize or chunkID
                    char format[4];
                };

                WAVE_Format wave_format;
                RIFF_Header riff_header;
                WAVE_Data wave_data;

                je_file_t* wav_file = je_file_open(path);
                if (wav_file == nullptr)
                {
                    debug::logerr("Failed to open file: '%s', unable to create audio buffer.", path);
                    return nullptr;
                }
                // Read in the first chunk into the struct

                je_file_read(&riff_header, sizeof(RIFF_Header), 1, wav_file);

                // check for RIFF and WAVE tag in memeory
                if ((riff_header.chunkID[0] != 'R' ||
                    riff_header.chunkID[1] != 'I' ||
                    riff_header.chunkID[2] != 'F' ||
                    riff_header.chunkID[3] != 'F') ||
                    (riff_header.format[0] != 'W' ||
                        riff_header.format[1] != 'A' ||
                        riff_header.format[2] != 'V' ||
                        riff_header.format[3] != 'E'))
                {
                    debug::logerr("Invalid wav file: '%s', bad format tag.", path);
                    je_file_close(wav_file);
                    return nullptr;
                }

                // Read in the 2nd chunk for the wave info
                je_file_read(&wave_format, sizeof(WAVE_Format), 1, wav_file);
                // check for fmt tag in memory
                if (wave_format.subChunkID[0] != 'f' ||
                    wave_format.subChunkID[1] != 'm' ||
                    wave_format.subChunkID[2] != 't' ||
                    wave_format.subChunkID[3] != ' ')
                {
                    debug::logerr("Invalid wav file: '%s', bad format head.", path);
                    je_file_close(wav_file);
                    return nullptr;
                }

                // check for extra parameters;
                if (wave_format.subChunkSize > 16)
                {
                    je_file_seek(
                        wav_file,
                        wave_format.subChunkSize - 16,
                        je_file_seek_mode_t::JE_READ_FILE_SEEK_CURRENT);
                }

                do
                {
                    // Read in the the last byte of data before the sound file
                    if (0 == je_file_read(&wave_data, sizeof(WAVE_Data), 1, wav_file))
                    {
                        debug::logerr("Invalid wav file: '%s', bad data head.", path);
                        je_file_close(wav_file);
                        return nullptr;
                    }

                    // check for data tag in memory
                    if (wave_data.subChunkID[0] == 'd' &&
                        wave_data.subChunkID[1] == 'a' &&
                        wave_data.subChunkID[2] == 't' &&
                        wave_data.subChunkID[3] == 'a')
                    {
                        // data chunk found.
                        break;
                    }
                    else
                    {
                        // Skip the chunk data we don't care about
                        je_file_seek(wav_file, wave_data.subChunk2Size, JE_READ_FILE_SEEK_CURRENT);
                    }

                } while (true);

                // Allocate memory for data
                void* data = malloc(wave_data.subChunk2Size);

                // Read in the sound data into the soundData variable
                if (!je_file_read(data, wave_data.subChunk2Size, 1, wav_file))
                {
                    debug::logerr("Invalid wav file: '%s', bad data size.", path);
                    free(data);
                    je_file_close(wav_file);
                    return nullptr;
                }

                je_file_close(wav_file);

                if ((wave_format.byteRate !=
                    wave_format.numChannels * wave_format.bitsPerSample * wave_format.sampleRate / 8) ||
                    (wave_format.blockAlign !=
                        wave_format.numChannels * wave_format.bitsPerSample / 8))
                {
                    debug::logerr("Invalid wav file: '%s', bad data format.", path);
                    free(data);
                    return nullptr;
                }

                je_audio_format_t wav_format = JE_AUDIO_FORMAT_MONO8;

                // The format is worked out by looking at the number of
                // channels and the bits per sample.
                switch (wave_format.bitsPerSample)
                {
                case 8:
                    if (wave_format.numChannels == 1)
                    {
                        wav_format = JE_AUDIO_FORMAT_MONO8;
                        break;
                    }
                    else if (wave_format.numChannels == 2)
                    {
                        wav_format = JE_AUDIO_FORMAT_STEREO8;
                        break;
                    }
                    [[fallthrough]];
                case 16:
                    if (wave_format.numChannels == 1)
                    {
                        wav_format = JE_AUDIO_FORMAT_MONO16;
                        break;
                    }
                    else if (wave_format.numChannels == 2)
                    {
                        wav_format = JE_AUDIO_FORMAT_STEREO16;
                        break;
                    }
                    [[fallthrough]];
                case 32:
                    if (wave_format.numChannels == 1)
                    {
                        wav_format = JE_AUDIO_FORMAT_MONO32F;
                        break;
                    }
                    else if (wave_format.numChannels == 2)
                    {
                        wav_format = JE_AUDIO_FORMAT_STEREO32F;
                        break;
                    }
                    [[fallthrough]];
                default:
                    debug::logerr("Invalid wav file: '%s', bad data format.", path);
                    free(data);
                    return nullptr;
                }

                lock_guard g2(m_buffers_mx);  // g2
                shared_lock g0(m_context_mx); // g0
                return instance_buffer(
                    data,
                    wave_data.subChunk2Size,
                    wave_format.sampleRate,
                    wav_format);
            }
            void close_buffer(const je_audio_buffer_t* buffer)
            {
                lock_guard g2(m_buffers_mx);  // g2
                shared_lock g0(m_context_mx); // g0

                AudioContextHelper::decrease_ref(buffer);
            }

            je_audio_source_t* create_source()
            {
                je_audio_source_t* source = new je_audio_source_t;

                source->m_source_instance = nullptr;
                source->m_loop = false;
                source->m_gain = 1.0f;
                source->m_pitch = 1.0f;
                source->m_location[0] = 0.0f;
                source->m_location[1] = 0.0f;
                source->m_location[2] = 0.0f;
                source->m_velocity[0] = 0.0f;
                source->m_velocity[1] = 0.0f;
                source->m_velocity[2] = 0.0f;

                lock_guard g1(m_sources_mx);  // g1
                shared_lock g0(m_context_mx); // g0

                m_sources.emplace(source);
                update_source_lockfree(source);

                return source;
            }
            void close_source(je_audio_source_t* source)
            {
                // NOTE: 此处锁定 g2-g4 是因为源在关闭时，需要修改/关闭引用计数
                lock_guard g4(m_effect_slots_mx); // g4
                lock_guard g3(m_effects_mx);      // g3
                lock_guard g2(m_buffers_mx);      // g2
                lock_guard g1(m_sources_mx);      // g1
                shared_lock g0(m_context_mx);     // g0

                // Remove from the set.
                m_sources.erase(source);
                shutdown_source(source);
            }
            void update_source(je_audio_source_t* source)
            {
                // NOTE: 尽管update_source 可能读取buffer的状态，但buffer没有内部可变性，
                //      并且纯粹的update操作不涉及引用计数的变更和实例的创建/销毁，因此
                //      这里不需要加锁g2
                lock_guard g1(m_sources_mx);  // g1
                shared_lock g0(m_context_mx); // g0

                update_source_lockfree(source);
            }
            void set_source_effect_slot_and_filter(
                je_audio_source_t* source,
                size_t slot_idx,
                je_audio_effect_slot_t* slot_may_null,
                je_audio_filter_t* filter_may_null)
            {
                assert(source != nullptr);
                assert(slot_idx < audio::MAX_AUXILIARY_SENDS);

                lock_guard g4(m_effect_slots_mx); // g4
                lock_guard g1(m_sources_mx);      // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        assert(slot_may_null == nullptr
                            || slot_may_null->m_effect_slot_instance != nullptr);

                        auto& slot_and_filters = source->m_source_instance->m_playing_effect_slot_and_filters[slot_idx];
                        slot_and_filters.m_slot.set_res_may_null(slot_may_null);
                        slot_and_filters.m_filter.set_res_may_null(filter_may_null);

                        if (m_alext_efx.has_value())
                        {
                            ALint slot_id = AL_EFFECTSLOT_NULL;
                            ALint filter_id = AL_FILTER_NULL;

                            if (slot_may_null != nullptr)
                                slot_id = slot_may_null->m_effect_slot_instance->m_effect_slot_id;

                            if (filter_may_null != nullptr)
                                filter_id = filter_may_null->m_filter_instance->m_filter_id;

                            alSource3i(source->m_source_instance->m_source_id,
                                AL_AUXILIARY_SEND_FILTER,
                                slot_id,
                                slot_idx,
                                filter_id);
                        }
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& slot_and_filters = dump_instance->m_playing_effect_slot_and_filters[slot_idx];

                        slot_and_filters.m_slot.set_res_may_null(slot_may_null);
                        slot_and_filters.m_filter.set_res_may_null(filter_may_null);
                    }

                    break;
                }
            }
            void set_source_filter(je_audio_source_t* source, je_audio_filter_t* filter_may_null)
            {
                assert(source != nullptr);

                lock_guard g5(m_filters_mx);       // g5
                lock_guard g1(m_sources_mx);       // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        assert(filter_may_null == nullptr
                            || filter_may_null->m_filter_instance != nullptr);

                        source->m_source_instance->m_playing_filter.set_res_may_null(filter_may_null);

                        if (m_alext_efx.has_value())
                        {
                            ALint filter_id = AL_FILTER_NULL;
                            if (filter_may_null != nullptr)
                                filter_id = filter_may_null->m_filter_instance->m_filter_id;

                            alSourcei(source->m_source_instance->m_source_id, AL_DIRECT_FILTER, filter_id);
                        }
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        dump_instance->m_playing_filter.set_res_may_null(filter_may_null);
                    }

                    break;
                }
            }
            bool set_source_buffer(je_audio_source_t* source, const je_audio_buffer_t* buffer)
            {
                assert(source != nullptr);
                assert(buffer != nullptr);

                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                bool buffer_changed = false;
                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        assert(buffer->m_buffer_instance != nullptr);
                        if (!source->m_source_instance->m_playing_buffer.has_value()
                            || source->m_source_instance->m_playing_buffer.value() != buffer)
                        {
                            source->m_source_instance->m_playing_buffer.set_res(buffer);

                            ALenum play_state;
                            alGetSourcei(
                                source->m_source_instance->m_source_id,
                                AL_SOURCE_STATE,
                                &play_state);

                            if (play_state != AL_INITIAL && play_state != AL_STOPPED)
                                // 对正在播放的 source 设置 buffer 时，应该停止当前的播放
                                alSourceStop(source->m_source_instance->m_source_id);

                            alSourcei(
                                source->m_source_instance->m_source_id,
                                AL_BUFFER,
                                buffer->m_buffer_instance->m_buffer_id);

                            buffer_changed = true;
                        }
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);

                        if (!dump_instance->m_playing_buffer.has_value()
                            || dump_instance->m_playing_buffer.value() != buffer)
                        {
                            auto& dump = dump_instance->m_dump.value();

                            dump_instance->m_playing_buffer.set_res(buffer);
                            dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_STOPPED;
                            dump.m_play_process = 0;

                            buffer_changed = true;
                        }
                    }
                    break;
                }
                return buffer_changed;
            }
            void play_source(je_audio_source_t* source)
            {
                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        if (source->m_source_instance->m_playing_buffer.has_value())
                            alSourcePlay(source->m_source_instance->m_source_id);
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (dump_instance->m_playing_buffer.has_value())
                        {
                            if (dump.m_play_state == je_audio_state_t::JE_AUDIO_STATE_STOPPED)
                                dump.m_play_process = 0;

                            dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_PLAYING;
                        }
                    }
                    break;
                }
            }
            void pause_source(je_audio_source_t* source)
            {
                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        if (source->m_source_instance->m_playing_buffer.has_value())
                            alSourcePause(source->m_source_instance->m_source_id);
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (dump_instance->m_playing_buffer.has_value())
                        {
                            if (dump.m_play_state == je_audio_state_t::JE_AUDIO_STATE_PLAYING)
                                dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_PAUSED;
                        }
                    }
                    break;
                }
            }
            void stop_source(je_audio_source_t* source)
            {
                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        if (source->m_source_instance->m_playing_buffer.has_value())
                            alSourceStop(source->m_source_instance->m_source_id);
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (dump_instance->m_playing_buffer.has_value())
                        {
                            if (dump.m_play_state == je_audio_state_t::JE_AUDIO_STATE_PLAYING)
                                dump.m_play_state = je_audio_state_t::JE_AUDIO_STATE_STOPPED;
                        }
                    }
                    break;
                }
            }
            je_audio_state_t get_source_play_state(je_audio_source_t* source)
            {
                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        if (!source->m_source_instance->m_playing_buffer.has_value())
                            return je_audio_state_t::JE_AUDIO_STATE_STOPPED;

                        ALenum play_state;
                        alGetSourcei(
                            source->m_source_instance->m_source_id,
                            AL_SOURCE_STATE,
                            &play_state);

                        switch (play_state)
                        {
                        case AL_PLAYING:
                            return je_audio_state_t::JE_AUDIO_STATE_PLAYING;
                        case AL_PAUSED:
                            return je_audio_state_t::JE_AUDIO_STATE_PAUSED;
                        default:
                            assert(play_state == AL_STOPPED || play_state == AL_INITIAL);
                            return je_audio_state_t::JE_AUDIO_STATE_STOPPED;
                        }
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (dump_instance->m_playing_buffer.has_value())
                            return dump.m_play_state;

                        return je_audio_state_t::JE_AUDIO_STATE_STOPPED;
                    }
                    break;
                }
            }
            size_t get_source_play_process(je_audio_source_t* source)
            {
                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        if (!source->m_source_instance->m_playing_buffer.has_value())
                            return 0;

                        ALint play_process;
                        alGetSourcei(
                            source->m_source_instance->m_source_id,
                            AL_BYTE_OFFSET,
                            &play_process);

                        return (size_t)play_process;
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (dump_instance->m_playing_buffer.has_value())
                            return dump.m_play_process;

                        return 0;
                    }
                    break;
                }
            }
            void set_source_play_process(je_audio_source_t* source, size_t offset)
            {
                auto align_offset =
                    [](const je_audio_buffer_t* buffer, size_t offset)
                    {
                        if (offset % buffer->m_sample_size != 0)
                        {
                            debug::logwarn("Audio buffer offset is not aligned.");
                            offset = offset / buffer->m_sample_size * buffer->m_sample_size;
                        }
                        return (ALint)min(offset, buffer->m_size);
                    };

                lock_guard g2(m_buffers_mx); // g2
                lock_guard g1(m_sources_mx); // g1

                for (;;)
                {
                    if (source->m_source_instance != nullptr)
                    {
                        shared_lock g0(m_context_mx); // g0
                        if (source->m_source_instance == nullptr)
                            // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                            continue;

                        auto& play_buffer = source->m_source_instance->m_playing_buffer;
                        if (!play_buffer.has_value())
                            return;

                        alSourcei(
                            source->m_source_instance->m_source_id,
                            AL_BYTE_OFFSET,
                            align_offset(play_buffer.value(), offset));
                    }
                    else
                    {
                        lock_guard g0(m_context_mx); // g0
                        if (source->m_source_instance != nullptr)
                            // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                            continue;

                        // 此时没有设备，但状态需要装填到转储中
                        auto* dump_instance = require_dump_for_no_device_context_source(source);
                        auto& dump = dump_instance->m_dump.value();

                        if (!dump_instance->m_playing_buffer.has_value())
                            return;

                        dump.m_play_process =
                            align_offset(dump_instance->m_playing_buffer.value(), offset);
                    }
                    break;
                }
            }

            void update_listener()
            {
                shared_lock g0(m_context_mx); // g0

                if (m_current_play_device.has_value())
                {
                    alListenerf(AL_GAIN,
                        m_listener.m_gain * m_listener.m_global_gain);
                    alListener3f(AL_POSITION,
                        m_listener.m_location[0],
                        m_listener.m_location[1],
                        -m_listener.m_location[2]);
                    alListener3f(AL_VELOCITY,
                        m_listener.m_velocity[0],
                        m_listener.m_velocity[1],
                        -m_listener.m_velocity[2]);

                    float orientation[6] = {
                        m_listener.m_forward[0],
                        m_listener.m_forward[1],
                        -m_listener.m_forward[2],
                        m_listener.m_upward[0],
                        m_listener.m_upward[1],
                        -m_listener.m_upward[2],
                    };
                    alListenerfv(AL_ORIENTATION, orientation);
                }
            }
            je_audio_listener_t* get_listener()
            {
                return &m_listener;
            }

            void refetch_devices_and_do(
                je_audio_enumerate_device_callback_t callback, void* userdata)
            {
                lock_guard g0(m_context_mx); // g0

                refetch_and_update_enumed_play_devices();

                auto* using_device = callback(
                    m_enumed_play_devices.data(),
                    m_enumed_play_devices.size(),
                    userdata);

                if (using_device != nullptr)
                {
                    if (m_current_play_device.has_value())
                        deactive_using_device(m_current_play_device.value());

                    active_using_device(
                        const_cast<je_audio_play_device_t*>(using_device));
                }
            }

            void disconnect_all_devices()
            {
                lock_guard g0(m_context_mx); // g0

                update_enumed_play_devices({});

                debug::loginfo("All audio devices disconnected.");
            }

            void audio_device_update_thread_job()
            {
                using namespace std;
                for (;;)
                {
                    device_updating_shared_unique_lock ug(m_context_mx);

                    bool need_update_device = false;
                    m_context_cv_for_device_updating.wait_for(
                        ug,
                        1s,
                        [this, &need_update_device]()
                        {
                            // Check for 
                            auto* current_device_may_null = m_current_play_device.value_or(nullptr);

                            if (m_context_updating_thread_quit)
                                return true;

                            if (current_device_may_null != nullptr)
                            {
                                assert(current_device_may_null->m_active);

                                need_update_device =
                                    // 如果设备已经断开
                                    (!check_device_instance_connected(current_device_may_null->m_device_instance))
                                    // 或者默认设备被切换
                                    || (current_device_may_null->m_is_default
                                        && strcmp(current_device_may_null->m_name, get_device_names()) != 0);
                            }
                            return need_update_device;
                        });

                    if (m_context_updating_thread_quit)
                        break;

                    if (need_update_device)
                    {
                        // 设备可能被移除，或者默认设备被切换, 重新枚举设备
                        ug.unlock();

                        // 切换到默认设备
                        refetch_devices_and_do(
                            [](
                                const je_audio_play_device_t* enumerated_devices,
                                size_t enumerated_device_count,
                                void* userdata)-> const je_audio_play_device_t*
                            {
                                if (enumerated_device_count != 0)
                                {
                                    assert(enumerated_devices[0].m_is_default);
                                    return &enumerated_devices[0];
                                }
                                return nullptr;
                            },
                            nullptr);
                    }
                }
            }

            AudioContext()
                : m_openal_ALC_ENUMERATE_ALL_EXT_enabled(
                    alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
                , m_context_updating_thread_quit(false)
                , m_current_play_device(nullopt)
            {
                m_listener.m_gain = 1.0f;
                m_listener.m_global_gain = 1.0f;
                m_listener.m_location[0] = 0.0f;
                m_listener.m_location[1] = 0.0f;
                m_listener.m_location[2] = 0.0f;
                m_listener.m_velocity[0] = 0.0f;
                m_listener.m_velocity[1] = 0.0f;
                m_listener.m_velocity[2] = 0.0f;
                m_listener.m_forward[0] = 0.0f;
                m_listener.m_forward[1] = 0.0f;
                m_listener.m_forward[2] = 1.0f;
                m_listener.m_upward[0] = 0.0f;
                m_listener.m_upward[1] = 1.0f;
                m_listener.m_upward[2] = 0.0f;

                refetch_and_update_enumed_play_devices();

                update_listener();

                m_context_updating_thread =
                    thread(&AudioContext::audio_device_update_thread_job, this);
            }
            ~AudioContext()
            {
                do
                {
                    lock_guard g0(m_context_mx); // g0
                    m_context_updating_thread_quit = true;
                    m_context_cv_for_device_updating.notify_all();

                } while (0);
                m_context_updating_thread.join();

                update_enumed_play_devices({});

                for (auto* buffer : m_buffers)
                    shutdown_buffer(buffer);

                for (auto* source : m_sources)
                    shutdown_source(source);

                for (auto* effect : m_effects)
                    shutdown_effect(effect);

                for (auto* slot : m_effect_slots)
                    shutdown_effect_slot(slot);
            }
        };

        static AudioContext* g_engine_audio_context = nullptr;

        void AudioContextHelper::increase_ref(const je_audio_buffer_t* buffer)
        {
            ++buffer->m_references;
        }
        void AudioContextHelper::decrease_ref(const je_audio_buffer_t* buffer)
        {
            if (0 == --buffer->m_references)
            {
                // Remove from the set.
                g_engine_audio_context->close_buffer_lockfree(buffer);
            }
        }

        void AudioContextHelper::increase_ref(AudioEffectHead* effect)
        {
            ++effect->m_references;
        }
        void AudioContextHelper::decrease_ref(AudioEffectHead* effect)
        {
            if (0 == --effect->m_references)
            {
                // Remove from the set.
                g_engine_audio_context->close_effect_lockfree(effect);
            }
        }

        void AudioContextHelper::increase_ref(je_audio_effect_slot_t* slot)
        {
            ++slot->m_references;
        }
        void AudioContextHelper::decrease_ref(je_audio_effect_slot_t* slot)
        {
            if (0 == --slot->m_references)
            {
                // Remove from the set.
                g_engine_audio_context->close_effect_slot_lockfree(slot);
            }
        }

        void AudioContextHelper::increase_ref(je_audio_filter_t* filter)
        {
            ++filter->m_references;
        }
        void AudioContextHelper::decrease_ref(je_audio_filter_t* filter)
        {
            if (0 == --filter->m_references)
            {
                // Remove from the set.
                g_engine_audio_context->close_filter_lockfree(filter);
            }
        }

        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_reverb_t* reverb)
        {
            head->m_effect_kind = AL_EFFECT_REVERB;

            reverb->m_density = AL_REVERB_DEFAULT_DENSITY;
            reverb->m_diffusion = AL_REVERB_DEFAULT_DIFFUSION;
            reverb->m_gain = AL_REVERB_DEFAULT_GAIN;
            reverb->m_gain_hf = AL_REVERB_DEFAULT_GAINHF;
            reverb->m_decay_time = AL_REVERB_DEFAULT_DECAY_TIME;
            reverb->m_decay_hf_ratio = AL_REVERB_DEFAULT_DECAY_HFRATIO;
            reverb->m_reflections_gain = AL_REVERB_DEFAULT_REFLECTIONS_GAIN;
            reverb->m_reflections_delay = AL_REVERB_DEFAULT_REFLECTIONS_DELAY;
            reverb->m_late_reverb_gain = AL_REVERB_DEFAULT_LATE_REVERB_GAIN;
            reverb->m_late_reverb_delay = AL_REVERB_DEFAULT_LATE_REVERB_DELAY;
            reverb->m_air_absorption_gain_hf = AL_REVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
            reverb->m_room_rolloff_factor = AL_REVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
            reverb->m_decay_hf_limit = AL_REVERB_DEFAULT_DECAY_HFLIMIT != AL_FALSE;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_chorus_t* chorus)
        {
            head->m_effect_kind = AL_EFFECT_CHORUS;

            chorus->m_waveform = (je_audio_effect_chorus_waveform_t)AL_CHORUS_DEFAULT_WAVEFORM;
            chorus->m_phase = AL_CHORUS_DEFAULT_PHASE;
            chorus->m_rate = AL_CHORUS_DEFAULT_RATE;
            chorus->m_depth = AL_CHORUS_DEFAULT_DEPTH;
            chorus->m_feedback = AL_CHORUS_DEFAULT_FEEDBACK;
            chorus->m_delay = AL_CHORUS_DEFAULT_DELAY;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_distortion_t* distortion)
        {
            head->m_effect_kind = AL_EFFECT_DISTORTION;

            distortion->m_edge = AL_DISTORTION_DEFAULT_EDGE;
            distortion->m_gain = AL_DISTORTION_DEFAULT_GAIN;
            distortion->m_lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
            distortion->m_equalizer_center_freq = AL_DISTORTION_DEFAULT_EQCENTER;
            distortion->m_equalizer_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_echo_t* echo)
        {
            head->m_effect_kind = AL_EFFECT_ECHO;

            echo->m_delay = AL_ECHO_DEFAULT_DELAY;
            echo->m_lr_delay = AL_ECHO_DEFAULT_LRDELAY;
            echo->m_damping = AL_ECHO_DEFAULT_DAMPING;
            echo->m_feedback = AL_ECHO_DEFAULT_FEEDBACK;
            echo->m_spread = AL_ECHO_DEFAULT_SPREAD;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_flanger_t* flanger)
        {
            head->m_effect_kind = AL_EFFECT_FLANGER;

            flanger->m_waveform = (je_audio_effect_flanger_waveform_t)AL_FLANGER_DEFAULT_WAVEFORM;
            flanger->m_phase = AL_FLANGER_DEFAULT_PHASE;
            flanger->m_rate = AL_FLANGER_DEFAULT_RATE;
            flanger->m_depth = AL_FLANGER_DEFAULT_DEPTH;
            flanger->m_feedback = AL_FLANGER_DEFAULT_FEEDBACK;
            flanger->m_delay = AL_FLANGER_DEFAULT_DELAY;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_frequency_shifter_t* shifter)
        {
            head->m_effect_kind = AL_EFFECT_FREQUENCY_SHIFTER;

            shifter->m_frequency = AL_FREQUENCY_SHIFTER_DEFAULT_FREQUENCY;
            shifter->m_left_direction =
                (je_audio_effect_frequency_shifter_direction_t)AL_FREQUENCY_SHIFTER_DEFAULT_LEFT_DIRECTION;
            shifter->m_right_direction =
                (je_audio_effect_frequency_shifter_direction_t)AL_FREQUENCY_SHIFTER_DEFAULT_RIGHT_DIRECTION;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_vocal_morpher_t* morpher)
        {
            head->m_effect_kind = AL_EFFECT_VOCAL_MORPHER;

            morpher->m_phoneme_a = (je_audio_effect_vocal_morpher_phoneme_t)AL_VOCAL_MORPHER_DEFAULT_PHONEMEA;
            morpher->m_phoneme_a_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEA_COARSE_TUNING;
            morpher->m_phoneme_b = (je_audio_effect_vocal_morpher_phoneme_t)AL_VOCAL_MORPHER_DEFAULT_PHONEMEB;
            morpher->m_phoneme_b_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEB_COARSE_TUNING;
            morpher->m_waveform = (je_audio_effect_vocal_morpher_waveform_t)AL_VOCAL_MORPHER_DEFAULT_WAVEFORM;
            morpher->m_rate = AL_VOCAL_MORPHER_DEFAULT_RATE;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_pitch_shifter_t* shifter)
        {
            head->m_effect_kind = AL_EFFECT_PITCH_SHIFTER;

            shifter->m_coarse_tune = AL_PITCH_SHIFTER_DEFAULT_COARSE_TUNE;
            shifter->m_fine_tune = AL_PITCH_SHIFTER_DEFAULT_FINE_TUNE;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_ring_modulator_t* modulator)
        {
            head->m_effect_kind = AL_EFFECT_RING_MODULATOR;

            modulator->m_frequency = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
            modulator->m_highpass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
            modulator->m_waveform = (je_audio_effect_ring_modulator_waveform_t)AL_RING_MODULATOR_DEFAULT_WAVEFORM;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_autowah_t* wah)
        {
            head->m_effect_kind = AL_EFFECT_AUTOWAH;

            wah->m_attack_time = AL_AUTOWAH_DEFAULT_ATTACK_TIME;
            wah->m_release_time = AL_AUTOWAH_DEFAULT_RELEASE_TIME;
            wah->m_resonance = AL_AUTOWAH_DEFAULT_RESONANCE;
            wah->m_peak_gain = AL_AUTOWAH_DEFAULT_PEAK_GAIN;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_compressor_t* compressor)
        {
            head->m_effect_kind = AL_EFFECT_COMPRESSOR;

            compressor->m_enabled = AL_COMPRESSOR_DEFAULT_ONOFF != AL_FALSE;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_equalizer_t* equalizer)
        {
            head->m_effect_kind = AL_EFFECT_EQUALIZER;

            equalizer->m_low_gain = AL_EQUALIZER_DEFAULT_LOW_GAIN;
            equalizer->m_low_cutoff = AL_EQUALIZER_DEFAULT_LOW_CUTOFF;
            equalizer->m_mid1_gain = AL_EQUALIZER_DEFAULT_MID1_GAIN;
            equalizer->m_mid1_center = AL_EQUALIZER_DEFAULT_MID1_CENTER;
            equalizer->m_mid1_width = AL_EQUALIZER_DEFAULT_MID1_WIDTH;
            equalizer->m_mid2_gain = AL_EQUALIZER_DEFAULT_MID2_GAIN;
            equalizer->m_mid2_center = AL_EQUALIZER_DEFAULT_MID2_CENTER;
            equalizer->m_mid2_width = AL_EQUALIZER_DEFAULT_MID2_WIDTH;
            equalizer->m_high_gain = AL_EQUALIZER_DEFAULT_HIGH_GAIN;
            equalizer->m_high_cutoff = AL_EQUALIZER_DEFAULT_HIGH_CUTOFF;
        }
        void AudioContextHelper::effect_init_default(AudioEffectHead* head, je_audio_effect_eax_reverb_t* eaxreverb)
        {
            head->m_effect_kind = AL_EFFECT_EAXREVERB;

            eaxreverb->m_density = AL_EAXREVERB_DEFAULT_DENSITY;
            eaxreverb->m_diffusion = AL_EAXREVERB_DEFAULT_DIFFUSION;
            eaxreverb->m_gain = AL_EAXREVERB_DEFAULT_GAIN;
            eaxreverb->m_gain_hf = AL_EAXREVERB_DEFAULT_GAINHF;
            eaxreverb->m_gain_lf = AL_EAXREVERB_DEFAULT_GAINLF;
            eaxreverb->m_decay_time = AL_EAXREVERB_DEFAULT_DECAY_TIME;
            eaxreverb->m_decay_hf_ratio = AL_EAXREVERB_DEFAULT_DECAY_HFRATIO;
            eaxreverb->m_decay_lf_ratio = AL_EAXREVERB_DEFAULT_DECAY_LFRATIO;
            eaxreverb->m_reflections_gain = AL_EAXREVERB_DEFAULT_REFLECTIONS_GAIN;
            eaxreverb->m_reflections_delay = AL_EAXREVERB_DEFAULT_REFLECTIONS_DELAY;
            eaxreverb->m_reflections_pan_xyz[0] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
            eaxreverb->m_reflections_pan_xyz[1] = AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
            eaxreverb->m_reflections_pan_xyz[2] = -AL_EAXREVERB_DEFAULT_REFLECTIONS_PAN_XYZ;
            eaxreverb->m_late_reverb_gain = AL_EAXREVERB_DEFAULT_LATE_REVERB_GAIN;
            eaxreverb->m_late_reverb_delay = AL_EAXREVERB_DEFAULT_LATE_REVERB_DELAY;
            eaxreverb->m_late_reverb_pan_xyz[0] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
            eaxreverb->m_late_reverb_pan_xyz[1] = AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
            eaxreverb->m_late_reverb_pan_xyz[2] = -AL_EAXREVERB_DEFAULT_LATE_REVERB_PAN_XYZ;
            eaxreverb->m_echo_time = AL_EAXREVERB_DEFAULT_ECHO_TIME;
            eaxreverb->m_echo_depth = AL_EAXREVERB_DEFAULT_ECHO_DEPTH;
            eaxreverb->m_modulation_time = AL_EAXREVERB_DEFAULT_MODULATION_TIME;
            eaxreverb->m_modulation_depth = AL_EAXREVERB_DEFAULT_MODULATION_DEPTH;
            eaxreverb->m_air_absorption_gain_hf = AL_EAXREVERB_DEFAULT_AIR_ABSORPTION_GAINHF;
            eaxreverb->m_hf_reference = AL_EAXREVERB_DEFAULT_HFREFERENCE;
            eaxreverb->m_lf_reference = AL_EAXREVERB_DEFAULT_LFREFERENCE;
            eaxreverb->m_room_rolloff_factor = AL_EAXREVERB_DEFAULT_ROOM_ROLLOFF_FACTOR;
            eaxreverb->m_decay_hf_limit = AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT != AL_FALSE;
        }
    }
}

void jeal_init()
{
    assert(g_engine_audio_context == nullptr);
    g_engine_audio_context = new AudioContext;
}
void jeal_finish()
{
    assert(g_engine_audio_context != nullptr);
    delete g_engine_audio_context;
    g_engine_audio_context = nullptr;
}
/* OPTIONAL: If format is invalid or buffer_data_len is not aligned to sample size */ const je_audio_buffer_t* je_audio_buffer_create(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    je_audio_format_t format)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_buffer(
        data, buffer_data_len, sample_rate, format);
}
/* OPTIONAL: If wav file failed to load */ const je_audio_buffer_t* je_audio_buffer_load_wav(const char* path)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->load_buffer_wav(path);
}
void je_audio_buffer_close(const je_audio_buffer_t* buffer)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->close_buffer(buffer);
}
je_audio_source_t* je_audio_source_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_source();
}
void je_audio_source_update(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->update_source(source);
}
void je_audio_source_close(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->close_source(source);
}
bool je_audio_source_buffer_set(je_audio_source_t* source, const je_audio_buffer_t* buffer)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->set_source_buffer(source, buffer);
}
void je_audio_source_effect_slot_and_filter_set(
    je_audio_source_t* source,
    size_t slot_idx,
    /* OPTIONAL */ je_audio_effect_slot_t* slot_may_null,
    /* OPTIONAL */ je_audio_filter_t* filter_may_null)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->set_source_effect_slot_and_filter(
        source, slot_idx, slot_may_null, filter_may_null);
}

void je_audio_source_filter_set(je_audio_source_t* source, /* OPTIONAL */ je_audio_filter_t* filter_may_null)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->set_source_filter(source, filter_may_null);
}

void je_audio_source_play(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->play_source(source);
}
void je_audio_source_pause(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->pause_source(source);
}
void je_audio_source_stop(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->stop_source(source);
}
je_audio_state_t je_audio_source_play_state_get(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->get_source_play_state(source);
}
size_t je_audio_source_play_process_get(je_audio_source_t* source)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->get_source_play_process(source);
}
void je_audio_source_play_process_set(je_audio_source_t* source, size_t offset)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->set_source_play_process(source, offset);
}
je_audio_listener_t* je_audio_listener_get()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->get_listener();
}
void je_audio_listener_update()
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->update_listener();
}

je_audio_filter_t* je_audio_filter_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_filter();
}

void je_audio_filter_update(je_audio_filter_t* filter)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->update_filter(filter);
}

void je_audio_filter_close(je_audio_filter_t* filter)
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->close_filter(filter);
}

je_audio_effect_slot_t* je_audio_effect_slot_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect_slot();
}
void je_audio_effect_slot_bind(je_audio_effect_slot_t* slot, /* OPTIONAL */ void* effect_may_null)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->set_slot_effect(
        slot, effect_may_null != nullptr ? JE_EFFECT_HEAD(effect_may_null) : nullptr);
}
void je_audio_effect_slot_close(je_audio_effect_slot_t* slot)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->close_effect_slot(slot);
}
void je_audio_effect_slot_update(je_audio_effect_slot_t* slot)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->update_effect_slot(slot);
}

je_audio_effect_reverb_t* je_audio_effect_reverb_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_reverb_t>();
}
je_audio_effect_chorus_t* je_audio_effect_chorus_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_chorus_t>();
}
je_audio_effect_distortion_t* je_audio_effect_distortion_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_distortion_t>();
}
je_audio_effect_echo_t* je_audio_effect_echo_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_echo_t>();
}
je_audio_effect_flanger_t* je_audio_effect_flanger_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_flanger_t>();
}
je_audio_effect_frequency_shifter_t* je_audio_effect_frequency_shifter_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_frequency_shifter_t>();
}
je_audio_effect_vocal_morpher_t* je_audio_effect_vocal_morpher_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_vocal_morpher_t>();
}
je_audio_effect_pitch_shifter_t* je_audio_effect_pitch_shifter_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_pitch_shifter_t>();
}
je_audio_effect_ring_modulator_t* je_audio_effect_ring_modulator_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_ring_modulator_t>();
}
je_audio_effect_autowah_t* je_audio_effect_autowah_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_autowah_t>();
}
je_audio_effect_compressor_t* je_audio_effect_compressor_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_compressor_t>();
}
je_audio_effect_equalizer_t* je_audio_effect_equalizer_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_equalizer_t>();
}
je_audio_effect_eax_reverb_t* je_audio_effect_eaxreverb_create()
{
    assert(g_engine_audio_context != nullptr);

    return g_engine_audio_context->create_effect<je_audio_effect_eax_reverb_t>();
}
void je_audio_effect_close(void* effect)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->close_effect(effect);
}
void je_audio_effect_update(void* effect)
{
    assert(g_engine_audio_context != nullptr);

    g_engine_audio_context->update_effect(effect);
}

void je_audio_devices_enumerate_and_do(
    je_audio_enumerate_device_callback_t callback, /* OPTIONAL */ void* userdata)
{
    assert(g_engine_audio_context != nullptr);
    g_engine_audio_context->refetch_devices_and_do(
        callback,
        userdata);
}

void je_audio_devices_disconnect_all()
{
    assert(g_engine_audio_context != nullptr);
    g_engine_audio_context->disconnect_all_devices();
}

#undef JE_AL_DEVICE
#undef AL_JE_DEVICE
#undef JE_EFFECT_HEAD
#undef JE_EFFECT_DATA
