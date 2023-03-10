#define JE_IMPL
#include "jeecs.hpp"

#include <al.h>
#include <alc.h>

#include <unordered_set>

static jeal_device** _jegl_device_list = nullptr;
struct jeal_device
{
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
    bool m_loop;
};

std::mutex _jeal_all_sources_mx;
std::unordered_set<jeal_source*> _jeal_all_sources;

std::mutex _jeal_all_buffers_mx;
std::unordered_set<jeal_buffer*> _jeal_all_buffers;

void jeal_init()
{
    std::vector<jeal_device*> devices;

    // 如果支持枚举所有设备，就把枚举所有设备并全部打开
    const char* device_names = alcGetString(NULL,
        alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE
        ? ALC_DEVICE_SPECIFIER
        : ALC_DEFAULT_ALL_DEVICES_SPECIFIER
    );
    const char* current_device_name = device_names;

    while (current_device_name && *current_device_name != 0)
    {
        jeal_device* current_device = new jeal_device;
        current_device->m_device_name = jeecs::basic::make_new_string(current_device_name);
        current_device->m_openal_device = alcOpenDevice(current_device_name);

        jeecs::debug::loginfo("Found audio device: %s.", current_device_name);
        if (current_device->m_openal_device == nullptr)
            jeecs::debug::logfatal("Failed to open device: '%s'.", current_device_name);

        devices.push_back(current_device);

        current_device_name += strlen(current_device_name) + 1;
    }

    if (devices.size() == 0)
        jeecs::debug::logfatal("No audio device found.");

    _jegl_device_list = new jeal_device * [devices.size() + 1];
    memcpy(_jegl_device_list, devices.data(), devices.size() * sizeof(jeal_device*));
    _jegl_device_list[devices.size()] = nullptr;
}

void jeal_finish()
{
    // 在此检查是否有尚未关闭的声源和声音缓存，按照规矩，此处应该全部清退干净
    std::lock_guard g1(_jeal_all_sources_mx);
    std::lock_guard g2(_jeal_all_buffers_mx);
    assert(_jeal_all_sources.empty());
    assert(_jeal_all_buffers.empty());

    auto** current_device_pointer = _jegl_device_list;
    while (*current_device_pointer != nullptr)
    {
        alcCloseDevice((*current_device_pointer)->m_openal_device);

        jeecs::debug::loginfo("Audio device: %s closed.", (*current_device_pointer)->m_device_name);
        je_mem_free((void*)(*current_device_pointer)->m_device_name);

        delete* current_device_pointer;
        ++current_device_pointer;
    }
    delete[] _jegl_device_list;
    _jegl_device_list = nullptr;

    // 关闭上下文
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        if (AL_FALSE == alcMakeContextCurrent(nullptr))
            jeecs::debug::logerr("Failed to clear current context.");
        alcDestroyContext(current_context);
    }
}

jeal_device** jeal_get_all_devices()
{
    assert(_jegl_device_list != nullptr);
    return _jegl_device_list;
}

const char* jeal_device_name(jeal_device* device)
{
    assert(device != nullptr);
    return device->m_device_name;
}

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

void jeal_using_device(jeal_device* device)
{
    std::lock_guard g1(_jeal_all_sources_mx);
    std::lock_guard g2(_jeal_all_buffers_mx);

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
    };
    source_restoring_information listener_information;
    float listener_orientation[6] = {};

    std::vector<source_restoring_information> sources_information;

    assert(device != nullptr);
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        // 保存listener信息
        alGetListener3f(AL_POSITION,
            &listener_information.m_position.x,
            &listener_information.m_position.y,
            &listener_information.m_position.z);
        alGetListener3f(AL_VELOCITY,
            &listener_information.m_velocity.x,
            &listener_information.m_velocity.y,
            &listener_information.m_velocity.z);
        alGetListenerfv(AL_ORIENTATION, listener_orientation);
        alGetListenerf(AL_PITCH,
            &listener_information.m_pitch);
        alGetListenerf(AL_GAIN,
            &listener_information.m_volume);

        // 遍历所有 source，获取相关信息，在创建新的上下文之后恢复
        for (auto* source : _jeal_all_sources)
        {
            source_restoring_information src_info;
            src_info.m_source = source;
            alGetSource3f(source->m_openal_source, AL_POSITION,
                &src_info.m_position.x,
                &src_info.m_position.y,
                &src_info.m_position.z);
            alGetSource3f(source->m_openal_source, AL_VELOCITY,
                &src_info.m_velocity.x,
                &src_info.m_velocity.y,
                &src_info.m_velocity.z);
            src_info.m_offset = jeal_source_get_byte_offset(source);
            alGetSourcef(source->m_openal_source, AL_PITCH,
                &src_info.m_pitch);
            alGetSourcef(source->m_openal_source, AL_GAIN,
                &src_info.m_volume);
            src_info.m_state = jeal_source_get_state(source);
            src_info.m_playing_buffer = source->m_last_played_buffer;

            sources_information.emplace_back(src_info);
        }

        if (AL_FALSE == alcMakeContextCurrent(nullptr))
            jeecs::debug::logerr("Failed to clear current context.");
        alcDestroyContext(current_context);
    }
    current_context = alcCreateContext(device->m_openal_device, nullptr);
    if (current_context == nullptr)
        jeecs::debug::logerr("Failed to create context for device: %s.", device->m_device_name);
    else if (AL_FALSE == alcMakeContextCurrent(current_context))
        jeecs::debug::logerr("Failed to active context for device: %s.", device->m_device_name);
    else
    {
        // OK, Restore buffer, listener and source.
        for (auto* buffer : _jeal_all_buffers)
            _jeal_update_buffer_instance(buffer);

        jeal_listener_position(
            listener_information.m_position.x,
            listener_information.m_position.y,
            listener_information.m_position.z);
        jeal_listener_velocity(
            listener_information.m_velocity.x,
            listener_information.m_velocity.y,
            listener_information.m_velocity.z);
        jeal_listener_direction(
            listener_orientation[0],
            listener_orientation[1], 
            listener_orientation[2], 
            listener_orientation[3], 
            listener_orientation[4], 
            listener_orientation[5]);
        jeal_listener_pitch(listener_information.m_pitch);
        jeal_listener_volume(listener_information.m_volume);

        for (auto& src_info : sources_information)
        {
            _jeal_update_source(src_info.m_source);

            jeal_source_position(
                src_info.m_source,
                src_info.m_position.x,
                src_info.m_position.y,
                src_info.m_position.z);
            jeal_source_velocity(
                src_info.m_source,
                src_info.m_velocity.x,
                src_info.m_velocity.y,
                src_info.m_velocity.z);
            jeal_source_pitch(
                src_info.m_source,
                src_info.m_pitch);
            jeal_source_volume(
                src_info.m_source,
                src_info.m_volume);

            if (src_info.m_state != jeal_state::STOPPED)
            {
                jeal_source_set_buffer(src_info.m_source, src_info.m_playing_buffer);
                jeal_source_set_byte_offset(src_info.m_source, src_info.m_offset);
                if (src_info.m_state == jeal_state::PAUSED)
                    jeal_source_pause(src_info.m_source);
                else
                    jeal_source_play(src_info.m_source);
            }
        }
    }
}

jeal_source* jeal_open_source()
{
    jeal_source* audio_source = new jeal_source;

    std::lock_guard g1(_jeal_all_sources_mx);
    _jeal_all_sources.insert(audio_source);

    _jeal_update_source(audio_source);

    return audio_source;
}

void jeal_close_source(jeal_source* source)
{
    std::lock_guard g1(_jeal_all_sources_mx);
    {
        _jeal_all_sources.erase(source);
    }
    alDeleteSources(1, &source->m_openal_source);
    delete source;
}

jeal_buffer* jeal_load_buffer_from_wav(const char* filename, bool loop)
{
    struct WAVE_Data {//Wav文件数据体模块
        char subChunkID[4]; //should contain the word data
        long subChunk2Size; //Stores the size of the data block
    };

    struct WAVE_Format {//wav文件数据参数类型
        char subChunkID[4];
        long subChunkSize;
        short audioFormat;
        short numChannels;
        long sampleRate;
        long byteRate;
        short blockAlign;
        short bitsPerSample;
    };

    struct RIFF_Header {//RIFF块标准模型
        char chunkID[4];
        long chunkSize;//size not including chunkSize or chunkID
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
        jeecs::debug::logerr("Invalid wav file: '%s', unknown format.", filename);
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
        jeecs::debug::logerr("Invalid wav file: '%s', unknown format.", filename);
        jeecs_file_close(wav_file);
        return nullptr;
    }

    //check for extra parameters;
    if (wave_format.subChunkSize > 16)
    {
        short useless;
        jeecs_file_read(&useless, sizeof(short), 1, wav_file);
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
    audio_buffer->m_loop = loop;

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
    _jeal_all_buffers.insert(audio_buffer);

    _jeal_update_buffer_instance(audio_buffer);

    return audio_buffer;
}

void jeal_close_buffer(jeal_buffer* buffer)
{
    std::lock_guard g1(_jeal_all_buffers_mx);
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
    source->m_last_played_buffer = buffer;
    alSourcei(source->m_openal_source, AL_LOOPING, buffer->m_loop ? 1 : 0);
    alSourcei(source->m_openal_source, AL_BUFFER, buffer->m_openal_buffer);
}

void jeal_source_play(jeal_source* source)
{
    alSourcePlay(source->m_openal_source);
}

void jeal_source_pause(jeal_source* source)
{
    alSourcePause(source->m_openal_source);
}

void jeal_source_stop(jeal_source* source)
{
    alSourceStop(source->m_openal_source);
}

void jeal_source_position(jeal_source* source, float x, float y, float z)
{
    alSource3f(source->m_openal_source, AL_POSITION, x, y, z);
}
void jeal_source_velocity(jeal_source* source, float x, float y, float z)
{
    alSource3f(source->m_openal_source, AL_VELOCITY, x, y, z);
}

size_t jeal_source_get_byte_offset(jeal_source* source)
{
    ALint byte_offset;
    alGetSourcei(source->m_openal_source, AL_BYTE_OFFSET, &byte_offset);
    return byte_offset;
}

void jeal_source_set_byte_offset(jeal_source* source, size_t byteoffset)
{
    alSourcei(source->m_openal_source, AL_BYTE_OFFSET, (ALint)byteoffset);
}

void jeal_source_pitch(jeal_source* source, float playspeed)
{
    alSourcef(source->m_openal_source, AL_PITCH, playspeed);
}

void jeal_source_volume(jeal_source* source, float volume)
{
    alSourcef(source->m_openal_source, AL_GAIN, volume);
}

jeal_state jeal_source_get_state(jeal_source* source)
{
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
    alListener3f(AL_POSITION, x, y, z);
}

void jeal_listener_velocity(float x, float y, float z)
{
    alListener3f(AL_VELOCITY, x, y, z);
}

void jeal_listener_direction(float forwardx, float forwardy, float forwardz, float upx, float upy, float upz)
{
    float orientation[] = { forwardx, forwardy, forwardz, upx, upy, upz };
    alListenerfv(AL_ORIENTATION, orientation);
}

void jeal_listener_pitch(float playspeed)
{
    alListenerf(AL_PITCH, playspeed);
}

void jeal_listener_volume(float volume)
{
    alListenerf(AL_GAIN, volume);
}