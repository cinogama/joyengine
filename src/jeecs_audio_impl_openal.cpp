#define JE_IMPL
#include "jeecs.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <unordered_set>
#include <vector>
#include <mutex>

#define JE_AL_DEVICE reinterpret_cast<ALCdevice*>
#define AL_JE_DEVICE reinterpret_cast<jeal_native_play_device_instance*>

struct jeal_native_effect_instance
{
    ALuint m_effect_id;

    struct dump_data_t
    {
        // Empty
    };
    std::optional<dump_data_t> m_dump;
};
struct alignas(8) jeal_effect_head
{
    jeal_native_effect_instance* m_effect_instance;
    ALenum                          m_effect_kind;

    size_t                          m_references;
};
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_reverb) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_chorus) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_distortion) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_echo) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_flanger) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_frequency_shifter) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_vocal_morpher) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_pitch_shifter) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_ring_modulator) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_autowah) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_compressor) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_equalizer) == 0);
static_assert(sizeof(jeal_effect_head) % alignof(jeal_effect_eaxreverb) == 0);

#define JE_EFFECT_HEAD(v) (jeal_effect_head*)(reinterpret_cast<intptr_t>(v) - sizeof(jeal_effect_head))
#define JE_EFFECT_DATA(t, v) (t*)(reinterpret_cast<intptr_t>(v) + sizeof(jeal_effect_head))

namespace jeecs
{
    class AudioContextHelpler
    {
    public:
        static void increase_ref(const jeal_buffer* buffer);
        static void decrease_ref(const jeal_buffer* buffer);
        static void increase_ref(jeal_effect_head* effect);
        static void decrease_ref(jeal_effect_head* effect);
        static void increase_ref(jeal_effect_slot* effect_slot);
        static void decrease_ref(jeal_effect_slot* effect_slot);

        static void effect_init_default(jeal_effect_head* head, jeal_effect_reverb* reverb);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_chorus* chorus);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_distortion* distortion);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_echo* echo);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_flanger* flanger);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_frequency_shifter* shifter);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_vocal_morpher* morpher);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_pitch_shifter* shifter);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_ring_modulator* modulator);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_autowah* wah);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_compressor* compressor);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_equalizer* equalizer);
        static void effect_init_default(jeal_effect_head* head, jeal_effect_eaxreverb* eaxreverb);

        template<typename T>
        class Holder
        {
            JECS_DISABLE_MOVE_AND_COPY(Holder);

            std::optional<T*> m_res;
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

struct jeal_native_buffer_instance
{
    ALuint m_buffer_id;

    struct dump_data_t
    {

    };
    std::optional<dump_data_t> m_dump;
};
struct jeal_native_source_instance
{
    ALuint m_source_id;
    jeecs::AudioContextHelpler::Holder<const jeal_buffer> m_playing_buffer;
    jeecs::AudioContextHelpler::Holder<jeal_effect_slot> m_playing_effect_slot[jeecs::audio::MAX_AUXILIARY_SENDS];

    // Dump data:
    struct dump_data_t
    {
        jeal_state m_play_state;
        ALint m_play_process;
    };
    std::optional<dump_data_t> m_dump;
};
struct jeal_native_effect_slot_instance
{
    ALuint m_effect_slot_id;
    jeecs::AudioContextHelpler::Holder<jeal_effect_head> m_binding_effect;

    struct dump_data_t
    {

    };
    std::optional<dump_data_t> m_dump;
};

namespace jeecs
{
    class AudioContext
    {
        JECS_DISABLE_MOVE_AND_COPY(AudioContext);

        friend class AudioContextHelpler;

        std::shared_mutex m_context_mx; // g0

        std::vector<jeal_play_device> m_enumed_play_devices;
        std::optional<jeal_play_device*> m_current_play_device;

        std::mutex m_sources_mx;       // g1
        std::unordered_set<jeal_source*> m_sources;
        std::mutex m_buffers_mx;       // g2
        std::unordered_set<jeal_buffer*> m_buffers;
        std::mutex m_effects_mx;       // g3
        std::unordered_set<jeal_effect_head*> m_effects;
        std::mutex m_effect_slots_mx;  // g4
        std::unordered_set<jeal_effect_slot*> m_effect_slots;

        jeal_listener m_listener;

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

            OpenALEfx(jeal_native_play_device_instance* device)
            {
                auto* aldevice = JE_AL_DEVICE(device);

#define JEAL_LOAD_FUNC(name) \
                name = (decltype(name))alcGetProcAddress(aldevice, #name); \
                if (name == nullptr) \
                { \
                    jeecs::debug::logfatal("Failed to load al function: %s.", #name); \
                    return; \
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
        std::optional<OpenALEfx> m_alext_efx = std::nullopt;

        struct jeal_play_device_states_dump
        {
            std::unordered_map<jeal_buffer*, jeal_native_buffer_instance*>
                m_buffers_dump;
            std::unordered_map<jeal_source*, jeal_native_source_instance*>
                m_sources_dump;
            std::unordered_map<jeal_effect_head*, jeal_native_effect_instance*>
                m_effects_dump;
            std::unordered_map<jeal_effect_slot*, jeal_native_effect_slot_instance*>
                m_effect_slots_dump;
        };
        std::optional<jeal_play_device_states_dump> m_play_state_dump = std::nullopt;

        static bool check_device_instance_connected(jeal_native_play_device_instance* device)
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
                    dump.m_play_state = jeal_state::JE_AUDIO_STATE_PLAYING;
                    break;
                case AL_PAUSED:
                    dump.m_play_state = jeal_state::JE_AUDIO_STATE_PAUSED;
                    break;
                default:
                    assert(play_state == AL_STOPPED || play_state == AL_INITIAL);
                    dump.m_play_state = jeal_state::JE_AUDIO_STATE_STOPPED;
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
        }
        void restore_playing_states()
        {
            assert(m_play_state_dump.has_value());
            assert(m_current_play_device.has_value());

            auto& states = m_play_state_dump.value();

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

        void deactive_using_device(jeal_play_device* device)
        {
            assert(device->m_active);
            assert(device->m_device_instance != nullptr);

            jeecs::debug::loginfo("Deactive audio device: %s.", device->m_name);

            // 被关闭的设备此前正在使用中，需要保存当前的音频播放状态以供恢复
            dump_and_close_playing_states();
            m_current_play_device.reset();
            device->m_active = false;

            ALCcontext* current_context = alcGetCurrentContext();
            assert(current_context != nullptr);

            // 关闭当前设备上下文
            if (AL_FALSE == alcMakeContextCurrent(nullptr))
                jeecs::debug::logfatal("Failed to clear current audio context.");

            alcDestroyContext(current_context);
        }
        void active_using_device(jeal_play_device* device)
        {
            assert(!device->m_active);
            assert(device->m_device_instance != nullptr);

            jeecs::debug::loginfo("Active audio device: %s.", device->m_name);

            auto* current_context = alcCreateContext(JE_AL_DEVICE(device->m_device_instance), nullptr);
            if (current_context == nullptr || AL_FALSE == alcMakeContextCurrent(current_context))
            {
                jeecs::debug::logfatal("Failed to create context for device: %s.", device->m_name);
                return;
            }

            if (device->m_max_auxiliary_sends <= 0)
            {
                jeecs::debug::logwarn("Audio device: %s, does not support EFX.", device->m_name);

                // 关闭 EFX 拓展
                m_alext_efx.reset();
            }
            else
            {
                jeecs::debug::loginfo("Audio device: %s, support EFX with max %d sends for each source.",
                    device->m_name, (int)device->m_max_auxiliary_sends);

                // 开启 EFX 拓展
                m_alext_efx.emplace(device->m_device_instance);
            }

            // 设备及上下文创建完成，恢复播放状态（如果有）
            m_current_play_device.emplace(device);
            try_restore_playing_states();
            device->m_active = true;
        }

        void update_enumed_play_devices(std::vector<jeal_play_device>&& new_opened_devices)
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
                auto fnd = std::find_if(
                    m_enumed_play_devices.begin(),
                    m_enumed_play_devices.end(),
                    [](jeal_play_device& device)
                    {
                        return device.m_active;
                    });

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
        void refetch_and_update_enumed_play_devices()
        {
            const char* device_names = nullptr;
            auto enum_device_enabled = alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT");
            if (enum_device_enabled)
                device_names = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
            else
                device_names = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

            std::vector<jeal_play_device> new_opened_devices;

            const char* current_device_name = device_names;
            while (current_device_name && *current_device_name != 0)
            {
                auto fnd = std::find_if(
                    m_enumed_play_devices.begin(),
                    m_enumed_play_devices.end(),
                    [current_device_name](jeal_play_device& device)
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

                    jeecs::debug::loginfo("Found audio device: %s.", current_device_name);

                    if (device_instance == nullptr)
                    {
                        jeecs::debug::logerr("Failed to open device: '%s'.", current_device_name);
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

                            new_device.m_max_auxiliary_sends = std::min(
                                device_max_auxiliary_sends,
                                (ALCint)audio::MAX_AUXILIARY_SENDS);
                        }

                        new_device.m_device_instance = AL_JE_DEVICE(device_instance);
                        new_device.m_name = strdup(current_device_name);
                        new_device.m_active = false;
                        new_device.m_max_auxiliary_sends = (int)device_max_auxiliary_sends;
                    }
                }

                // 继续枚举下一个设备
                if (!enum_device_enabled)
                    break;

                current_device_name += strlen(current_device_name) + 1;
            }

            m_current_play_device.reset();
            update_enumed_play_devices(std::move(new_opened_devices));

            if (m_enumed_play_devices.empty())
                debug::logwarn("No play devices found.");
        }

        static ALenum je_al_format(jeal_format format, size_t* out_byte_per_sample)
        {
            switch (format)
            {
            case JE_AUDIO_FORMAT_MONO8:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 1;
                return AL_FORMAT_MONO8;
            case JE_AUDIO_FORMAT_MONO16:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 2;
                return AL_FORMAT_MONO16;
            case JE_AUDIO_FORMAT_MONO32F:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 4;
                return AL_FORMAT_MONO_FLOAT32;
            case JE_AUDIO_FORMAT_STEREO8:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 2;
                return AL_FORMAT_STEREO8;
            case JE_AUDIO_FORMAT_STEREO16:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 4;
                return AL_FORMAT_STEREO16;
            case JE_AUDIO_FORMAT_STEREO32F:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 8;
                return AL_FORMAT_STEREO_FLOAT32;
            default:
                if (out_byte_per_sample != nullptr) *out_byte_per_sample = 0;
                return AL_NONE;
            }
        }

        // Buffer
        void update_buffer_lockfree(jeal_buffer* buffer)
        {
            if (!m_current_play_device.has_value())
            {
                // 没有设备，实例亦不应当存在（已被转储）
                assert(buffer->m_buffer_instance == nullptr);
                return;
            }

            if (buffer->m_buffer_instance == nullptr)
            {
                buffer->m_buffer_instance = new jeal_native_buffer_instance;
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
        const jeal_buffer* instance_buffer(
            const void* new_allocated_buffer_data,
            size_t buffer_data_len,
            size_t sample_rate,
            jeal_format format)
        {
            size_t byte_per_sample;
            auto al_format = je_al_format(format, &byte_per_sample);
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
        jeal_native_buffer_instance* shutdown_buffer_native_instance(jeal_buffer* buffer_instance)
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
        void shutdown_buffer(jeal_buffer* buffer)
        {
            assert(buffer != nullptr);

            jeal_buffer* rw_buffer = const_cast<jeal_buffer*>(buffer);
            auto* native_instance = shutdown_buffer_native_instance(rw_buffer);
            if (native_instance == nullptr && m_play_state_dump.has_value())
            {
                // ATTENTION: 没有 native instance，说明此时可能是在转储期间销毁，
                //  需要从转储中删除此buffer

                // NOTE: 实例可能不存在于转储中，因为此实例可能是在转储之后创建的
                auto& dump = m_play_state_dump.value().m_buffers_dump;
                auto fnd = dump.find(rw_buffer);
                if (fnd != dump.end())
                {
                    delete fnd->second;
                    dump.erase(fnd);
                }
            }
            else
                delete native_instance;

            free(const_cast<void*>(buffer->m_data));

            delete rw_buffer;
        }

        // Source
        void update_source_lockfree(jeal_source* source)
        {
            if (!m_current_play_device.has_value())
            {
                // 此时应该没有任何可用设备
                assert(source->m_source_instance == nullptr);
                return;
            }

            if (source->m_source_instance == nullptr)
            {
                source->m_source_instance = new jeal_native_source_instance;
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

                    if (dump.m_play_state != jeal_state::JE_AUDIO_STATE_STOPPED)
                    {
                        // 恢复播放状态
                        alSourcei(
                            source->m_source_instance->m_source_id,
                            AL_BYTE_OFFSET,
                            dump.m_play_process);

                        if (dump.m_play_state == jeal_state::JE_AUDIO_STATE_PAUSED)
                            alSourcePause(source->m_source_instance->m_source_id);
                        else
                        {
                            assert(dump.m_play_state == jeal_state::JE_AUDIO_STATE_PLAYING);
                            alSourcePlay(source->m_source_instance->m_source_id);
                        }
                    }
                }

                const size_t max_auxiliary_sends = m_current_play_device.value()->m_max_auxiliary_sends;
                for (size_t i = 0; i < max_auxiliary_sends; i += 1)
                {
                    auto& binded_effect_slot = source->m_source_instance->m_playing_effect_slot[i];

                    ALint slot_id = AL_EFFECTSLOT_NULL;
                    if (binded_effect_slot.has_value())
                        slot_id = binded_effect_slot.value()->m_effect_slot_instance->m_effect_slot_id;

                    alSource3i(source->m_source_instance->m_source_id,
                        AL_AUXILIARY_SEND_FILTER,
                        slot_id,
                        (ALint)i,
                        AL_FILTER_NULL);
                }

                source->m_source_instance->m_dump.reset();
            }
        }
        jeal_native_source_instance* shutdown_source_native_instance(jeal_source* source_instance)
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
        void shutdown_source(jeal_source* source)
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

        jeal_native_source_instance* require_dump_for_no_device_context_source(
            jeal_source* source)
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

            jeal_native_source_instance* new_dump_for_no_device_context_source =
                new jeal_native_source_instance;

            auto& source_dump = new_dump_for_no_device_context_source->m_dump.emplace();
            assert(!new_dump_for_no_device_context_source->m_playing_buffer.has_value());
            source_dump.m_play_process = 0;
            source_dump.m_play_state = jeal_state::JE_AUDIO_STATE_STOPPED;

            // NOTE: 只有完全没有设备的情况下才需要 require_dump_for_no_device_context_source，此时
            //      源 ID 已经可有可无，此处不需要设置
            new_dump_for_no_device_context_source->m_source_id = 0;

            dump.m_sources_dump.insert(
                std::make_pair(
                    source, new_dump_for_no_device_context_source));

            return new_dump_for_no_device_context_source;
        }

        jeal_native_effect_slot_instance* require_dump_for_no_device_context_effect_slot(
            jeal_effect_slot* slot)
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

            jeal_native_effect_slot_instance* new_dump_for_no_device_context_effect_slot =
                new jeal_native_effect_slot_instance;

            auto& effect_slot_dump = new_dump_for_no_device_context_effect_slot->m_dump.emplace();
            (void)effect_slot_dump;

            // NOTE: 只有完全没有设备的情况下才需要 require_dump_for_no_device_context_effect_slot，此时
            //      源 ID 已经可有可无，此处不需要设置
            new_dump_for_no_device_context_effect_slot->m_effect_slot_id = 0;

            dump.m_effect_slots_dump.insert(
                std::make_pair(
                    slot, new_dump_for_no_device_context_effect_slot));

            return new_dump_for_no_device_context_effect_slot;
        }

        void close_buffer_lockfree(const jeal_buffer* buffer)
        {
            auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

            m_buffers.erase(rw_buffer);
            shutdown_buffer(rw_buffer);
        }

        void update_effect_lockfree(jeal_effect_head* effect)
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
                effect->m_effect_instance = new jeal_native_effect_instance;
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
                    const jeal_effect_reverb* instance_data = JE_EFFECT_DATA(jeal_effect_reverb, effect);

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
                    const jeal_effect_chorus* instance_data = JE_EFFECT_DATA(jeal_effect_chorus, effect);

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
                    const jeal_effect_distortion* instance_data = JE_EFFECT_DATA(jeal_effect_distortion, effect);

                    efx->alEffectf(effect_id, AL_DISTORTION_EDGE, instance_data->m_edge);
                    efx->alEffectf(effect_id, AL_DISTORTION_GAIN, instance_data->m_gain);
                    efx->alEffectf(effect_id, AL_DISTORTION_LOWPASS_CUTOFF, instance_data->m_lowpass_cutoff);
                    efx->alEffectf(effect_id, AL_DISTORTION_EQCENTER, instance_data->m_equalizer_center_freq);
                    efx->alEffectf(effect_id, AL_DISTORTION_EQBANDWIDTH, instance_data->m_equalizer_bandwidth);

                    break;
                }
                case AL_EFFECT_ECHO:
                {
                    const jeal_effect_echo* instance_data = JE_EFFECT_DATA(jeal_effect_echo, effect);

                    efx->alEffectf(effect_id, AL_ECHO_DELAY, instance_data->m_delay);
                    efx->alEffectf(effect_id, AL_ECHO_LRDELAY, instance_data->m_lr_delay);
                    efx->alEffectf(effect_id, AL_ECHO_DAMPING, instance_data->m_damping);
                    efx->alEffectf(effect_id, AL_ECHO_FEEDBACK, instance_data->m_feedback);
                    efx->alEffectf(effect_id, AL_ECHO_SPREAD, instance_data->m_spread);

                    break;
                }
                case AL_EFFECT_FLANGER:
                {
                    const jeal_effect_flanger* instance_data = JE_EFFECT_DATA(jeal_effect_flanger, effect);

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
                    const jeal_effect_frequency_shifter* instance_data = JE_EFFECT_DATA(jeal_effect_frequency_shifter, effect);

                    efx->alEffectf(effect_id, AL_FREQUENCY_SHIFTER_FREQUENCY, instance_data->m_frequency);
                    efx->alEffecti(effect_id, AL_FREQUENCY_SHIFTER_LEFT_DIRECTION, (ALint)instance_data->m_left_direction);
                    efx->alEffecti(effect_id, AL_FREQUENCY_SHIFTER_RIGHT_DIRECTION, (ALint)instance_data->m_right_direction);

                    break;
                }
                case AL_EFFECT_VOCAL_MORPHER:
                {
                    const jeal_effect_vocal_morpher* instance_data = JE_EFFECT_DATA(jeal_effect_vocal_morpher, effect);

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
                    const jeal_effect_pitch_shifter* instance_data = JE_EFFECT_DATA(jeal_effect_pitch_shifter, effect);

                    efx->alEffecti(effect_id, AL_PITCH_SHIFTER_COARSE_TUNE, instance_data->m_coarse_tune);
                    efx->alEffecti(effect_id, AL_PITCH_SHIFTER_FINE_TUNE, instance_data->m_fine_tune);

                    break;
                }
                case AL_EFFECT_RING_MODULATOR:
                {
                    const jeal_effect_ring_modulator* instance_data = JE_EFFECT_DATA(jeal_effect_ring_modulator, effect);

                    efx->alEffectf(effect_id, AL_RING_MODULATOR_FREQUENCY, instance_data->m_frequency);
                    efx->alEffectf(effect_id, AL_RING_MODULATOR_HIGHPASS_CUTOFF, instance_data->m_highpass_cutoff);
                    efx->alEffecti(effect_id, AL_RING_MODULATOR_WAVEFORM, (ALint)instance_data->m_waveform);

                    break;
                }
                case AL_EFFECT_AUTOWAH:
                {
                    const jeal_effect_autowah* instance_data = JE_EFFECT_DATA(jeal_effect_autowah, effect);

                    efx->alEffectf(effect_id, AL_AUTOWAH_ATTACK_TIME, instance_data->m_attack_time);
                    efx->alEffectf(effect_id, AL_AUTOWAH_RELEASE_TIME, instance_data->m_release_time);
                    efx->alEffectf(effect_id, AL_AUTOWAH_RESONANCE, instance_data->m_resonance);
                    efx->alEffectf(effect_id, AL_AUTOWAH_PEAK_GAIN, instance_data->m_peak_gain);

                    break;
                }
                case AL_EFFECT_COMPRESSOR:
                {
                    const jeal_effect_compressor* instance_data = JE_EFFECT_DATA(jeal_effect_compressor, effect);

                    efx->alEffecti(effect_id, AL_COMPRESSOR_ONOFF, instance_data->m_enabled ? AL_TRUE : AL_FALSE);

                    break;
                }
                case AL_EFFECT_EQUALIZER:
                {
                    const jeal_effect_equalizer* instance_data = JE_EFFECT_DATA(jeal_effect_equalizer, effect);

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
                    const jeal_effect_eaxreverb* instance_data = JE_EFFECT_DATA(jeal_effect_eaxreverb, effect);

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
                    jeecs::debug::logfatal("Unknown audio effect: %d when trying to update.",
                        (int)effect->m_effect_kind);
                }
            }

            if (effect->m_effect_instance->m_dump.has_value())
            {
                // Nothing todo.

                effect->m_effect_instance->m_dump.reset();
            }
        }
        jeal_native_effect_instance* shutdown_effect_native_instance(jeal_effect_head* effect)
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
        void shutdown_effect(jeal_effect_head* effect)
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
        void close_effect_lockfree(jeal_effect_head* effect)
        {
            m_effects.erase(effect);
            shutdown_effect(effect);
        }

        void update_effect_slot_lockfree(jeal_effect_slot* slot)
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
                slot->m_effect_slot_instance = new jeal_native_effect_slot_instance;
                if (efx != nullptr)
                    efx->alGenAuxiliaryEffectSlots(1, &slot->m_effect_slot_instance->m_effect_slot_id);
            }

            ALuint slot_id = slot->m_effect_slot_instance->m_effect_slot_id;

            if (efx != nullptr)
            {
                if (slot->m_effect_slot_instance->m_binding_effect.has_value())
                {
                    jeal_effect_head* effect = slot->m_effect_slot_instance->m_binding_effect.value();
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
        jeal_native_effect_slot_instance* shutdown_effect_slot_native_instance(jeal_effect_slot* slot)
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
        void shutdown_effect_slot(jeal_effect_slot* slot)
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
        void close_effect_slot_lockfree(jeal_effect_slot* slot)
        {
            m_effect_slots.erase(slot);
            shutdown_effect_slot(slot);
        }
    public:
        jeal_effect_slot* create_effect_slot()
        {
            jeal_effect_slot* slot = new jeal_effect_slot;

            slot->m_effect_slot_instance = nullptr;
            slot->m_references = 1;
            slot->m_gain = 1.0f;

            std::lock_guard g4(m_effect_slots_mx); // g4
            std::shared_lock g0(m_context_mx); // g0

            m_effect_slots.emplace(slot);
            update_effect_slot_lockfree(slot);

            return slot;
        }
        void update_effect_slot(jeal_effect_slot* slot)
        {
            std::lock_guard g4(m_effect_slots_mx); // g4
            std::shared_lock g0(m_context_mx); // g0

            update_effect_slot_lockfree(slot);
        }
        void close_effect_slot(jeal_effect_slot* slot)
        {
            std::lock_guard g4(m_effect_slots_mx); // g4
            std::shared_lock g0(m_context_mx); // g0

            AudioContextHelpler::decrease_ref(slot);
        }
        void set_slot_effect(jeal_effect_slot* slot, jeal_effect_head* effect_may_null)
        {
            std::lock_guard g4(m_effect_slots_mx); // g4
            std::lock_guard g3(m_effects_mx); // g3

            for (;;)
            {
                if (slot->m_effect_slot_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (slot->m_effect_slot_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    assert(effect_may_null == nullptr
                        || effect_may_null->m_effect_instance != nullptr);

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
                    std::lock_guard g0(m_context_mx); // g0
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

        template<typename T>
        T* create_effect()
        {
            void* buf = malloc(sizeof(jeal_effect_head) + sizeof(T));

            jeal_effect_head* head = reinterpret_cast<jeal_effect_head*>(buf);
            T* effect = JE_EFFECT_DATA(T, buf);

            head->m_effect_instance = nullptr;
            head->m_references = 1;

            AudioContextHelpler::effect_init_default(head, effect);

            std::lock_guard g3(m_effects_mx); // g3
            std::shared_lock g0(m_context_mx); // g0

            m_effects.emplace(head);
            update_effect_lockfree(head);

            return effect;
        }
        void update_effect(void* effect_data)
        {
            std::lock_guard g3(m_effects_mx); // g3
            std::shared_lock g0(m_context_mx); // g0

            update_effect_lockfree(JE_EFFECT_HEAD(effect_data));
        }

        void close_effect(void* effect_data)
        {
            std::lock_guard g3(m_effects_mx); // g3
            std::shared_lock g0(m_context_mx); // g0

            AudioContextHelpler::decrease_ref(JE_EFFECT_HEAD(effect_data));
        }

        const jeal_buffer* create_buffer(
            const void* data,
            size_t buffer_data_len,
            size_t sample_rate,
            jeal_format format)
        {
            void* buffer_data = malloc(buffer_data_len);
            assert(buffer_data != nullptr);

            memcpy(buffer_data, data, buffer_data_len);

            std::lock_guard g2(m_buffers_mx); // g2
            std::shared_lock g0(m_context_mx); // g0
            return instance_buffer(
                buffer_data,
                buffer_data_len,
                sample_rate,
                format);
        }
        const jeal_buffer* load_buffer_wav(const char* path)
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

            jeecs_file* wav_file = jeecs_file_open(path);
            if (wav_file == nullptr)
            {
                jeecs::debug::logerr("Failed to open file: '%s', unable to create audio buffer.", path);
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
                jeecs::debug::logerr("Invalid wav file: '%s', bad format tag.", path);
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
                jeecs::debug::logerr("Invalid wav file: '%s', bad format head.", path);
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
                jeecs::debug::logerr("Invalid wav file: '%s', bad data head.", path);
                jeecs_file_close(wav_file);
                return nullptr;
            }

            //Allocate memory for data
            void* data = malloc(wave_data.subChunk2Size);

            // Read in the sound data into the soundData variable
            if (!jeecs_file_read(data, wave_data.subChunk2Size, 1, wav_file))
            {
                jeecs::debug::logerr("Invalid wav file: '%s', bad data size.", path);
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
                jeecs::debug::logerr("Invalid wav file: '%s', bad data format.", path);
                free(data);
                return nullptr;
            }

            jeal_format wav_format = JE_AUDIO_FORMAT_MONO8;

            //The format is worked out by looking at the number of
            //channels and the bits per sample.
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
                jeecs::debug::logerr("Invalid wav file: '%s', bad data format.", path);
                free(data);
                return nullptr;
            }

            std::lock_guard g2(m_buffers_mx); // g2
            std::shared_lock g0(m_context_mx); // g0
            return instance_buffer(
                data,
                wave_data.subChunk2Size,
                wave_format.sampleRate,
                wav_format);
        }
        void close_buffer(const jeal_buffer* buffer)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::shared_lock g0(m_context_mx); // g0

            AudioContextHelpler::decrease_ref(buffer);
        }

        jeal_source* create_source()
        {
            jeal_source* source = new jeal_source;

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

            std::lock_guard g1(m_sources_mx); // g1
            std::shared_lock g0(m_context_mx); // g0

            m_sources.emplace(source);
            update_source_lockfree(source);

            return source;
        }
        void close_source(jeal_source* source)
        {
            // NOTE: 此处锁定 g2-g4 是因为源在关闭时，需要修改/关闭引用计数
            std::lock_guard g4(m_effect_slots_mx); // g4
            std::lock_guard g3(m_effects_mx); // g3
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1
            std::shared_lock g0(m_context_mx); // g0

            // Remove from the set.
            m_sources.erase(source);
            shutdown_source(source);
        }
        void update_source(jeal_source* source)
        {
            // NOTE: 尽管update_source 可能读取buffer的状态，但buffer没有内部可变性，
            //      并且纯粹的update操作不涉及引用计数的变更和实例的创建/销毁，因此
            //      这里不需要加锁g2
            std::lock_guard g1(m_sources_mx); // g1
            std::shared_lock g0(m_context_mx); // g0

            update_source_lockfree(source);
        }
        void set_source_effect_slot(jeal_source* source, jeal_effect_slot* slot_may_null, size_t slot_idx)
        {
            assert(source != nullptr);
            assert(slot_idx < audio::MAX_AUXILIARY_SENDS);

            std::lock_guard g4(m_effect_slots_mx); // g4
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    assert(slot_may_null->m_effect_slot_instance != nullptr);

                    source->m_source_instance->
                        m_playing_effect_slot[slot_idx].set_res_may_null(slot_may_null);

                    if (m_alext_efx.has_value())
                    {
                        ALint slot_id = AL_EFFECTSLOT_NULL;
                        if (slot_may_null != nullptr)
                            slot_id = slot_may_null->m_effect_slot_instance->m_effect_slot_id;

                        alSource3i(source->m_source_instance->m_source_id,
                            AL_AUXILIARY_SEND_FILTER,
                            slot_id,
                            slot_idx,
                            AL_FILTER_NULL);
                    }
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    dump_instance->
                        m_playing_effect_slot[slot_idx].set_res_may_null(slot_may_null);
                }

                break;
            }
        }
        void set_source_buffer(jeal_source* source, const jeal_buffer* buffer)
        {
            assert(source != nullptr);
            assert(buffer != nullptr);

            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    assert(buffer->m_buffer_instance != nullptr);

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
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    // 此时没有设备，但状态需要装填到转储中
                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    auto& dump = dump_instance->m_dump.value();

                    dump_instance->m_playing_buffer.set_res(buffer);
                    dump.m_play_state = jeal_state::JE_AUDIO_STATE_STOPPED;
                    dump.m_play_process = 0;
                }

                break;
            }
        }
        void play_source(jeal_source* source)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    if (source->m_source_instance->m_playing_buffer.has_value())
                        alSourcePlay(source->m_source_instance->m_source_id);
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    // 此时没有设备，但状态需要装填到转储中
                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    auto& dump = dump_instance->m_dump.value();

                    if (dump_instance->m_playing_buffer.has_value())
                    {
                        if (dump.m_play_state == jeal_state::JE_AUDIO_STATE_STOPPED)
                            dump.m_play_process = 0;

                        dump.m_play_state = jeal_state::JE_AUDIO_STATE_PLAYING;
                    }
                }
                break;
            }
        }
        void pause_source(jeal_source* source)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    if (source->m_source_instance->m_playing_buffer.has_value())
                        alSourcePause(source->m_source_instance->m_source_id);
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    // 此时没有设备，但状态需要装填到转储中
                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    auto& dump = dump_instance->m_dump.value();

                    if (dump_instance->m_playing_buffer.has_value())
                    {
                        if (dump.m_play_state == jeal_state::JE_AUDIO_STATE_PLAYING)
                            dump.m_play_state = jeal_state::JE_AUDIO_STATE_PAUSED;
                    }
                }
                break;
            }
        }
        void stop_source(jeal_source* source)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    if (source->m_source_instance->m_playing_buffer.has_value())
                        alSourceStop(source->m_source_instance->m_source_id);
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    // 此时没有设备，但状态需要装填到转储中
                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    auto& dump = dump_instance->m_dump.value();

                    if (dump_instance->m_playing_buffer.has_value())
                    {
                        if (dump.m_play_state == jeal_state::JE_AUDIO_STATE_PLAYING)
                            dump.m_play_state = jeal_state::JE_AUDIO_STATE_STOPPED;
                    }
                }
                break;
            }
        }
        jeal_state get_source_play_state(jeal_source* source)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
                    if (source->m_source_instance == nullptr)
                        // 锁上上下文锁后发现，此时实例已经被转储，重新执行检查
                        continue;

                    if (!source->m_source_instance->m_playing_buffer.has_value())
                        return  jeal_state::JE_AUDIO_STATE_STOPPED;

                    ALenum play_state;
                    alGetSourcei(
                        source->m_source_instance->m_source_id,
                        AL_SOURCE_STATE,
                        &play_state);

                    switch (play_state)
                    {
                    case AL_PLAYING:
                        return jeal_state::JE_AUDIO_STATE_PLAYING;
                    case AL_PAUSED:
                        return jeal_state::JE_AUDIO_STATE_PAUSED;
                    default:
                        assert(play_state == AL_STOPPED || play_state == AL_INITIAL);
                        return jeal_state::JE_AUDIO_STATE_STOPPED;
                    }
                }
                else
                {
                    std::lock_guard g0(m_context_mx); // g0
                    if (source->m_source_instance != nullptr)
                        // 锁上上下文锁后发现，此时实例转储已经被恢复，重新执行检查
                        continue;

                    // 此时没有设备，但状态需要装填到转储中
                    auto* dump_instance = require_dump_for_no_device_context_source(source);
                    auto& dump = dump_instance->m_dump.value();

                    if (dump_instance->m_playing_buffer.has_value())
                        return dump.m_play_state;

                    return jeal_state::JE_AUDIO_STATE_STOPPED;
                }
                break;
            }
        }
        size_t get_source_play_process(jeal_source* source)
        {
            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
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
                    std::lock_guard g0(m_context_mx); // g0
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
        void set_source_play_process(jeal_source* source, size_t offset)
        {
            auto align_offset =
                [](const jeal_buffer* buffer, size_t offset)
                {
                    if (offset % buffer->m_sample_size != 0)
                    {
                        jeecs::debug::logwarn("Audio buffer offset is not aligned.");
                        offset = offset / buffer->m_sample_size * buffer->m_sample_size;
                    }
                    return (ALint)std::min(offset, buffer->m_size);
                };

            std::lock_guard g2(m_buffers_mx); // g2
            std::lock_guard g1(m_sources_mx); // g1

            for (;;)
            {
                if (source->m_source_instance != nullptr)
                {
                    std::shared_lock g0(m_context_mx); // g0
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
                    std::lock_guard g0(m_context_mx); // g0
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
            std::shared_lock g0(m_context_mx); // g0

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
        jeal_listener* get_listener()
        {
            return &m_listener;
        }

        const std::vector<jeal_play_device>& refetch_devices()
        {
            std::lock_guard g0(m_context_mx); // g0

            refetch_and_update_enumed_play_devices();
            return m_enumed_play_devices;
        }
        void using_specify_device(const jeal_play_device* device)
        {
            std::lock_guard g0(m_context_mx); // g0

            if (m_current_play_device.has_value())
                deactive_using_device(m_current_play_device.value());

            active_using_device(const_cast<jeal_play_device*>(device));
        }
        bool check_device_connected(const jeal_play_device* device)
        {
            std::lock_guard g0(m_context_mx); // g0

            if (device->m_device_instance != nullptr)
                return check_device_instance_connected(device->m_device_instance);

            return false;
        }

        AudioContext()
            : m_current_play_device(std::nullopt)
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
        }
        ~AudioContext()
        {
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

    void AudioContextHelpler::increase_ref(const jeal_buffer* buffer)
    {
        auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

        ++rw_buffer->m_references;
    }
    void AudioContextHelpler::decrease_ref(const jeal_buffer* buffer)
    {
        auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

        if (0 == --rw_buffer->m_references)
        {
            // Remove from the set.
            g_engine_audio_context->close_buffer_lockfree(rw_buffer);
        }
    }

    void AudioContextHelpler::increase_ref(jeal_effect_head* effect)
    {
        ++effect->m_references;
    }
    void AudioContextHelpler::decrease_ref(jeal_effect_head* effect)
    {
        if (0 == --effect->m_references)
        {
            // Remove from the set.
            g_engine_audio_context->close_effect_lockfree(effect);
        }
    }

    void AudioContextHelpler::increase_ref(jeal_effect_slot* slot)
    {
        ++slot->m_references;
    }
    void AudioContextHelpler::decrease_ref(jeal_effect_slot* slot)
    {
        if (0 == --slot->m_references)
        {
            // Remove from the set.
            g_engine_audio_context->close_effect_slot_lockfree(slot);
        }
    }

    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_reverb* reverb)
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
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_chorus* chorus)
    {
        head->m_effect_kind = AL_EFFECT_CHORUS;

        chorus->m_waveform = (jeal_effect_chorus::waveform)AL_CHORUS_DEFAULT_WAVEFORM;
        chorus->m_phase = AL_CHORUS_DEFAULT_PHASE;
        chorus->m_rate = AL_CHORUS_DEFAULT_RATE;
        chorus->m_depth = AL_CHORUS_DEFAULT_DEPTH;
        chorus->m_feedback = AL_CHORUS_DEFAULT_FEEDBACK;
        chorus->m_delay = AL_CHORUS_DEFAULT_DELAY;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_distortion* distortion)
    {
        head->m_effect_kind = AL_EFFECT_DISTORTION;

        distortion->m_edge = AL_DISTORTION_DEFAULT_EDGE;
        distortion->m_gain = AL_DISTORTION_DEFAULT_GAIN;
        distortion->m_lowpass_cutoff = AL_DISTORTION_DEFAULT_LOWPASS_CUTOFF;
        distortion->m_equalizer_center_freq = AL_DISTORTION_DEFAULT_EQCENTER;
        distortion->m_equalizer_bandwidth = AL_DISTORTION_DEFAULT_EQBANDWIDTH;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_echo* echo)
    {
        head->m_effect_kind = AL_EFFECT_ECHO;

        echo->m_delay = AL_ECHO_DEFAULT_DELAY;
        echo->m_lr_delay = AL_ECHO_DEFAULT_LRDELAY;
        echo->m_damping = AL_ECHO_DEFAULT_DAMPING;
        echo->m_feedback = AL_ECHO_DEFAULT_FEEDBACK;
        echo->m_spread = AL_ECHO_DEFAULT_SPREAD;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_flanger* flanger)
    {
        head->m_effect_kind = AL_EFFECT_FLANGER;

        flanger->m_waveform = (jeal_effect_flanger::waveform)AL_FLANGER_DEFAULT_WAVEFORM;
        flanger->m_phase = AL_FLANGER_DEFAULT_PHASE;
        flanger->m_rate = AL_FLANGER_DEFAULT_RATE;
        flanger->m_depth = AL_FLANGER_DEFAULT_DEPTH;
        flanger->m_feedback = AL_FLANGER_DEFAULT_FEEDBACK;
        flanger->m_delay = AL_FLANGER_DEFAULT_DELAY;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_frequency_shifter* shifter)
    {
        head->m_effect_kind = AL_EFFECT_FREQUENCY_SHIFTER;

        shifter->m_frequency = AL_FREQUENCY_SHIFTER_DEFAULT_FREQUENCY;
        shifter->m_left_direction =
            (jeal_effect_frequency_shifter::direction)AL_FREQUENCY_SHIFTER_DEFAULT_LEFT_DIRECTION;
        shifter->m_right_direction =
            (jeal_effect_frequency_shifter::direction)AL_FREQUENCY_SHIFTER_DEFAULT_RIGHT_DIRECTION;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_vocal_morpher* morpher)
    {
        head->m_effect_kind = AL_EFFECT_VOCAL_MORPHER;

        morpher->m_phoneme_a = (jeal_effect_vocal_morpher::phoneme)AL_VOCAL_MORPHER_DEFAULT_PHONEMEA;
        morpher->m_phoneme_a_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEA_COARSE_TUNING;
        morpher->m_phoneme_b = (jeal_effect_vocal_morpher::phoneme)AL_VOCAL_MORPHER_DEFAULT_PHONEMEB;
        morpher->m_phoneme_b_coarse_tuning = AL_VOCAL_MORPHER_DEFAULT_PHONEMEB_COARSE_TUNING;
        morpher->m_waveform = (jeal_effect_vocal_morpher::waveform)AL_VOCAL_MORPHER_DEFAULT_WAVEFORM;
        morpher->m_rate = AL_VOCAL_MORPHER_DEFAULT_RATE;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_pitch_shifter* shifter)
    {
        head->m_effect_kind = AL_EFFECT_PITCH_SHIFTER;

        shifter->m_coarse_tune = AL_PITCH_SHIFTER_DEFAULT_COARSE_TUNE;
        shifter->m_fine_tune = AL_PITCH_SHIFTER_DEFAULT_FINE_TUNE;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_ring_modulator* modulator)
    {
        head->m_effect_kind = AL_EFFECT_RING_MODULATOR;

        modulator->m_frequency = AL_RING_MODULATOR_DEFAULT_FREQUENCY;
        modulator->m_highpass_cutoff = AL_RING_MODULATOR_DEFAULT_HIGHPASS_CUTOFF;
        modulator->m_waveform = (jeal_effect_ring_modulator::waveform)AL_RING_MODULATOR_DEFAULT_WAVEFORM;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_autowah* wah)
    {
        head->m_effect_kind = AL_EFFECT_AUTOWAH;

        wah->m_attack_time = AL_AUTOWAH_DEFAULT_ATTACK_TIME;
        wah->m_release_time = AL_AUTOWAH_DEFAULT_RELEASE_TIME;
        wah->m_resonance = AL_AUTOWAH_DEFAULT_RESONANCE;
        wah->m_peak_gain = AL_AUTOWAH_DEFAULT_PEAK_GAIN;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_compressor* compressor)
    {
        head->m_effect_kind = AL_EFFECT_COMPRESSOR;

        compressor->m_enabled = AL_COMPRESSOR_DEFAULT_ONOFF != AL_FALSE;
    }
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_equalizer* equalizer)
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
    void AudioContextHelpler::effect_init_default(jeal_effect_head* head, jeal_effect_eaxreverb* eaxreverb)
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

void jeal_init()
{
    assert(jeecs::g_engine_audio_context == nullptr);
    jeecs::g_engine_audio_context = new jeecs::AudioContext;
}
void jeal_finish()
{
    assert(jeecs::g_engine_audio_context != nullptr);
    delete jeecs::g_engine_audio_context;
    jeecs::g_engine_audio_context = nullptr;
}
const jeal_buffer* jeal_create_buffer(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    jeal_format format)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_buffer(
        data, buffer_data_len, sample_rate, format);
}
const jeal_buffer* jeal_load_buffer_wav(const char* path)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->load_buffer_wav(path);
}
void jeal_close_buffer(const jeal_buffer* buffer)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->close_buffer(buffer);
}
jeal_source* jeal_create_source()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_source();
}
void jeal_update_source(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->update_source(source);
}
void jeal_close_source(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->close_source(source);
}
void jeal_set_source_buffer(jeal_source* source, const jeal_buffer* buffer)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->set_source_buffer(source, buffer);
}
void jeal_set_source_effect_slot(
    jeal_source* source,
    jeal_effect_slot* slot_may_null,
    size_t slot_idx)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->set_source_effect_slot(source, slot_may_null, slot_idx);
}
void jeal_play_source(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->play_source(source);
}
void jeal_pause_source(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->pause_source(source);
}
void jeal_stop_source(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->stop_source(source);
}
jeal_state jeal_get_source_play_state(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->get_source_play_state(source);
}
size_t jeal_get_source_play_process(jeal_source* source)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->get_source_play_process(source);
}
void jeal_set_source_play_process(jeal_source* source, size_t offset)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->set_source_play_process(source, offset);
}
jeal_listener* jeal_get_listener()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->get_listener();
}
void jeal_update_listener()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->update_listener();
}
jeal_effect_slot* jeal_create_effect_slot()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect_slot();
}
void jeal_effect_slot_bind(jeal_effect_slot* slot, void* effect_may_null)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->set_slot_effect(
        slot, effect_may_null != nullptr ? JE_EFFECT_HEAD(effect_may_null) : nullptr);
}
void jeal_close_effect_slot(jeal_effect_slot* slot)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->close_effect_slot(slot);
}
void jeal_update_effect_slot(jeal_effect_slot* slot)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->update_effect_slot(slot);
}

jeal_effect_reverb* jeal_create_effect_reverb()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_reverb>();
}
jeal_effect_chorus* jeal_create_effect_chorus()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_chorus>();
}
jeal_effect_distortion* jeal_create_effect_distortion()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_distortion>();
}
jeal_effect_echo* jeal_create_effect_echo()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_echo>();
}
jeal_effect_flanger* jeal_create_effect_flanger()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_flanger>();
}
jeal_effect_frequency_shifter* jeal_create_effect_frequency_shifter()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_frequency_shifter>();
}
jeal_effect_vocal_morpher* jeal_create_effect_vocal_morpher()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_vocal_morpher>();
}
jeal_effect_pitch_shifter* jeal_create_effect_pitch_shifter()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_pitch_shifter>();
}
jeal_effect_ring_modulator* jeal_create_effect_ring_modulator()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_ring_modulator>();
}
jeal_effect_autowah* jeal_create_effect_autowah()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_autowah>();
}
jeal_effect_compressor* jeal_create_effect_compressor()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_compressor>();
}
jeal_effect_equalizer* jeal_create_effect_equalizer()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_equalizer>();
}
jeal_effect_eaxreverb* jeal_create_effect_eaxreverb()
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->create_effect<jeal_effect_eaxreverb>();
}
void jeal_close_effect(void* effect)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->close_effect(effect);
}
void jeal_update_effect(void* effect)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->update_effect(effect);
}

const jeal_play_device* jeal_refetch_devices(size_t* out_device_count)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    auto& devices = jeecs::g_engine_audio_context->refetch_devices();
    *out_device_count = devices.size();

    return devices.data();
}

void jeal_using_device(const jeal_play_device* device)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    jeecs::g_engine_audio_context->using_specify_device(device);
}

bool jeal_check_device_connected(const jeal_play_device* device)
{
    assert(jeecs::g_engine_audio_context != nullptr);

    return jeecs::g_engine_audio_context->check_device_connected(device);
}