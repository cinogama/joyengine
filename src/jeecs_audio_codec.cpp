#define JE_IMPL
#include "jeecs/audio.hpp"
#include "jeecs/cache.hpp"
#include "jeecs/file.hpp"
#include "jeecs/logging.hpp"
#include "jeecs_audio_codec.hpp"
#include "jeecs_basic_cache_version.hpp"
#include "jeecs_basic_internal_scripts.hpp"

#include <ogg/ogg.h>
#include <vorbis/codec.h>
#include <vorbis/vorbisenc.h>
#include <vorbis/vorbisfile.h>

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace jeecs::audio::codec
{
    namespace
    {
        // ADPCM 的单块目标帧数：约 0.17s@48kHz，兼顾随机定位粒度与块开销。
        constexpr uint32_t AUDIO_CODEC_BLOCK_TARGET_FRAMES = 8192;

        // 编码 Vorbis 时每次喂给分析器的帧数。
        constexpr int VORBIS_FEED_FRAMES = 4096;

        // IMA-ADPCM 步长表与索引表。
        const int s_adpcm_index_table[16] = {
            -1, -1, -1, -1, 2, 4, 6, 8,
            -1, -1, -1, -1, 2, 4, 6, 8,
        };
        const int s_adpcm_step_table[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
            34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130,
            143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408,
            449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282,
            1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
            3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630,
            9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
            22385, 24623, 27086, 29794, 32767,
        };

        bool read_exact(je_file_t* file, void* dst, size_t bytes)
        {
            if (bytes == 0)
                return true;

            return bytes == je_file_read(dst, sizeof(uint8_t), bytes, file);
        }

        bool write_exact(je_writing_cache_file_t* file, const void* src, size_t bytes)
        {
            if (bytes == 0)
                return true;

            return bytes == je_cache_file_write(src, sizeof(uint8_t), bytes, file);
        }

        bool read_u32(je_file_t* file, uint32_t* out)
        {
            return read_exact(file, out, sizeof(*out));
        }

        bool read_u64(je_file_t* file, uint64_t* out)
        {
            return read_exact(file, out, sizeof(*out));
        }

        bool write_u32(je_writing_cache_file_t* file, uint32_t value)
        {
            return write_exact(file, &value, sizeof(value));
        }

        bool write_u64(je_writing_cache_file_t* file, uint64_t value)
        {
            return write_exact(file, &value, sizeof(value));
        }

        bool seek_absolute(je_file_t* file, uint64_t offset)
        {
            if (offset > static_cast<uint64_t>(INT64_MAX))
                return false;

            return je_file_seek(file, static_cast<int64_t>(offset), JE_READ_FILE_SEEK_SET);
        }

        bool skip_bytes(je_file_t* file, uint64_t bytes)
        {
            if (bytes == 0)
                return true;

            const uint64_t position = je_file_tell(file);
            if (position > static_cast<uint64_t>(INT64_MAX) - bytes)
                return false;

            return seek_absolute(file, position + bytes);
        }

        ///////////////////////////////////////////////////////////////////////
        // IMA-ADPCM
        ///////////////////////////////////////////////////////////////////////

        int adpcm_clamp_predictor(int predictor)
        {
            if (predictor > 32767)
                return 32767;
            if (predictor < -32768)
                return -32768;
            return predictor;
        }

        int adpcm_clamp_index(int index)
        {
            if (index < 0)
                return 0;
            if (index > 88)
                return 88;
            return index;
        }

        int adpcm_encode_sample(int sample, int* predictor, int* index)
        {
            int step = s_adpcm_step_table[*index];
            int diff = sample - *predictor;

            int code = 0;
            if (diff < 0)
            {
                code = 8;
                diff = -diff;
            }

            int vpdiff = step >> 3;
            if (diff >= step)
            {
                code |= 4;
                diff -= step;
                vpdiff += step;
            }
            step >>= 1;
            if (diff >= step)
            {
                code |= 2;
                diff -= step;
                vpdiff += step;
            }
            step >>= 1;
            if (diff >= step)
            {
                code |= 1;
                vpdiff += step;
            }

            if (code & 8)
                *predictor -= vpdiff;
            else
                *predictor += vpdiff;

            *predictor = adpcm_clamp_predictor(*predictor);
            *index = adpcm_clamp_index(*index + s_adpcm_index_table[code]);

            return code;
        }

        int adpcm_decode_sample(int code, int* predictor, int* index)
        {
            const int step = s_adpcm_step_table[*index];
            const int delta = code & 7;

            int vpdiff = step >> 3;
            if (delta & 4)
                vpdiff += step;
            if (delta & 2)
                vpdiff += step >> 1;
            if (delta & 1)
                vpdiff += step >> 2;

            if (code & 8)
                *predictor -= vpdiff;
            else
                *predictor += vpdiff;

            *predictor = adpcm_clamp_predictor(*predictor);
            *index = adpcm_clamp_index(*index + s_adpcm_index_table[code]);

            return *predictor;
        }

        // 一个块：每声道 4 字节初始状态，随后是声道优先的 nibble 数据。
        void adpcm_encode_block(
            const int16_t* interleaved,
            size_t frames,
            int32_t channels,
            std::vector<uint8_t>& out)
        {
            const size_t nibble_bytes = frames / 2;

            out.assign(static_cast<size_t>(channels) * 4, 0);

            for (int32_t channel = 0; channel < channels; ++channel)
            {
                int predictor = frames > 0 ? interleaved[channel] : 0;
                int index = 0;

                out[static_cast<size_t>(channel) * 4 + 0] = static_cast<uint8_t>(predictor & 0xff);
                out[static_cast<size_t>(channel) * 4 + 1] = static_cast<uint8_t>((predictor >> 8) & 0xff);
                out[static_cast<size_t>(channel) * 4 + 2] = static_cast<uint8_t>(index);
                out[static_cast<size_t>(channel) * 4 + 3] = 0;

                if (nibble_bytes == 0)
                    continue;

                const size_t data_offset
                    = static_cast<size_t>(channels) * 4 + static_cast<size_t>(channel) * nibble_bytes;
                out.resize(data_offset + nibble_bytes, 0);

                for (size_t frame = 1; frame < frames; ++frame)
                {
                    const int code = adpcm_encode_sample(
                        interleaved[frame * static_cast<size_t>(channels) + static_cast<size_t>(channel)],
                        &predictor,
                        &index);

                    const size_t nibble = frame - 1;
                    if ((nibble & 1) == 0)
                        out[data_offset + nibble / 2] = static_cast<uint8_t>(code & 0xf);
                    else
                        out[data_offset + nibble / 2] |= static_cast<uint8_t>((code & 0xf) << 4);
                }
            }
        }

        bool adpcm_decode_block(
            const uint8_t* data,
            size_t size,
            size_t frames,
            int32_t channels,
            std::vector<int16_t>& out)
        {
            out.assign(frames * static_cast<size_t>(channels), 0);

            if (frames == 0)
                return true;
            if (size < static_cast<size_t>(channels) * 4)
                return false;

            const size_t nibble_bytes = frames / 2;
            if (size < static_cast<size_t>(channels) * (4 + nibble_bytes))
                return false;

            for (int32_t channel = 0; channel < channels; ++channel)
            {
                const size_t header = static_cast<size_t>(channel) * 4;
                int predictor = static_cast<int16_t>(
                    static_cast<uint16_t>(data[header])
                    | (static_cast<uint16_t>(data[header + 1]) << 8));
                int index = adpcm_clamp_index(data[header + 2]);

                out[static_cast<size_t>(channel)] = static_cast<int16_t>(predictor);

                if (frames <= 1)
                    continue;

                const uint8_t* bits = data
                    + static_cast<size_t>(channels) * 4
                    + static_cast<size_t>(channel) * nibble_bytes;

                for (size_t frame = 1; frame < frames; ++frame)
                {
                    const size_t nibble = frame - 1;
                    const uint8_t packed = bits[nibble / 2];
                    const int code = (nibble & 1) == 0
                        ? (packed & 0xf)
                        : ((packed >> 4) & 0xf);

                    out[frame * static_cast<size_t>(channels) + static_cast<size_t>(channel)]
                        = static_cast<int16_t>(adpcm_decode_sample(code, &predictor, &index));
                }
            }

            return true;
        }

        bool adpcm_encode_payload(
            je_writing_cache_file_t* file,
            const void* pcm,
            size_t pcm_len,
            int32_t channels,
            int32_t bits,
            bool is_float)
        {
            if (bits != 16 || is_float)
            {
                debug::logerr(
                    "ADPCM audio cache requires 16-bit integer PCM (%d-bit%s).",
                    (int)bits,
                    is_float ? " float" : "");
                return false;
            }

            const int16_t* samples = static_cast<const int16_t*>(pcm);
            const size_t frame_bytes = static_cast<size_t>(channels) * sizeof(int16_t);
            const uint64_t total_frames = pcm_len / frame_bytes;
            const uint32_t block_frames = AUDIO_CODEC_BLOCK_TARGET_FRAMES;
            const uint32_t block_count = static_cast<uint32_t>(
                (total_frames + block_frames - 1) / block_frames);

            std::vector<std::vector<uint8_t>> blocks;
            blocks.reserve(block_count);

            uint64_t block_data_size = 0;
            for (uint32_t block = 0; block < block_count; ++block)
            {
                const uint64_t frame_offset = static_cast<uint64_t>(block) * block_frames;
                const size_t frames = static_cast<size_t>(std::min<uint64_t>(
                    block_frames, total_frames - frame_offset));

                std::vector<uint8_t> encoded;
                adpcm_encode_block(
                    samples + frame_offset * static_cast<uint64_t>(channels),
                    frames,
                    channels,
                    encoded);

                block_data_size += encoded.size();
                blocks.push_back(std::move(encoded));
            }

            // [u32 codec][u64 decoded][u64 frames][u32 block_frames]
            // [u32 block_count][entries][blocks]
            const uint64_t payload_header_size
                = sizeof(uint32_t) + sizeof(uint64_t) * 2
                + sizeof(uint32_t) * 2;
            const uint64_t table_size
                = static_cast<uint64_t>(block_count) * 24;
            const uint64_t decoded_size
                = total_frames * static_cast<uint64_t>(frame_bytes);

            if (!write_u32(file, JE_AUDIO_CODEC_ADPCM)
                || !write_u64(file, decoded_size)
                || !write_u64(file, total_frames)
                || !write_u32(file, block_frames)
                || !write_u32(file, block_count))
            {
                return false;
            }

            uint64_t block_offset = payload_header_size + table_size;
            for (uint32_t block = 0; block < block_count; ++block)
            {
                if (!write_u64(file, block_offset)
                    || !write_u64(
                        file, static_cast<uint64_t>(block) * block_frames)
                    || !write_u32(file, static_cast<uint32_t>(blocks[block].size()))
                    || !write_u32(file, 0))
                {
                    return false;
                }

                block_offset += blocks[block].size();
            }

            for (const auto& block : blocks)
            {
                if (!write_exact(file, block.data(), block.size()))
                    return false;
            }

            return true;
        }

        ///////////////////////////////////////////////////////////////////////
        // Vorbis 编码（内嵌 Ogg 流）
        ///////////////////////////////////////////////////////////////////////

        struct vorbis_encoder_session_t
        {
            vorbis_info m_info;
            vorbis_comment m_comment;
            vorbis_dsp_state m_dsp;
            vorbis_block m_block;
            ogg_stream_state m_stream;

            bool m_info_ready;
            bool m_comment_ready;
            bool m_dsp_ready;
            bool m_block_ready;
            bool m_stream_ready;

            vorbis_encoder_session_t()
            {
                m_info_ready = false;
                m_comment_ready = false;
                m_dsp_ready = false;
                m_block_ready = false;
                m_stream_ready = false;
            }

            ~vorbis_encoder_session_t()
            {
                if (m_stream_ready)
                    ogg_stream_clear(&m_stream);
                if (m_block_ready)
                    vorbis_block_clear(&m_block);
                if (m_dsp_ready)
                    vorbis_dsp_clear(&m_dsp);
                if (m_comment_ready)
                    vorbis_comment_clear(&m_comment);
                if (m_info_ready)
                    vorbis_info_clear(&m_info);
            }
        };

        void append_ogg_page(std::vector<uint8_t>& out, const ogg_page& page)
        {
            const uint8_t* header = page.header;
            out.insert(out.end(), header, header + page.header_len);

            const uint8_t* body = page.body;
            out.insert(out.end(), body, body + page.body_len);
        }

        bool drain_encoder_packets(
            vorbis_encoder_session_t& session,
            std::vector<uint8_t>& out)
        {
            ogg_page page;

            while (vorbis_analysis_blockout(&session.m_dsp, &session.m_block) == 1)
            {
                vorbis_analysis(&session.m_block, nullptr);
                vorbis_bitrate_addblock(&session.m_block);

                ogg_packet packet;
                while (vorbis_bitrate_flushpacket(&session.m_dsp, &packet))
                {
                    if (ogg_stream_packetin(&session.m_stream, &packet) != 0)
                        return false;

                    while (ogg_stream_pageout(&session.m_stream, &page) != 0)
                        append_ogg_page(out, page);
                }
            }

            return true;
        }

        bool vorbis_encode_all(
            const float* pcm,
            uint64_t frames,
            int32_t channels,
            int32_t sample_rate,
            float quality,
            std::vector<uint8_t>& out)
        {
            if (quality < 0.0f || quality > 1.0f)
            {
                debug::logerr("Invalid Vorbis audio cache quality: %f.", (double)quality);
                return false;
            }

            vorbis_encoder_session_t session;

            vorbis_info_init(&session.m_info);
            session.m_info_ready = true;

            if (vorbis_encode_init_vbr(
                    &session.m_info, channels, sample_rate, quality) != 0)
            {
                debug::logerr(
                    "Unable to initialize Vorbis encoder (channels=%d, rate=%d, quality=%f).",
                    (int)channels,
                    (int)sample_rate,
                    (double)quality);
                return false;
            }

            vorbis_comment_init(&session.m_comment);
            session.m_comment_ready = true;
            vorbis_comment_add_tag(&session.m_comment, "ENCODER", "JoyEngineECS");

            if (vorbis_analysis_init(&session.m_dsp, &session.m_info) != 0)
            {
                debug::logerr("Unable to initialize Vorbis analysis state.");
                return false;
            }
            session.m_dsp_ready = true;

            if (vorbis_block_init(&session.m_dsp, &session.m_block) != 0)
            {
                debug::logerr("Unable to initialize Vorbis block state.");
                return false;
            }
            session.m_block_ready = true;

            if (ogg_stream_init(&session.m_stream, 0x4a454543) != 0)
            {
                debug::logerr("Unable to initialize Ogg stream for Vorbis cache.");
                return false;
            }
            session.m_stream_ready = true;

            ogg_packet header;
            ogg_packet header_comment;
            ogg_packet header_codebook;
            if (vorbis_analysis_headerout(
                    &session.m_dsp,
                    &session.m_comment,
                    &header,
                    &header_comment,
                    &header_codebook) != 0)
            {
                debug::logerr("Unable to produce Vorbis headers.");
                return false;
            }

            if (ogg_stream_packetin(&session.m_stream, &header) != 0
                || ogg_stream_packetin(&session.m_stream, &header_comment) != 0
                || ogg_stream_packetin(&session.m_stream, &header_codebook) != 0)
            {
                debug::logerr("Unable to store Vorbis headers in Ogg stream.");
                return false;
            }

            out.clear();
            ogg_page page;
            while (ogg_stream_flush(&session.m_stream, &page) != 0)
                append_ogg_page(out, page);

            uint64_t position = 0;
            while (position < frames)
            {
                const int feed = static_cast<int>(std::min<uint64_t>(
                    VORBIS_FEED_FRAMES, frames - position));

                float** buffer = vorbis_analysis_buffer(&session.m_dsp, feed);
                if (buffer == nullptr)
                {
                    debug::logerr("Unable to allocate Vorbis analysis buffer.");
                    return false;
                }

                for (int32_t channel = 0; channel < channels; ++channel)
                {
                    for (int frame = 0; frame < feed; ++frame)
                    {
                        buffer[channel][frame]
                            = pcm[(position + static_cast<uint64_t>(frame))
                                * static_cast<uint64_t>(channels)
                                + static_cast<uint64_t>(channel)];
                    }
                }

                vorbis_analysis_wrote(&session.m_dsp, feed);

                if (!drain_encoder_packets(session, out))
                    return false;

                position += static_cast<uint64_t>(feed);
            }

            vorbis_analysis_wrote(&session.m_dsp, 0);
            if (!drain_encoder_packets(session, out))
                return false;

            while (ogg_stream_flush(&session.m_stream, &page) != 0)
                append_ogg_page(out, page);

            return true;
        }

        bool vorbis_encode_payload(
            je_writing_cache_file_t* file,
            const void* pcm,
            size_t pcm_len,
            int32_t channels,
            int32_t sample_rate,
            int32_t bits,
            bool is_float,
            float quality)
        {
            if (bits != 32 || !is_float)
            {
                debug::logerr(
                    "Vorbis audio cache requires 32-bit float PCM (%d-bit%s).",
                    (int)bits,
                    is_float ? " float" : "");
                return false;
            }

            const size_t frame_bytes = static_cast<size_t>(channels) * sizeof(float);
            const uint64_t total_frames = pcm_len / frame_bytes;
            const uint64_t decoded_size
                = total_frames * static_cast<uint64_t>(frame_bytes);

            std::vector<uint8_t> ogg;
            if (!vorbis_encode_all(
                    static_cast<const float*>(pcm),
                    total_frames,
                    channels,
                    sample_rate,
                    quality,
                    ogg))
            {
                return false;
            }

            return write_u32(file, JE_AUDIO_CODEC_VORBIS)
                && write_u64(file, decoded_size)
                && write_u64(file, total_frames)
                && write_u64(file, static_cast<uint64_t>(ogg.size()))
                && write_exact(file, ogg.data(), ogg.size());
        }

        ///////////////////////////////////////////////////////////////////////
        // Vorbisfile datasource
        ///////////////////////////////////////////////////////////////////////

        struct vorbis_datasource_t
        {
            je_file_t* m_file;
            int64_t m_base;
            int64_t m_size;
            int64_t m_position;

            vorbis_datasource_t(je_file_t* file, uint64_t base, uint64_t size)
            {
                m_file = file;
                m_base = static_cast<int64_t>(base);
                m_size = static_cast<int64_t>(size);
                m_position = 0;
            }
        };

        size_t vorbis_read_callback(void* ptr, size_t size, size_t count, void* datasource)
        {
            auto* source = static_cast<vorbis_datasource_t*>(datasource);
            if (size == 0 || count == 0)
                return 0;

            const int64_t remaining = source->m_size - source->m_position;
            if (remaining <= 0)
                return 0;

            size_t want = size * count;
            if (want / size != count)
                return 0;
            if (want > static_cast<size_t>(remaining))
                want = static_cast<size_t>(remaining);

            if (!je_file_seek(
                    source->m_file,
                    source->m_base + source->m_position,
                    JE_READ_FILE_SEEK_SET))
                return 0;

            const size_t got = je_file_read(ptr, sizeof(uint8_t), want, source->m_file);
            source->m_position += static_cast<int64_t>(got);

            return got / size;
        }

        int vorbis_seek_callback(void* datasource, ogg_int64_t offset, int whence)
        {
            auto* source = static_cast<vorbis_datasource_t*>(datasource);

            int64_t target = 0;
            switch (whence)
            {
            case SEEK_SET:
                target = static_cast<int64_t>(offset);
                break;
            case SEEK_CUR:
                target = source->m_position + static_cast<int64_t>(offset);
                break;
            case SEEK_END:
                target = source->m_size + static_cast<int64_t>(offset);
                break;
            default:
                return -1;
            }

            if (target < 0 || target > source->m_size)
                return -1;

            source->m_position = target;
            return 0;
        }

        int vorbis_close_callback(void* datasource)
        {
            delete static_cast<vorbis_datasource_t*>(datasource);
            return 0;
        }

        long vorbis_tell_callback(void* datasource)
        {
            return static_cast<long>(
                static_cast<vorbis_datasource_t*>(datasource)->m_position);
        }

        ///////////////////////////////////////////////////////////////////////
        // Stream decoders
        ///////////////////////////////////////////////////////////////////////

        class pcm_stream_decoder_t final : public audio_stream_decoder_t
        {
        public:
            pcm_stream_decoder_t(
                je_file_t* cache,
                uint64_t pcm_offset,
                uint64_t pcm_size)
            {
                m_cache = cache;
                m_pcm_offset = pcm_offset;
                m_size = pcm_size;
                m_position = 0;
            }

            bool seek_to_pcm_byte(size_t pcm_byte) override
            {
                if (static_cast<uint64_t>(pcm_byte) > m_size)
                    return false;

                m_position = pcm_byte;
                return true;
            }

            size_t read(void* dst, size_t bytes) override
            {
                if (m_position >= m_size)
                    return 0;

                size_t want = static_cast<size_t>(m_size - m_position);
                if (want > bytes)
                    want = bytes;

                uint8_t* out = static_cast<uint8_t*>(dst);
                size_t produced = 0;

                while (produced < want)
                {
                    if (!seek_absolute(m_cache, m_pcm_offset + m_position))
                        break;

                    const size_t got = je_file_read(
                        out + produced, sizeof(uint8_t), want - produced, m_cache);
                    if (got == 0)
                        break;

                    m_position += got;
                    produced += got;
                }

                return produced;
            }

        private:
            je_file_t* m_cache;
            uint64_t m_pcm_offset;
            uint64_t m_size;
            size_t m_position;
        };

        class adpcm_stream_decoder_t final : public audio_stream_decoder_t
        {
        public:
            adpcm_stream_decoder_t(
                je_file_t* cache,
                const audio_payload_info_t& info,
                int32_t channels,
                size_t frame_bytes)
            {
                m_cache = cache;
                m_info = info;
                m_channels = channels;
                m_frame_bytes = frame_bytes;
                m_position = 0;
                m_loaded_block = UINT32_MAX;
                m_block_frame_count = 0;
            }

            bool seek_to_pcm_byte(size_t pcm_byte) override
            {
                if (static_cast<uint64_t>(pcm_byte) > m_info.m_decoded_pcm_size)
                    return false;

                m_position = pcm_byte;
                return true;
            }

            size_t read(void* dst, size_t bytes) override
            {
                if (m_position >= m_info.m_decoded_pcm_size)
                    return 0;

                size_t want = static_cast<size_t>(
                    m_info.m_decoded_pcm_size - m_position);
                if (want > bytes)
                    want = bytes;

                uint8_t* out = static_cast<uint8_t*>(dst);
                size_t produced = 0;

                while (produced < want)
                {
                    const uint64_t frame = m_position / m_frame_bytes;
                    const uint32_t block_index = static_cast<uint32_t>(
                        frame / m_info.m_block_frames);
                    const size_t intra_frame = static_cast<size_t>(
                        frame % m_info.m_block_frames);

                    if (!ensure_block(block_index))
                        break;

                    const size_t available_frames
                        = m_block_frame_count - intra_frame;
                    size_t chunk = available_frames * m_frame_bytes;
                    if (chunk > want - produced)
                        chunk = want - produced;

                    memcpy(
                        out + produced,
                        reinterpret_cast<const uint8_t*>(m_block_pcm.data())
                            + intra_frame * m_frame_bytes,
                        chunk);

                    produced += chunk;
                    m_position += chunk;
                }

                return produced;
            }

        private:
            bool ensure_block(uint32_t block_index)
            {
                if (block_index == m_loaded_block)
                    return true;

                if (block_index >= m_info.m_blocks.size())
                {
                    debug::logerr("Audio cache ADPCM block index out of range.");
                    return false;
                }

                const auto& entry = m_info.m_blocks[block_index];
                const uint64_t block_first
                    = static_cast<uint64_t>(block_index) * m_info.m_block_frames;
                if (block_first >= m_info.m_total_frames)
                {
                    debug::logerr("Audio cache ADPCM block starts beyond the stream.");
                    return false;
                }

                const size_t frames = static_cast<size_t>(std::min<uint64_t>(
                    m_info.m_block_frames, m_info.m_total_frames - block_first));

                std::vector<uint8_t> compressed(entry.m_size);
                if (!seek_absolute(
                        m_cache,
                        m_info.m_payload_offset + entry.m_offset)
                    || entry.m_size != je_file_read(
                        compressed.data(),
                        sizeof(uint8_t),
                        compressed.size(),
                        m_cache))
                {
                    debug::logerr("Failed to read an ADPCM block from the audio cache.");
                    return false;
                }

                if (!adpcm_decode_block(
                        compressed.data(),
                        compressed.size(),
                        frames,
                        m_channels,
                        m_block_pcm))
                {
                    debug::logerr("Invalid ADPCM block in the audio cache.");
                    return false;
                }

                m_loaded_block = block_index;
                m_block_frame_count = frames;
                return true;
            }

            je_file_t* m_cache;
            audio_payload_info_t m_info;
            int32_t m_channels;
            size_t m_frame_bytes;
            size_t m_position;

            uint32_t m_loaded_block;
            size_t m_block_frame_count;
            std::vector<int16_t> m_block_pcm;
        };

        class vorbis_stream_decoder_t final : public audio_stream_decoder_t
        {
        public:
            vorbis_stream_decoder_t(
                je_file_t* cache,
                const audio_payload_info_t& info,
                int32_t channels,
                size_t frame_bytes)
            {
                m_info = info;
                m_channels = channels;
                m_frame_bytes = frame_bytes;
                m_position = 0;
                m_open = false;
                memset(&m_file, 0, sizeof(m_file));

                auto* source = new vorbis_datasource_t(
                    cache, info.m_ogg_offset, info.m_ogg_size);

                ov_callbacks callbacks;
                callbacks.read_func = vorbis_read_callback;
                callbacks.seek_func = vorbis_seek_callback;
                callbacks.close_func = vorbis_close_callback;
                callbacks.tell_func = vorbis_tell_callback;

                if (ov_open_callbacks(source, &m_file, nullptr, 0, callbacks) != 0)
                {
                    delete source;
                    return;
                }

                vorbis_info* vorbis_cache_info = ov_info(&m_file, -1);
                if (vorbis_cache_info == nullptr
                    || vorbis_cache_info->channels != m_channels
                    || ov_seekable(&m_file) == 0)
                {
                    ov_clear(&m_file);
                    return;
                }

                const ogg_int64_t total = ov_pcm_total(&m_file, -1);
                if (total < 0
                    || static_cast<uint64_t>(total) != m_info.m_total_frames)
                {
                    debug::logerr(
                        "Audio cache Vorbis frame count mismatch (%lld vs %llu).",
                        (long long)total,
                        (unsigned long long)m_info.m_total_frames);
                    ov_clear(&m_file);
                    return;
                }

                m_open = true;
            }

            ~vorbis_stream_decoder_t() override
            {
                if (m_open)
                    ov_clear(&m_file);
            }

            bool seek_to_pcm_byte(size_t pcm_byte) override
            {
                if (!m_open)
                    return false;
                if (static_cast<uint64_t>(pcm_byte) > m_info.m_decoded_pcm_size)
                    return false;
                if (pcm_byte % m_frame_bytes != 0)
                    return false;
                if (pcm_byte == m_position)
                    return true;

                if (static_cast<uint64_t>(pcm_byte) == m_info.m_decoded_pcm_size)
                {
                    m_position = pcm_byte;
                    return true;
                }

                const size_t frame = pcm_byte / m_frame_bytes;
                if (ov_pcm_seek(&m_file, static_cast<ogg_int64_t>(frame)) != 0)
                {
                    debug::logerr("Failed to seek the Vorbis audio cache.");
                    return false;
                }

                m_position = pcm_byte;
                return true;
            }

            size_t read(void* dst, size_t bytes) override
            {
                if (!m_open || m_position >= m_info.m_decoded_pcm_size)
                    return 0;

                size_t want = static_cast<size_t>(
                    m_info.m_decoded_pcm_size - m_position);
                if (want > bytes)
                    want = bytes;

                const size_t frames_want = want / m_frame_bytes;
                size_t frames_done = 0;

                float* out = static_cast<float*>(dst);
                int bitstream = 0;

                while (frames_done < frames_want)
                {
                    float** planar = nullptr;
                    const long got = ov_read_float(
                        &m_file,
                        &planar,
                        static_cast<int>(std::min<size_t>(
                            frames_want - frames_done, INT_MAX)),
                        &bitstream);

                    if (got < 0)
                    {
                        debug::logerr("Failed to decode the Vorbis audio cache.");
                        break;
                    }
                    if (got == 0)
                        break;

                    for (long frame = 0; frame < got; ++frame)
                    {
                        for (int32_t channel = 0; channel < m_channels; ++channel)
                        {
                            out[(frames_done + static_cast<size_t>(frame))
                                    * static_cast<size_t>(m_channels)
                                + static_cast<size_t>(channel)]
                                = planar[channel][frame];
                        }
                    }

                    frames_done += static_cast<size_t>(got);
                }

                const size_t produced = frames_done * m_frame_bytes;
                m_position += produced;
                return produced;
            }

        private:
            audio_payload_info_t m_info;
            int32_t m_channels;
            size_t m_frame_bytes;
            size_t m_position;
            bool m_open;
            OggVorbis_File m_file;
        };
    }

    audio_block_entry_t::audio_block_entry_t()
    {
        m_offset = 0;
        m_start_frame = 0;
        m_size = 0;
        m_reserved = 0;
    }

    audio_stream_decoder_t::audio_stream_decoder_t()
    {
    }

    audio_stream_decoder_t::~audio_stream_decoder_t()
    {
    }

    audio_payload_info_t::audio_payload_info_t()
    {
        m_codec = JE_AUDIO_CODEC_PCM;
        m_payload_offset = 0;
        m_decoded_pcm_size = 0;
        m_total_frames = 0;
        m_pcm_offset = 0;
        m_block_frames = 0;
        m_ogg_offset = 0;
        m_ogg_size = 0;
    }

    bool audio_cache_payload_parse(je_file_t* cache, audio_payload_info_t* out)
    {
        const size_t file_size = je_file_size(cache);
        const size_t position = je_file_tell(cache);
        if (position > file_size)
            return false;

        audio_payload_info_t info;
        info.m_payload_offset = position;

        uint32_t codec = 0;
        if (!read_u32(cache, &codec))
            return false;

        switch (codec)
        {
        case JE_AUDIO_CODEC_PCM:
        {
            uint64_t pcm_size = 0;
            if (!read_u64(cache, &pcm_size))
                return false;

            const size_t pcm_offset = je_file_tell(cache);
            if (pcm_offset > file_size
                || pcm_size > static_cast<uint64_t>(file_size - pcm_offset))
                return false;

            info.m_codec = JE_AUDIO_CODEC_PCM;
            info.m_pcm_offset = pcm_offset;
            info.m_decoded_pcm_size = pcm_size;

            if (!skip_bytes(cache, pcm_size))
                return false;
            break;
        }
        case JE_AUDIO_CODEC_ADPCM:
        {
            uint64_t decoded_size = 0;
            uint64_t total_frames = 0;
            uint32_t block_frames = 0;
            uint32_t block_count = 0;

            if (!read_u64(cache, &decoded_size)
                || !read_u64(cache, &total_frames)
                || !read_u32(cache, &block_frames)
                || !read_u32(cache, &block_count))
                return false;

            if (block_frames == 0 || (block_count == 0 && total_frames != 0))
                return false;

            const size_t table_offset = je_file_tell(cache);
            if (table_offset > file_size
                || static_cast<uint64_t>(block_count)
                    > static_cast<uint64_t>(file_size - table_offset) / 24)
                return false;

            const uint64_t table_end = static_cast<uint64_t>(table_offset)
                + static_cast<uint64_t>(block_count) * 24;
            const uint64_t table_end_in_payload
                = table_end - info.m_payload_offset;

            info.m_codec = JE_AUDIO_CODEC_ADPCM;
            info.m_decoded_pcm_size = decoded_size;
            info.m_total_frames = total_frames;
            info.m_block_frames = block_frames;
            info.m_blocks.resize(block_count);

            uint64_t blocks_end = table_offset;
            for (uint32_t block = 0; block < block_count; ++block)
            {
                uint64_t offset = 0;
                uint64_t start_frame = 0;
                uint32_t size = 0;
                uint32_t reserved = 0;

                if (!read_u64(cache, &offset)
                    || !read_u64(cache, &start_frame)
                    || !read_u32(cache, &size)
                    || !read_u32(cache, &reserved))
                    return false;

                if (offset < table_end_in_payload
                    || size == 0
                    || offset > static_cast<uint64_t>(file_size) - info.m_payload_offset
                    || size > file_size - info.m_payload_offset - offset)
                    return false;

                info.m_blocks[block].m_offset = offset;
                info.m_blocks[block].m_start_frame = start_frame;
                info.m_blocks[block].m_size = size;
                info.m_blocks[block].m_reserved = reserved;

                const uint64_t block_end = info.m_payload_offset + offset + size;
                if (block_end > blocks_end)
                    blocks_end = block_end;
            }

            if (!seek_absolute(cache, blocks_end))
                return false;
            break;
        }
        case JE_AUDIO_CODEC_VORBIS:
        {
            uint64_t decoded_size = 0;
            uint64_t total_frames = 0;
            uint64_t ogg_size = 0;

            if (!read_u64(cache, &decoded_size)
                || !read_u64(cache, &total_frames)
                || !read_u64(cache, &ogg_size))
                return false;

            const size_t ogg_offset = je_file_tell(cache);
            if (ogg_offset > file_size
                || ogg_size > static_cast<uint64_t>(file_size - ogg_offset))
                return false;

            info.m_codec = JE_AUDIO_CODEC_VORBIS;
            info.m_decoded_pcm_size = decoded_size;
            info.m_total_frames = total_frames;
            info.m_ogg_offset = ogg_offset;
            info.m_ogg_size = ogg_size;

            if (!skip_bytes(cache, ogg_size))
                return false;
            break;
        }
        default:
            return false;
        }

        *out = std::move(info);
        return true;
    }

    audio_stream_decoder_t* create_audio_stream_decoder(
        je_file_t* cache,
        const audio_payload_info_t& info,
        int32_t channels,
        size_t frame_bytes)
    {
        if (channels != 1 && channels != 2)
            return nullptr;

        switch (info.m_codec)
        {
        case JE_AUDIO_CODEC_PCM:
            return new pcm_stream_decoder_t(
                cache, info.m_pcm_offset, info.m_decoded_pcm_size);
        case JE_AUDIO_CODEC_ADPCM:
            return new adpcm_stream_decoder_t(cache, info, channels, frame_bytes);
        case JE_AUDIO_CODEC_VORBIS:
        {
            auto* decoder = new vorbis_stream_decoder_t(
                cache, info, channels, frame_bytes);
            if (!decoder->seek_to_pcm_byte(0))
            {
                delete decoder;
                return nullptr;
            }
            return decoder;
        }
        }

        return nullptr;
    }

    bool audio_payload_decode_all(
        je_file_t* cache,
        const audio_payload_info_t& info,
        int32_t channels,
        size_t frame_bytes,
        void** out_pcm,
        size_t* out_size)
    {
        if (frame_bytes == 0
            || info.m_decoded_pcm_size % frame_bytes != 0)
            return false;

        std::unique_ptr<audio_stream_decoder_t> decoder(
            create_audio_stream_decoder(cache, info, channels, frame_bytes));
        if (decoder == nullptr)
            return false;

        if (!decoder->seek_to_pcm_byte(0))
            return false;

        const size_t size = static_cast<size_t>(info.m_decoded_pcm_size);
        void* const pcm = malloc(size > 0 ? size : 1);
        if (pcm == nullptr)
            return false;

        size_t total = 0;
        while (total < size)
        {
            const size_t got = decoder->read(
                static_cast<uint8_t*>(pcm) + total, size - total);
            if (got == 0)
                break;

            total += got;
        }

        if (total != size)
        {
            free(pcm);
            return false;
        }

        *out_pcm = pcm;
        *out_size = size;
        return true;
    }

    bool audio_cache_payload_write(
        je_writing_cache_file_t* file,
        const void* pcm,
        size_t pcm_len,
        int32_t channels,
        int32_t sample_rate,
        int32_t bits,
        bool is_float,
        je_audio_codec_t codec,
        float quality)
    {
        if (file == nullptr || (pcm == nullptr && pcm_len > 0))
            return false;
        if (channels != 1 && channels != 2)
        {
            debug::logerr("Unsupported audio cache channel count: %d.", (int)channels);
            return false;
        }
        if (sample_rate <= 0)
        {
            debug::logerr("Invalid audio cache sample rate: %d.", (int)sample_rate);
            return false;
        }
        if (bits <= 0 || bits % 8 != 0)
        {
            debug::logerr("Invalid audio cache bit depth: %d.", (int)bits);
            return false;
        }

        const size_t frame_bytes
            = static_cast<size_t>(channels) * static_cast<size_t>(bits / 8);
        if (pcm_len % frame_bytes != 0)
        {
            debug::logerr("Audio cache PCM length is not frame-aligned.");
            return false;
        }

        switch (codec)
        {
        case JE_AUDIO_CODEC_PCM:
            return write_u32(file, JE_AUDIO_CODEC_PCM)
                && write_u64(file, static_cast<uint64_t>(pcm_len))
                && write_exact(file, pcm, pcm_len);

        case JE_AUDIO_CODEC_ADPCM:
            return adpcm_encode_payload(file, pcm, pcm_len, channels, bits, is_float);

        case JE_AUDIO_CODEC_VORBIS:
            return vorbis_encode_payload(
                file, pcm, pcm_len, channels, sample_rate, bits, is_float, quality);
        }

        debug::logerr("Unsupported audio cache codec: %d.", (int)codec);
        return false;
    }
}

namespace jeecs::audio
{
    /* OPTIONAL */ je_file_t* open_or_create_cache(const char* path)
    {
        // The cache carries both the decoded PCM and the streaming flag from
        // the optional <path>.je4audio source, so the key chains both files:
        // a change to either invalidates it. With neither file present
        // (exported runtimes ship only the cache) the key is 0 and validation
        // is skipped.
        const uint64_t crc64_key = je_file_crc64(
            (std::string(path) + ".je4audio").c_str(),
            je_file_crc64(path, 0));

        if (je_file_t* file = je_cache_file_load(path, AUDIO_CACHE_VERSION, crc64_key))
            return file;

        return internal_script::regenerate_audio_cache(path, crc64_key);
    }
}
