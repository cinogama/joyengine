#define JE_IMPL
#include "jeecs.hpp"

#include <al.h>
#include <alc.h>

static jeal_device** _jegl_device_list = nullptr;
struct jeal_device
{
    const char* m_device_name;
    ALCdevice* m_openal_device;
};
struct jeal_source
{
    ALuint m_openal_source;
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
    // TODO: 在此检查是否有尚未关闭的声源和声音缓存，按照规矩，此处应该全部清退干净
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

void jeal_using_device(jeal_device* device)
{
    assert(device != nullptr);
    ALCcontext* current_context = alcGetCurrentContext();
    if (current_context != nullptr)
    {
        // TODO: 在此储存声源状态和声音样本，并在更新设备后恢复
        if (AL_FALSE == alcMakeContextCurrent(nullptr))
            jeecs::debug::logerr("Failed to clear current context.");
        alcDestroyContext(current_context);
    }
    current_context = alcCreateContext(device->m_openal_device, nullptr);
    if (current_context == nullptr)
        jeecs::debug::logerr("Failed to create context for device: %s.", device->m_device_name);
    else if (AL_FALSE == alcMakeContextCurrent(current_context))
        jeecs::debug::logerr("Failed to active context for device: %s.", device->m_device_name);
}

jeal_source* jeal_open_source()
{
    jeal_source* audio_source = new jeal_source;
    alGenSources(1, &audio_source->m_openal_source);
    return audio_source;
}

void jeal_close_source(jeal_source* source)
{
    alDeleteSources(1, &source->m_openal_source);
    delete source;
}

jeal_buffer* jeal_load_buffer_from_wav(const char* filename)
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

    alGenBuffers(1, &audio_buffer->m_openal_buffer);

    //now we put our data into the openAL buffer and
    //check for success
    alBufferData(audio_buffer->m_openal_buffer,
        audio_buffer->m_format,
        audio_buffer->m_data,
        audio_buffer->m_size,
        audio_buffer->m_frequency);

    jeecs_file_close(wav_file);

    return audio_buffer;
}

void jeal_close_buffer(jeal_buffer* buffer)
{
    alDeleteBuffers(1, &buffer->m_openal_buffer);
    free((void*)buffer->m_data);
    delete buffer;
}

void jeal_source_set_buffer(jeal_source* source, jeal_buffer* buffer)
{
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
