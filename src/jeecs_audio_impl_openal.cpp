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
    ALsizei m_frequency;
    ALsizei m_byterate;
    ALenum m_format;
};

std::shared_mutex _jeal_context_mx;

std::mutex _jeal_all_devices_mx;
jeal_device* _jeal_current_device = nullptr;
std::vector<jeal_device*> _jeal_all_devices;

std::mutex _jeal_all_sources_mx;
std::unordered_set<jeal_source*> _jeal_all_sources;

std::mutex _jeal_all_buffers_mx;
std::unordered_set<jeal_buffer*> _jeal_all_buffers;

std::atomic<float> _jeal_listener_gain = 1.0f;
std::atomic<float> _jeal_global_volume_gain = 1.0f;

void _jeal_update_buffer_instance(jeal_buffer* buffer)
{
    alGenBuffers(1, &buffer->m_openal_buffer);

    //now we put our data into the openAL buffer and
    //check for success
    alBufferData(buffer->m_openal_buffer,
        buffer->m_format,
        buffer->m_data,
        buffer->m_size,
        buffer->m_frequency);
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
    _jeal_current_device = nullptr;
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        for (auto* src : _jeal_all_sources)
            alDeleteSources(1, &src->m_openal_source);

        for (auto* buf : _jeal_all_buffers)
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
    _jeal_current_device = device;
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
        for (auto* source : _jeal_all_sources)
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
                src_info.m_state = jeal_state::PLAYING; break;
            case AL_PAUSED:
                src_info.m_state = jeal_state::PAUSED; break;
            default:
                assert(state == AL_INITIAL || state == AL_STOPPED);
                src_info.m_state = jeal_state::STOPPED; break;
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
    for (auto* buffer : _jeal_all_buffers)
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

        if (src_info.m_state != jeal_state::STOPPED)
        {
            assert(src_info.m_playing_buffer != nullptr);

            alSourcei(src_info.m_source->m_openal_source, AL_BUFFER, src_info.m_playing_buffer->m_openal_buffer);
            alSourcei(src_info.m_source->m_openal_source, AL_BYTE_OFFSET, (ALint)src_info.m_offset);

            if (src_info.m_state == jeal_state::PAUSED)
                alSourcePause(src_info.m_source->m_openal_source);
            else
                alSourcePlay(src_info.m_source->m_openal_source);
        }
    }
}
jeal_device** _jeal_update_refetch_devices(size_t* out_len)
{
    auto old_devices = _jeal_all_devices;
    _jeal_all_devices.clear();

    // 如果支持枚举所有设备，就把枚举所有设备并全部打开
    const char* device_names = nullptr;

    if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
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
        if (fnd == old_devices.end())
        {
            auto* device_instance = alcOpenDevice(current_device_name);

            current_device = new jeal_device;
            current_device->m_device_name = jeecs::basic::make_new_string(current_device_name);
            current_device->m_openal_device = device_instance;

            jeecs::debug::loginfo("Found audio device: %s.", current_device_name);
            if (current_device->m_openal_device == nullptr)
                jeecs::debug::logerr("Failed to open device: '%s'.", current_device_name);
        }
        else
            current_device = *fnd;

        static_assert(ALC_EXT_disconnect);

        ALCint device_connected;
        alcGetIntegerv(current_device->m_openal_device, ALC_CONNECTED, 1, &device_connected);

        if (device_connected != ALC_FALSE)
        {
            current_device->m_alive = true;
            _jeal_all_devices.push_back(current_device);
        }
        else
            jeecs::debug::logwarn(
                "Audio device: '%s' disconnected, skip.", current_device_name);

        current_device_name += strlen(current_device_name) + 1;
    }

    if (_jeal_current_device != nullptr && _jeal_current_device->m_alive == false)
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

    if (_jeal_all_devices.size() == 0)
        jeecs::debug::logerr("No audio device found.");

    *out_len = _jeal_all_devices.size();
    return _jeal_all_devices.data();
}

jeal_device** jeal_get_all_devices(size_t* out_len)
{
    std::lock_guard g0(_jeal_all_devices_mx);
    std::lock_guard g3(_jeal_context_mx);

    _jeal_global_context context;

    bool need_try_restart_device = false;
    if (_jeal_current_device != nullptr)
    {
        _jeal_store_current_context(&context);
        need_try_restart_device = true;
    }

    // NOTE: 若当前设备在枚举过程中寄了，那么当前设备会被 _jeal_update_refetch_devices
    //       负责关闭，同时_jeal_current_device被置为nullptr
    jeal_device** result = _jeal_update_refetch_devices(out_len);

    if (need_try_restart_device && _jeal_current_device == nullptr)
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
    std::lock_guard g0(_jeal_all_devices_mx);
    std::lock_guard g1(_jeal_all_sources_mx);
    std::lock_guard g2(_jeal_all_buffers_mx);
    std::lock_guard g3(_jeal_context_mx);

    assert(_jeal_all_sources.empty());
    assert(_jeal_all_buffers.empty());

    _jeal_shutdown_current_device();
    for (auto* device : _jeal_all_devices)
    {
        assert(device != nullptr);
        alcCloseDevice(device->m_openal_device);

        jeecs::debug::loginfo("Audio device: %s closed.", device->m_device_name);
        je_mem_free((void*)device->m_device_name);

        delete device;
    }
    _jeal_all_devices.clear();
}

const char* jeal_device_name(jeal_device* device)
{
    std::lock_guard g0(_jeal_all_devices_mx);

    assert(device != nullptr);
    return device->m_device_name;
}

bool jeal_using_device(jeal_device* device)
{
    std::lock_guard g0(_jeal_all_devices_mx);
    std::lock_guard g1(_jeal_all_sources_mx);
    std::lock_guard g2(_jeal_all_buffers_mx);

    auto fnd = std::find(_jeal_all_devices.begin(), _jeal_all_devices.end(), device);
    if (fnd == _jeal_all_devices.end())
    {
        jeecs::debug::logerr("Trying to use invalid device: %p", device);
        return false;
    }
    if (device != _jeal_current_device)
    {
        std::lock_guard g3(_jeal_context_mx);

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

    std::lock_guard g1(_jeal_all_sources_mx);
    std::shared_lock g3(_jeal_context_mx);

    _jeal_all_sources.insert(audio_source);

    _jeal_update_source(audio_source);
    audio_source->m_last_played_buffer = nullptr;

    return audio_source;
}

void jeal_close_source(jeal_source* source)
{
    std::lock_guard g1(_jeal_all_sources_mx);
    std::shared_lock g3(_jeal_context_mx);
    {
        _jeal_all_sources.erase(source);
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

    //Now we set the variables that we passed in with the
    //data from the structs
    jeal_buffer* audio_buffer = new jeal_buffer;
    audio_buffer->m_data = data;
    audio_buffer->m_size = wave_data.subChunk2Size;
    audio_buffer->m_frequency = wave_format.sampleRate;
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
    }
    else if (wave_format.numChannels == 2)
    {
        if (wave_format.bitsPerSample == 8)
            audio_buffer->m_format = AL_FORMAT_STEREO8;
        else if (wave_format.bitsPerSample == 16)
            audio_buffer->m_format = AL_FORMAT_STEREO16;
    }

    std::lock_guard g1(_jeal_all_buffers_mx);
    std::shared_lock g3(_jeal_context_mx);

    _jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

jeal_buffer* jeal_create_buffer(
    const void* buffer_data,
    size_t buffer_data_len,
    size_t frequency,
    size_t byterate,
    jeal_format format)
{
    jeal_buffer* audio_buffer = new jeal_buffer;

    void* copy_buffer_data = malloc(buffer_data_len);
    memcpy(copy_buffer_data, buffer_data, buffer_data_len);

    audio_buffer->m_data = copy_buffer_data;
    audio_buffer->m_size = buffer_data_len;
    audio_buffer->m_frequency = frequency;
    audio_buffer->m_byterate = byterate;

    switch (format)
    {
    case MONO8:
        audio_buffer->m_format = AL_FORMAT_MONO8; break;
    case MONO16:
        audio_buffer->m_format = AL_FORMAT_MONO16; break;
    case STEREO8:
        audio_buffer->m_format = AL_FORMAT_STEREO8; break;
    case STEREO16:
        audio_buffer->m_format = AL_FORMAT_STEREO16; break;
    default:
        jeecs::debug::logerr("Bad audio buffer format: %d.", (int)format);
        break;
    }

    std::lock_guard g1(_jeal_all_buffers_mx);
    std::shared_lock g3(_jeal_context_mx);

    _jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

void jeal_close_buffer(jeal_buffer* buffer)
{
    std::lock_guard g1(_jeal_all_buffers_mx);
    std::shared_lock g3(_jeal_context_mx);
    {
        _jeal_all_buffers.erase(buffer);
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
    std::shared_lock g3(_jeal_context_mx);

    source->m_last_played_buffer = buffer;
    alSourcei(source->m_openal_source, AL_BUFFER, buffer->m_openal_buffer);
}

void jeal_source_loop(jeal_source* source, bool loop)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcei(source->m_openal_source, AL_LOOPING, loop ? 1 : 0);
}

void jeal_source_play(jeal_source* source)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcePlay(source->m_openal_source);
}

void jeal_source_pause(jeal_source* source)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcePause(source->m_openal_source);
}

void jeal_source_stop(jeal_source* source)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourceStop(source->m_openal_source);
}

void jeal_source_position(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g3(_jeal_context_mx);

    alSource3f(source->m_openal_source, AL_POSITION, x, y, z);
}
void jeal_source_velocity(jeal_source* source, float x, float y, float z)
{
    std::shared_lock g3(_jeal_context_mx);

    alSource3f(source->m_openal_source, AL_VELOCITY, x, y, z);
}

size_t jeal_source_get_byte_offset(jeal_source* source)
{
    std::shared_lock g3(_jeal_context_mx);

    ALint byte_offset;
    alGetSourcei(source->m_openal_source, AL_BYTE_OFFSET, &byte_offset);
    return byte_offset;
}

void jeal_source_set_byte_offset(jeal_source* source, size_t byteoffset)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcei(source->m_openal_source, AL_BYTE_OFFSET, (ALint)byteoffset);
}

void jeal_source_pitch(jeal_source* source, float playspeed)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcef(source->m_openal_source, AL_PITCH, playspeed);
}

void jeal_source_volume(jeal_source* source, float volume)
{
    std::shared_lock g3(_jeal_context_mx);

    alSourcef(source->m_openal_source, AL_GAIN, volume);
}

jeal_state jeal_source_get_state(jeal_source* source)
{
    std::shared_lock g3(_jeal_context_mx);

    ALint state;
    alGetSourcei(source->m_openal_source, AL_SOURCE_STATE, &state);

    switch (state)
    {
    case AL_PLAYING:
        return jeal_state::PLAYING;
    case AL_PAUSED:
        return jeal_state::PAUSED;
    default:
        assert(state == AL_INITIAL || state == AL_STOPPED);
        return jeal_state::STOPPED;
    }
}

void jeal_listener_position(float x, float y, float z)
{
    std::shared_lock g3(_jeal_context_mx);

    alListener3f(AL_POSITION, x, y, z);
}

void jeal_listener_velocity(float x, float y, float z)
{
    std::shared_lock g3(_jeal_context_mx);

    alListener3f(AL_VELOCITY, x, y, z);
}

void jeal_listener_direction(float yaw, float pitch, float roll)
{
    std::shared_lock g3(_jeal_context_mx);

    jeecs::math::quat rot(yaw, pitch, roll);
    jeecs::math::vec3 facing = rot * jeecs::math::vec3(0.0f, 0.0f, -1.0f);
    jeecs::math::vec3 topping = rot * jeecs::math::vec3(0.0f, 1.0f, 0.0f);

    float orientation[] = {
       facing.x, facing.y, facing.z,
       topping.x, topping.y, topping.z,
    };

    alListenerfv(AL_ORIENTATION, orientation);
}

void jeal_listener_volume(float volume)
{
    std::shared_lock g3(_jeal_context_mx);

    _jeal_listener_gain.store(volume);

    float global_gain;
    do
    {
        global_gain = _jeal_global_volume_gain.load();
        alListenerf(AL_GAIN, _jeal_listener_gain.load() * global_gain);
    } while (_jeal_global_volume_gain.load() != global_gain);
}

void jeal_global_volume(float volume)
{
    std::shared_lock g3(_jeal_context_mx);

    _jeal_global_volume_gain.store(volume);

    float listener_gain;
    do
    {
        listener_gain = _jeal_listener_gain.load();
        alListenerf(AL_GAIN, _jeal_global_volume_gain.load() * listener_gain);
    } while (_jeal_listener_gain.load() != listener_gain);
}