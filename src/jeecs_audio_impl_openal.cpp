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

namespace jeecs
{
    class AudioContextHelpler
    {
    public:
        static void buffer_increase_ref(const jeal_buffer* buffer);
        static void buffer_decrease_ref(const jeal_buffer* buffer);

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

        class BufferHolder
        {
            JECS_DISABLE_MOVE_AND_COPY(BufferHolder);

            std::optional<const jeal_buffer*> m_buffer;
        public:
            BufferHolder() = default;
            ~BufferHolder()
            {
                if (m_buffer.has_value())
                    buffer_decrease_ref(m_buffer.value());
            }

            bool has_value() const
            {
                return m_buffer.has_value();
            }
            const jeal_buffer* value() const
            {
                return m_buffer.value();
            }

            void set_buffer(const jeal_buffer* buffer)
            {
                assert(buffer != nullptr);
                AudioContextHelpler::buffer_increase_ref(buffer);

                if (m_buffer.has_value())
                    buffer_decrease_ref(m_buffer.value());

                m_buffer = buffer;
            }
        };
    };
}

struct jeal_native_buffer_instance
{
    ALuint m_buffer_id;
};
struct jeal_native_source_instance
{
    ALuint m_source_id;
    jeecs::AudioContextHelpler::BufferHolder m_playing_buffer;

    // Dump data:
    struct dump_data_t
    {
        jeal_state m_play_state;
        ALint m_play_process;
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
        std::optional<jeal_play_device*> m_current_play_device = std::nullopt;

        std::mutex m_sources_mx;       // g1
        std::unordered_set<jeal_source*> m_sources;
        std::mutex m_buffers_mx;       // g2
        std::unordered_set<jeal_buffer*> m_buffers;
        std::mutex m_effects_mx;       // g3
        std::unordered_set<jeal_effect_head*> m_effects;

        jeal_listener m_listener;

        constexpr static ALCint M_ENGINE_ALC_MAX_AUXILIARY_SENDS =
            audio::MAX_AUXILIARY_SENDS;

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
        };
        std::optional<jeal_play_device_states_dump> m_play_state_dump = std::nullopt;

        static bool check_device_connected(jeal_native_play_device_instance* device)
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

            for (auto* buffer : m_buffers)
            {
                assert(buffer->m_buffer_instance != nullptr);

                // NOTE: Buffer 没有什么特别需要转储的状态，此处只保留 native instance 的实例
                states.m_buffers_dump[buffer] = shutdown_buffer_native_instance(buffer);
            }

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
        }
        void restore_playing_states()
        {
            assert(m_play_state_dump.has_value());
            assert(m_current_play_device.has_value());

            auto& states = m_play_state_dump.value();

            for (auto* buffer : m_buffers)
            {
                auto fnd = states.m_buffers_dump.find(buffer);
                if (fnd != states.m_buffers_dump.end())
                {
                    // 有之前留下的转储信息，更新之
                    assert(buffer->m_buffer_instance == nullptr);

                    buffer->m_buffer_instance = fnd->second;
                    fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                }
                // else
                    // ; // 这是一个新的 buffer 实例，在转储期间被创建出来的。

                update_buffer_lockfree(buffer);
            }

            for (auto* source : m_sources)
            {
                auto fnd = states.m_sources_dump.find(source);
                if (fnd != states.m_sources_dump.end())
                {
                    // 有之前留下的转储信息，更新之
                    assert(source->m_source_instance == nullptr);

                    source->m_source_instance = fnd->second;
                    fnd->second = nullptr; // 置空，后续需要清理无主的转储信息，避免错误释放
                }
                // else
                    // ; // 这是一个新的 source 实例，在转储期间被创建出来的。

                update_source_lockfree(source);
            }

            for (auto& [_, buffer_instance] : states.m_buffers_dump)
            {
                // 释放无主的 buffer 转储实例
                if (buffer_instance != nullptr)
                    delete buffer_instance;
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
            try_restore_playing_states();
            device->m_active = true;
            m_current_play_device.emplace(device);
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
                        return strcmp(device.m_name, current_device_name) == 0;
                    });

                if (fnd != m_enumed_play_devices.end())
                {
                    // 同名设备已经存在于之前枚举的列表中，说明之前这个设备已经连接
                    // 考虑以下情况：
                    //   1) 之前链接的设备实例已经断开，这次枚举到的是新的同名设备

                    // 检查旧设备是否仍然链接，如果是，则此次枚举到的就是旧设备
                    auto& old_connected_device = *fnd;
                    if (check_device_connected(old_connected_device.m_device_instance))
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
                                M_ENGINE_ALC_MAX_AUXILIARY_SENDS);
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

            update_enumed_play_devices(std::move(new_opened_devices));

            if (m_enumed_play_devices.empty())
                debug::logwarn("No play devices found.");
        }

        ALenum je_al_format(jeal_format format, size_t* out_byte_per_sample)
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
                buffer->m_buffer_instance = new jeal_native_buffer_instance;

            alGenBuffers(1, &buffer->m_buffer_instance->m_buffer_id);
            alBufferData(
                buffer->m_buffer_instance->m_buffer_id,
                je_al_format(buffer->m_format, nullptr),
                buffer->m_data,
                buffer->m_size,
                buffer->m_sample_rate);
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
            assert(buffer->m_buffer_instance != nullptr);

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
                assert(!source->m_source_instance->m_playing_buffer.has_value());
                assert(!source->m_source_instance->m_dump.has_value());
            }

            alGenSources(1, &source->m_source_instance->m_source_id);
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
            assert(source->m_source_instance != nullptr);

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
        void close_buffer_lockfree(const jeal_buffer* buffer)
        {
            auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

            assert(0 == rw_buffer->m_buffer_instance);
            // Remove from the set.

            m_buffers.erase(rw_buffer);
            shutdown_buffer(rw_buffer);
        }

    public:
        template<typename T>
        T* create_effect()
        {
            intptr_t buf = (intptr_t)malloc(sizeof(jeal_effect_head) + sizeof(T));

            jeal_effect_head* head = reinterpret_cast<jeal_effect_head*>(buf);
            T* effect = reinterpret_cast<T*>(buf + sizeof(jeal_effect_head));

            head->m_effect_instance = nullptr;
            head->m_references = 1;

            AudioContextHelpler::effect_init_default(head, effect);

            return effect;
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

            AudioContextHelpler::buffer_decrease_ref(buffer);
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
            // NOTE: 此处锁定 g2 是因为源在关闭时，需要修改/关闭buffer的引用计数
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

                    source->m_source_instance->m_playing_buffer.set_buffer(buffer);

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

                    dump_instance->m_playing_buffer.set_buffer(buffer);
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
                    m_listener.m_orientation[0][0],
                    m_listener.m_orientation[0][1],
                    m_listener.m_orientation[0][2],
                    m_listener.m_orientation[1][0],
                    m_listener.m_orientation[1][1],
                    m_listener.m_orientation[1][2],
                };
                alListenerfv(AL_ORIENTATION, orientation);
            }
        }

        jeal_listener* get_listener()
        {
            return &m_listener;
        }

        AudioContext()
        {
            m_listener.m_gain = 1.0f;
            m_listener.m_global_gain = 1.0f;
            m_listener.m_location[0] = 0.0f;
            m_listener.m_location[1] = 0.0f;
            m_listener.m_location[2] = 0.0f;
            m_listener.m_velocity[0] = 0.0f;
            m_listener.m_velocity[1] = 0.0f;
            m_listener.m_velocity[2] = 0.0f;

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
        }
    };

    static AudioContext* g_engine_audio_context = nullptr;

    void AudioContextHelpler::buffer_increase_ref(const jeal_buffer* buffer)
    {
        auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

        ++rw_buffer->m_references;
    }
    void AudioContextHelpler::buffer_decrease_ref(const jeal_buffer* buffer)
    {
        auto* rw_buffer = const_cast<jeal_buffer*>(buffer);

        if (0 == --rw_buffer->m_references)
        {
            // Remove from the set.
            g_engine_audio_context->close_buffer_lockfree(rw_buffer);
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

        chorus->m_waveform = (jeal_effect_chorus::wavefrom)AL_CHORUS_DEFAULT_WAVEFORM;
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

        flanger->m_waveform = (jeal_effect_flanger::wavefrom)AL_FLANGER_DEFAULT_WAVEFORM;
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
        morpher->m_waveform = (jeal_effect_vocal_morpher::wavefrom)AL_VOCAL_MORPHER_DEFAULT_WAVEFORM;
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
        modulator->m_waveform = (jeal_effect_ring_modulator::wavefrom)AL_RING_MODULATOR_DEFAULT_WAVEFORM;
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
        eaxreverb->m_decay_hf_limiter = AL_EAXREVERB_DEFAULT_DECAY_HFLIMIT != AL_FALSE;
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
