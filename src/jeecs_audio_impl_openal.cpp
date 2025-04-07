#define JE_IMPL
#include "jeecs.hpp"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <unordered_set>
#include <vector>
#include <mutex>

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

struct jeal_source
{
    ALuint m_openal_source;

    // 当前源最后播放的 buffer，不过需要注意，
    // 如果source的状态处于stopped，那么这个地方的buffer就应该被视为无效的
    jeal_buffer* m_last_played_buffer;
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
    bool _jeal_efx_enabled;

    std::vector<jeal_device*> _jeal_all_devices;
    std::vector<jeal_capture_device*> _jeal_all_capture_devices;

    std::mutex _jeal_all_sources_mx;
    std::unordered_set<jeal_source*> _jeal_all_sources;

    std::mutex _jeal_all_buffers_mx;
    std::unordered_set<jeal_buffer*> _jeal_all_buffers;

    std::atomic<float> _jeal_listener_gain = 1.0f;
    std::atomic<float> _jeal_global_volume_gain = 1.0f;
};
_jeal_static_context _jeal_ctx;

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
void _jeal_update_source(jeal_source* source)
{
    alGenSources(1, &source->m_openal_source);
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
    float listener_orientation[6] = { 0.0f,0.0f,1.0f, 0.0f,1.0f,0.0f };

    std::vector<source_restoring_information> sources_information;
};

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
    _jeal_ctx._jeal_current_device = device;
    _jeal_ctx._jeal_efx_enabled = 
        alcIsExtensionPresent(device->m_openal_device, ALC_EXT_EFX_NAME) != 0;

    if (!_jeal_ctx._jeal_efx_enabled)
        jeecs::debug::logwarn(
            "Audio device: %s, does not support EFX.", 
            device->m_device_name);
    
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
void _jeal_restore_context(const _jeal_global_context* context)
{
    // OK, Restore buffer, listener and source.
    for (auto* buffer : _jeal_ctx._jeal_all_buffers)
        _jeal_update_buffer_instance(buffer);

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
    std::lock_guard g3(_jeal_ctx._jeal_context_mx);

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
    std::lock_guard g3(_jeal_ctx._jeal_context_mx);

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

    std::lock_guard g3(_jeal_ctx._jeal_context_mx);

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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    if (device->m_capturing_device == nullptr)
    {
        jeecs::debug::logerr("Capture device `%s` is not opened.", device->m_device_name);
        return;
    }
    alcCaptureStart(device->m_capturing_device);
}
void jeal_capture_device_stop(jeal_capture_device* device)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

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

    std::lock_guard g3(_jeal_ctx._jeal_context_mx);

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
    std::lock_guard g3(_jeal_ctx._jeal_context_mx);

    assert(_jeal_ctx._jeal_all_sources.empty());
    assert(_jeal_ctx._jeal_all_buffers.empty());

    _jeal_shutdown_current_device();
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
        std::lock_guard g3(_jeal_ctx._jeal_context_mx);

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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_sources.insert(audio_source);

    _jeal_update_source(audio_source);
    audio_source->m_last_played_buffer = nullptr;

    return audio_source;
}

void jeal_close_source(jeal_source* source)
{
    std::lock_guard g1(_jeal_ctx._jeal_all_sources_mx);
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);
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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

void jeal_close_buffer(jeal_buffer* buffer)
{
    std::lock_guard g1(_jeal_ctx._jeal_all_buffers_mx);
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);
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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    source->m_last_played_buffer = buffer;
    alSourcei(source->m_openal_source, AL_BUFFER, buffer->m_openal_buffer);
}

void jeal_source_loop(jeal_source* source, bool loop)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcei(source->m_openal_source, AL_LOOPING, loop ? 1 : 0);
}

void jeal_source_play(jeal_source* source)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcePlay(source->m_openal_source);
}

void jeal_source_pause(jeal_source* source)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcePause(source->m_openal_source);
}

void jeal_source_stop(jeal_source* source)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourceStop(source->m_openal_source);
}

void jeal_source_position(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSource3f(source->m_openal_source, AL_POSITION, x, y, z);
}
void jeal_source_velocity(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSource3f(source->m_openal_source, AL_VELOCITY, x, y, z);
}

size_t jeal_source_get_byte_offset(jeal_source* source)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    ALint byte_offset;
    alGetSourcei(source->m_openal_source, AL_BYTE_OFFSET, &byte_offset);
    return byte_offset;
}

void jeal_source_set_byte_offset(jeal_source* source, size_t byteoffset)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcei(source->m_openal_source, AL_BYTE_OFFSET, (ALint)byteoffset);
}

void jeal_source_pitch(jeal_source* source, float playspeed)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcef(source->m_openal_source, AL_PITCH, playspeed);
}

void jeal_source_volume(jeal_source* source, float volume)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alSourcef(source->m_openal_source, AL_GAIN, volume);
}

jeal_state jeal_source_get_state(jeal_source* source)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alListener3f(AL_POSITION, x, y, z);
}

void jeal_listener_velocity(float x, float y, float z)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    alListener3f(AL_VELOCITY, x, y, z);
}

void jeal_listener_direction(
    float face_x, float face_y, float face_z,
    float top_x, float top_y, float top_z)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    float orientation[] = {
       face_x, face_y, -face_z,
       top_x, top_y, -top_z,
    };

    alListenerfv(AL_ORIENTATION, orientation);
}

void jeal_listener_volume(float volume)
{
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

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
    std::shared_lock g3(_jeal_ctx._jeal_context_mx);

    _jeal_ctx._jeal_global_volume_gain.store(volume);

    float listener_gain;
    do
    {
        listener_gain = _jeal_ctx._jeal_listener_gain.load();
        alListenerf(AL_GAIN, _jeal_ctx._jeal_global_volume_gain.load() * listener_gain);
    } while (_jeal_ctx._jeal_listener_gain.load() != listener_gain);
}