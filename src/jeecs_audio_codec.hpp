#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif

// ============================================================================
// jeecs_audio_codec.hpp —— 音频缓存载荷的编解码（PCM / IMA-ADPCM / Vorbis）
//
// 载荷区布局（缓存文件头之后，je_cache_binary_* 文档之前），所有多字节
// 整数按本机字节序存储：
//   [u32 codec]                                   je_audio_codec_t
//   PCM:    [u64 pcm_size][pcm bytes]
//   ADPCM:  [u64 decoded_bytes][u64 total_frames]
//           [u32 block_frames][u32 block_count]
//           [block entries][blocks]
//   VORBIS: [u64 decoded_bytes][u64 total_frames]
//           [u64 ogg_size][ogg stream]
//
// ADPCM 的块表项为 { u64 offset, u64 start_frame, u32 size, u32 rsv }，
// offset 相对载荷起点；每块数据 = 每声道 4 字节 {i16 predictor, u8 index,
// u8 rsv} + 声道优先的 4bit nibble，任意块可独立解码。
// VORBIS 的载荷是一段完整的 Ogg Vorbis 流，整段/流式解码都走 vorbisfile。
// ============================================================================

#include "jeecs/audio.hpp"
#include "jeecs/file.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jeecs::audio::codec
{
    struct audio_block_entry_t
    {
        uint64_t m_offset;      // 相对载荷起点的数据偏移
        uint64_t m_start_frame; // 块首解码帧号
        uint32_t m_size;        // 块数据字节数
        uint32_t m_reserved;

        audio_block_entry_t();
    };

    struct audio_payload_info_t
    {
        je_audio_codec_t m_codec;
        uint64_t m_payload_offset;   // [u32 codec] 在缓存文件中的偏移
        uint64_t m_decoded_pcm_size; // 解码后的交织 PCM 字节数
        uint64_t m_total_frames;     // 解码后的帧数

        // PCM
        uint64_t m_pcm_offset;

        // ADPCM
        uint32_t m_block_frames;
        std::vector<audio_block_entry_t> m_blocks;

        // VORBIS
        uint64_t m_ogg_offset;
        uint64_t m_ogg_size;

        audio_payload_info_t();
    };

    // 解析载荷头与块表，成功时文件读取位置停在后面的 je_cache_binary_*
    // 文档开头；不读取 PCM/压缩数据本身。
    bool audio_cache_payload_parse(je_file_t* cache, audio_payload_info_t* out);

    // 把整个载荷解码成 malloc 分配的交织 PCM（*out_pcm 由调用方 free）。
    bool audio_payload_decode_all(
        je_file_t* cache,
        const audio_payload_info_t& info,
        int32_t channels,
        size_t frame_bytes,
        void** out_pcm,
        size_t* out_size);

    // 流式解码器：按解码后的 PCM 字节偏移定位并读取，供声源的块队列使用。
    class audio_stream_decoder_t
    {
    public:
        audio_stream_decoder_t();
        virtual ~audio_stream_decoder_t();

        audio_stream_decoder_t(const audio_stream_decoder_t&) = delete;
        audio_stream_decoder_t& operator=(const audio_stream_decoder_t&) = delete;

        virtual bool seek_to_pcm_byte(size_t pcm_byte) = 0;
        virtual size_t read(void* dst, size_t bytes) = 0;
    };

    /* OPTIONAL: null if the payload is malformed or the codec is unsupported */
    audio_stream_decoder_t* create_audio_stream_decoder(
        je_file_t* cache,
        const audio_payload_info_t& info,
        int32_t channels,
        size_t frame_bytes);

    // 把整段已按目标位深转换的 PCM 按 codec 编码并写入缓存载荷区
    //（文件头之后、je_cache_binary_* 文档之前），供内置缓存生成脚本
    //（wojeapi_audio_cache_payload_write）调用；一次调用写完整个载荷区，
    // 调用方随后照常追加文档。原 je_audio_cache_payload_write C API 的
    // 内部化版本。quality 仅对 VORBIS 生效（0.0~1.0，越大音质越好、
    // 体积越大）；编码失败返回 false，调用方应中止缓存生成。
    bool audio_cache_payload_write(
        je_writing_cache_file_t* file,
        const void* pcm,
        size_t pcm_len,
        int32_t channels,
        int32_t sample_rate,
        int32_t bits,
        bool is_float,
        je_audio_codec_t codec,
        float quality);
}

namespace jeecs::audio
{
    // 打开音频缓存 <path>.je4cache（载荷区布局见文件头注释）：缓存有效时
    // 直接返回，缺失或失效时经内置生成器重建
    //（internal_script::regenerate_audio_cache；校验码为音频文件与可选的
    // <path>.je4audio 内容连续计算的 CRC64）。返回的句柄由调用方负责关闭
    //（见 je_file_close）；无法生成时为 nullptr。
    // 实现位于 jeecs_audio_codec.cpp。
    /* OPTIONAL */ je_file_t* open_or_create_cache(const char* path);
}
