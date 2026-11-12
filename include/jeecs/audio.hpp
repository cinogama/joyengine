#pragma once

// ============================================================================
// jeecs/audio.hpp —— 音频：je_audio_* C API（OpenAL 语义）与 jeecs::audio C++ 封装（effect/effect_slot/filter/buffer/source/listener）
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/file.hpp"
#include "jeecs/basic.hpp"

JE_FORCE_CAPI

// Audio
typedef struct je_audio_native_play_device_instance_t je_audio_native_play_device_instance_t;
typedef struct je_audio_play_device_t
{
    je_audio_native_play_device_instance_t*
        m_device_instance;

    const char* m_name;        // 设备名称，实现保证唯一性
    bool m_active;             // 是否是当前使用的播放设备
    bool m_is_default;         // 是否是默认播放设备
    int m_max_auxiliary_sends; // 最大辅助发送数量，如果等于0，则不支持
} je_audio_play_device_t;

/*
je_audio_state_t [类型]
用于表示当前声源的播放状态，包括停止、播放中和暂停
*/
enum je_audio_state_t
{
    JE_AUDIO_STATE_STOPPED,
    JE_AUDIO_STATE_PLAYING,
    JE_AUDIO_STATE_PAUSED,
};

/*
je_audio_format_t [类型]
用于表示波形的格式
*/
enum je_audio_format_t
{
    JE_AUDIO_FORMAT_MONO8,
    JE_AUDIO_FORMAT_MONO16,
    JE_AUDIO_FORMAT_MONO32F,
    JE_AUDIO_FORMAT_STEREO8,
    JE_AUDIO_FORMAT_STEREO16,
    JE_AUDIO_FORMAT_STEREO32F,
};

typedef struct je_audio_native_buffer_instance_t je_audio_native_buffer_instance_t;
typedef struct je_audio_buffer_t
{
    je_audio_native_buffer_instance_t*
        m_buffer_instance;

    mutable size_t m_references; // 引用计数

    const void* m_data;
    size_t m_size;
    size_t m_sample_rate;
    size_t m_sample_size;
    size_t m_byte_rate;
    je_audio_format_t m_format;
    bool m_streaming; // 来自 <音频路径>.je4audio 的流式播放配置，默认为 false
} je_audio_buffer_t;

typedef struct je_audio_native_source_instance_t je_audio_native_source_instance_t;
typedef struct je_audio_source_t
{
    je_audio_native_source_instance_t*
        m_source_instance;

    bool m_loop;         // 是否循环播放
    float m_gain;        // 音量增益
    float m_pitch;       // 播放速度
    float m_location[3]; // 声音位置
    float m_velocity[3]; // 声源自身速度（非播放速度）
} je_audio_source_t;

typedef struct je_audio_listener_t
{
    float m_gain;        // 音量增益，最终起效的是 m_gain * m_global_gain
    float m_global_gain; // 全局增益，最终起效的是 m_gain * m_global_gain
    float m_location[3]; // 监听器位置
    float m_velocity[3]; // 监听器自身速度（非播放速度）
    float m_forward[3];  // 监听器朝向（前方向）
    float m_upward[3];   // 监听器朝向（顶方向）
} je_audio_listener_t;

typedef struct je_audio_native_effect_slot_instance_t je_audio_native_effect_slot_instance_t;
typedef struct je_audio_effect_slot_t
{
    je_audio_native_effect_slot_instance_t*
        m_effect_slot_instance;

    size_t m_references; // 引用计数

    float m_gain; // 增益，默认值为 1.0，范围 [0.0, 1.0]
} je_audio_effect_slot_t;

/*
je_audio_effect_reverb_t [类型]
混响效果
模拟声音在封闭空间（如房间、大厅）中的反射效果，增加空间感。
*/
typedef struct je_audio_effect_reverb_t
{
    float m_density;                // 密度, 默认值为 1.0，范围 [0.0, 1.0]
    float m_diffusion;              // 扩散度, 默认值为 1.0，范围 [0.0, 1.0]
    float m_gain;                   // 增益, 默认值为 0.32，范围 [0.0, 1.0]
    float m_gain_hf;                // 高频增益, 默认值为 0.89，范围 [0.0, 1.0]
    float m_decay_time;             // 衰减时间, 默认值为 1.49，范围 [0.1, 20.0]
    float m_decay_hf_ratio;         // 高频衰减比率, 默认值为 0.83，范围 [0.1, 2.0]
    float m_reflections_gain;       // 反射增益, 默认值为 0.05，范围 [0.0, 3.16]
    float m_reflections_delay;      // 反射延迟, 默认值为 0.007，范围 [0.0, 0.3]
    float m_late_reverb_gain;       // 后混响增益, 默认值为 1.26，范围 [0.0, 10.0]
    float m_late_reverb_delay;      // 后混响延迟, 默认值为 0.011，范围 [0.0, 0.1]
    float m_air_absorption_gain_hf; // 高频吸收, 默认值为 0.994，范围 [0.892, 1.0]
    float m_room_rolloff_factor;    // 房间衰减因子, 默认值为 0.0，范围 [0.0, 10.0]
    bool m_decay_hf_limit;          // 高频衰减限制器, 默认值为 false
} je_audio_effect_reverb_t;

/*
je_audio_effect_chorus_waveform_t [枚举]
合唱效果的波形类型
*/
typedef enum je_audio_effect_chorus_waveform_t
{
    JE_AUDIO_EFFECT_CHORUS_WAVEFORM_SINUSOID = 0,
    JE_AUDIO_EFFECT_CHORUS_WAVEFORM_TRIANGLE = 1,
} je_audio_effect_chorus_waveform_t;

/*
je_audio_effect_chorus_t [类型]
合唱效果
通过轻微延迟和音高变化复制原始声音，产生多个声部同时发声的合唱效果。
*/
typedef struct je_audio_effect_chorus_t
{
    je_audio_effect_chorus_waveform_t m_waveform; // 波形类型，默认值为 SINUSOID
    int m_phase;         // 相位，默认值为 90，范围 [-180, 180]
    float m_rate;        // 速率，默认值为 1.1，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认值为 0.1，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认值为 0.25，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认值为 0.16，范围 [0.0, 0.016]
} je_audio_effect_chorus_t;

/*
je_audio_effect_distortion_t [类型]
失真效果
故意扭曲音频波形，常见于电吉他等乐器，制造粗糙或过载的音色。
*/
typedef struct je_audio_effect_distortion_t
{
    float m_edge;                  // 边缘，默认值为 0.2，范围 [0.0, 1.0]
    float m_gain;                  // 增益，默认值为 0.05，范围 [0.01, 1.0]
    float m_lowpass_cutoff;        // 低通截止频率，默认值为 8000.0，范围 [80.0, 24000.0]
    float m_equalizer_center_freq; // 均衡器中心频率，默认值为 3600.0，范围 [80.0, 24000.0]
    float m_equalizer_bandwidth;   // 均衡器带宽，默认值为 3600.0，范围 [80.0, 24000.0]
} je_audio_effect_distortion_t;

/*
je_audio_effect_echo_t [类型]
回声效果
模拟声音在远处反射后延迟返回的效果（如山谷中的回声）。
*/
typedef struct je_audio_effect_echo_t
{
    float m_delay;    // 延迟时间，默认值为 0.1，范围 [0.0, 0.207]
    float m_lr_delay; // 左声道延迟，默认值为 0.1，范围 [0.0, 0.404]
    float m_damping;  // 阻尼，默认值为 0.5，范围 [0.0, 0.99]
    float m_feedback; // 反馈，默认值为 0.5，范围 [0.0, 1.0]
    float m_spread;   // 扩散，默认值为 -1.0，范围 [-1.0, 1.0]
} je_audio_effect_echo_t;

/*
je_audio_effect_flanger_waveform_t [类型别名]
镶边效果使用与合唱效果相同的波形类型
*/
typedef je_audio_effect_chorus_waveform_t je_audio_effect_flanger_waveform_t;

/*
je_audio_effect_flanger_t [类型]
镶边效果
通过混合延迟时间变化的原始信号，产生类似“喷气机”或太空感的波动音效
*/
typedef struct je_audio_effect_flanger_t
{
    je_audio_effect_flanger_waveform_t m_waveform; // 波形类型，默认值为 TRIANGLE
    int m_phase;         // 相位，默认值为 0，范围 [-180, 180]
    float m_rate;        // 速率，默认值为 0.27，范围 [0.0, 10.0]
    float m_depth;       // 深度，默认值为 1.0，范围 [0.0, 1.0]
    float m_feedback;    // 反馈，默认值为 -0.5，范围 [-1.0, 1.0]
    float m_delay;       // 延迟，默认值为 0.002，范围 [0.0, 0.004]
} je_audio_effect_flanger_t;

/*
je_audio_effect_frequency_shifter_direction_t [枚举]
移频器的方向
*/
typedef enum je_audio_effect_frequency_shifter_direction_t
{
    JE_AUDIO_EFFECT_FREQUENCY_SHIFTER_DIRECTION_DOWN = 0,
    JE_AUDIO_EFFECT_FREQUENCY_SHIFTER_DIRECTION_UP = 1,
    JE_AUDIO_EFFECT_FREQUENCY_SHIFTER_DIRECTION_OFF = 2,
} je_audio_effect_frequency_shifter_direction_t;

/*
je_audio_effect_frequency_shifter_t [类型]
移频器
改变输入信号的频率，用于创造科幻或非自然的声音效果。
*/
typedef struct je_audio_effect_frequency_shifter_t
{
    float m_frequency;           // 频率，默认值为 0.0，范围 [0.0, 24000.0]
    je_audio_effect_frequency_shifter_direction_t m_left_direction;  // 左声道方向，默认值为 DOWN
    je_audio_effect_frequency_shifter_direction_t m_right_direction; // 右声道方向，默认值为 DOWN
} je_audio_effect_frequency_shifter_t;

/*
je_audio_effect_vocal_morpher_phoneme_t [枚举]
人声变声器的元音音素
*/
typedef enum je_audio_effect_vocal_morpher_phoneme_t
{
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_A = 0,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_E,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_I,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_O,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_U,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_AA,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_AE,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_AH,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_AO,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_EH,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_ER,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_IH,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_IY,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_UH,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_UW,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_B,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_D,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_F,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_G,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_J,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_K,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_L,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_M,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_N,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_P,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_R,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_S,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_T,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_V,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_PHONEME_Z,
} je_audio_effect_vocal_morpher_phoneme_t;

/*
je_audio_effect_vocal_morpher_waveform_t [枚举]
人声变声器的波形类型
*/
typedef enum je_audio_effect_vocal_morpher_waveform_t
{
    JE_AUDIO_EFFECT_VOCAL_MORPHER_WAVEFORM_SINUSOID = 0,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_WAVEFORM_TRIANGLE = 1,
    JE_AUDIO_EFFECT_VOCAL_MORPHER_WAVEFORM_SAWTOOTH = 2,
} je_audio_effect_vocal_morpher_waveform_t;

/*
je_audio_effect_vocal_morpher_t [类型]
人声变声器
调制人声的某些频段，实现机器人声、电话音效等特殊变声效果。
*/
typedef struct je_audio_effect_vocal_morpher_t
{
    je_audio_effect_vocal_morpher_phoneme_t m_phoneme_a;           // 元音a，默认值为 A
    int m_phoneme_a_coarse_tuning; // 元音a粗调，默认值为 0，范围 [-24, 24]
    je_audio_effect_vocal_morpher_phoneme_t m_phoneme_b;           // 元音b，默认值为 ER
    int m_phoneme_b_coarse_tuning; // 元音b粗调，默认值为 0，范围 [-24, 24]
    je_audio_effect_vocal_morpher_waveform_t m_waveform;           // 波形类型，默认值为 SINUSOID
    float m_rate;                  // 速率，默认值为 1.41，范围 [0.0, 10.0]
} je_audio_effect_vocal_morpher_t;

/*
je_audio_effect_pitch_shifter_t [类型]
移调器
实时调整音高（如升高或降低音调），不改变播放速度。
*/
typedef struct je_audio_effect_pitch_shifter_t
{
    int m_coarse_tune; // 粗调，默认值为 12，范围 [-12, 12]
    int m_fine_tune;   // 细调，默认值为 0，范围 [-50, 50]
} je_audio_effect_pitch_shifter_t;

/*
je_audio_effect_ring_modulator_waveform_t [枚举]
环形调制器的波形类型
*/
typedef enum je_audio_effect_ring_modulator_waveform_t
{
    JE_AUDIO_EFFECT_RING_MODULATOR_WAVEFORM_SINUSOID = 0,
    JE_AUDIO_EFFECT_RING_MODULATOR_WAVEFORM_SAWTOOTH = 1,
    JE_AUDIO_EFFECT_RING_MODULATOR_WAVEFORM_SQUARE = 2,
} je_audio_effect_ring_modulator_waveform_t;

/*
je_audio_effect_ring_modulator_t [类型]
环形调制器
用输入信号与载波频率混合，产生金属感或机器人般的音效。
*/
typedef struct je_audio_effect_ring_modulator_t
{
    float m_frequency;       // 频率，默认值为 440.0，范围 [0.0, 8000.0]
    float m_highpass_cutoff; // 高通截止频率，默认值为 800.0，范围 [0.0, 24000.0]
    je_audio_effect_ring_modulator_waveform_t m_waveform;     // 波形类型，默认值为 SINUSOID
} je_audio_effect_ring_modulator_t;

/*
je_audio_effect_autowah_t [类型]
自动哇音效果
根据输入音量动态调整滤波器频率，产生“哇哇”声（常用于吉他）。
*/
typedef struct je_audio_effect_autowah_t
{
    float m_attack_time;  // 攻击时间，默认值为 0.06，范围 [0.0001, 1.0]
    float m_release_time; // 释放时间，默认值为 0.06，范围 [0.0001, 1.0]
    float m_resonance;    // 共鸣，默认值为 1000.0，范围 [2.0, 1000]
    float m_peak_gain;    // 峰值增益，默认值为 11.22，范围 [0.00003, 31621.0]
} je_audio_effect_autowah_t;

/*
je_audio_effect_compressor_t [类型]
压缩器
减小音频的动态范围，使安静部分更响亮、响亮部分更柔和，平衡音量。
*/
typedef struct je_audio_effect_compressor_t
{
    bool m_enabled; // 是否启用，默认值为 true
} je_audio_effect_compressor_t;

/*
je_audio_effect_equalizer_t [类型]
均衡器
调整音频频率响应，增强或削弱特定频段的音量，改善音质。
*/
typedef struct je_audio_effect_equalizer_t
{
    float m_low_gain;    // 低频增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_low_cutoff;  // 低频截止频率，默认值为 200.0，范围 [50.0, 800.0]
    float m_mid1_gain;   // 中频1增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_mid1_center; // 中频1中心频率，默认值为 500.0，范围 [200.0, 3000.0]
    float m_mid1_width;  // 中频1带宽，默认值为 1.0，范围 [0.01, 1.0]
    float m_mid2_gain;   // 中频2增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_mid2_center; // 中频2中心频率，默认值为 3000.0，范围 [1000.0, 8000.0]
    float m_mid2_width;  // 中频2带宽，默认值为 1.0，范围 [0.01, 1.0]
    float m_high_gain;   // 高频增益，默认值为 1.0，范围 [0.126, 7.943]
    float m_high_cutoff; // 高频截止频率，默认值为 6000.0，范围 [4000.0, 160000.0]
} je_audio_effect_equalizer_t;

/*
je_audio_effect_eax_reverb_t [类型]
拓展EAX混响效果
扩展的混响效果，支持更复杂的环境模拟（如洞穴、水下），需兼容EAX的硬件支持。
*/
typedef struct je_audio_effect_eax_reverb_t
{
    float m_density;                // 密度，默认值为 1.0，范围 [0.0, 1.0]
    float m_diffusion;              // 扩散度，默认值为 1.0，范围 [0.0, 1.0]
    float m_gain;                   // 增益，默认值为 0.32，范围 [0.0, 1.0]
    float m_gain_hf;                // 高频增益，默认值为 0.89，范围 [0.0, 1.0]
    float m_gain_lf;                // 低频增益，默认值为 0.0，范围 [0.0, 1.0]
    float m_decay_time;             // 衰减时间，默认值为 1.49，范围 [0.1, 20.0]
    float m_decay_hf_ratio;         // 高频衰减比率，默认值为 0.83，范围 [0.1, 2.0]
    float m_decay_lf_ratio;         // 低频衰减比率，默认值为 1.0，范围 [0.1, 2.0]
    float m_reflections_gain;       // 反射增益，默认值为 0.05，范围 [0.0, 3.16]
    float m_reflections_delay;      // 反射延迟，默认值为 0.007，范围 [0.0, 0.3]
    float m_reflections_pan_xyz[3]; // 反射声道位置，默认值为 {0.0, 0.0, 0.0}
    float m_late_reverb_gain;       // 后混响增益，默认值为 1.26，范围 [0.0, 10.0]
    float m_late_reverb_delay;      // 后混响延迟，默认值为 0.011，范围 [0.0, 0.1]
    float m_late_reverb_pan_xyz[3]; // 后混响声道位置，默认值为 {0.0, 0.0, 0.0}
    float m_echo_time;              // 回声时间，默认值为 0.25，范围 [0.075, 0.25]
    float m_echo_depth;             // 回声深度，默认值为 0.0，范围 [0.0, 1.0]
    float m_modulation_time;        // 调制时间，默认值为 0.25，范围 [0.04, 4.0]
    float m_modulation_depth;       // 调制深度，默认值为 0.0，范围 [0.0, 1.0]
    float m_air_absorption_gain_hf; // 高频吸收，默认值为 0.994，范围 [0.892, 1.0]
    float m_hf_reference;           // 高频反射，默认值为 5000.0，范围 [1000.0, 20000.0]
    float m_lf_reference;           // 低频反射，默认值为 250.0，范围 [20.0, 1000.0]
    float m_room_rolloff_factor;    // 房间衰减因子，默认值为 0.0，范围 [0.0, 10.0]
    bool m_decay_hf_limit;          // 高频衰减限制器，默认值为 true
} je_audio_effect_eax_reverb_t;

/*
je_audio_buffer_create [基本接口]
创建一个音频缓冲区
    * data 是音频数据的指针
    * buffer_data_len 是音频数据的长度（字节数）
    * sample_rate 是采样率（Hz）
    * format 是音频格式
    返回值是一个指向音频缓冲区的指针
    使用完毕后需要调用 je_audio_buffer_close 释放资源
参见：
    je_audio_buffer_close
*/
JE_API /* OPTIONAL: If format is invalid or buffer_data_len is not aligned to sample size */ const je_audio_buffer_t* je_audio_buffer_create(
    const void* data,
    size_t buffer_data_len,
    size_t sample_rate,
    je_audio_format_t format);

/*
je_audio_buffer_load [基本接口]
加载一个音频文件（按其 <音频路径>.je4cache 缓存，缓存缺失或失效时经
pkg::audio 解码源文件并生成；流式播放配置与缓存格式取自可选的
<音频路径>.je4audio，PCM 一律转换为缓存格式存储，缺省 8 位整数）
    * path 是音频文件的引擎路径（@/ 或 !/）
    * 返回值是一个指向音频缓冲区的指针
    * 相同路径的重复加载共享同一个缓冲区（引用计数递增）
使用完毕后需要调用 je_audio_buffer_close 释放资源
如果加载失败（文件/缓存不可用或格式不支持），返回值为 nullptr
参见：
    je_audio_buffer_close
    je_audio_cache_open_or_create
    je_audio_shared_buffer_mark_outdated
*/
JE_API /* OPTIONAL: If audio file or its cache failed to load */ const je_audio_buffer_t* je_audio_buffer_load(const char* path);

/*
je_audio_cache_open_or_create [基本接口]
打开音频文件的二进制缓存 <音频路径>.je4cache：缓存有效时直接返回，
缺失或失效时调用内置生成脚本（pkg::audio 解码 + 可选的
<音频路径>.je4audio 流式播放与缓存格式配置）重新生成
    * path 是音频文件的引擎路径（@/ 或 !/）
    * 仅支持单声道/立体声源（OpenAL 缓冲格式约束），生成期校验
    * 缓存格式 format = "i8"（缺省）/"i16"/"f32"，PCM 经 pkg::audio
      转换后存储（任何源编码都可落成 OpenAL 可用的缓存）
缓存文档结构（je_cache_binary_*）：
    channels / samplerate / bits -> INT32，is_float / streaming -> BOOL，
    pcm -> BUFFER（转换后的交织 PCM）
返回值是缓存文件的只读句柄（用毕 je_file_close 关闭），失败返回 nullptr
参见：
    je_audio_buffer_load
    je_cache_file_load
*/
JE_API /* OPTIONAL */ je_file_t* je_audio_cache_open_or_create(const char* path);

/*
je_audio_shared_buffer_mark_outdated [基本接口]
把 path 从音频缓冲区的内存共享缓存中移除：后续 je_audio_buffer_load 会
重新读取（必要时重新生成）<音频路径>.je4cache，已分发的旧缓冲区句柄
不受影响（由引用计数保持存活）
    * path 是音频文件的引擎路径（@/ 或 !/）
无返回值。用于编辑器修改 <音频路径>.je4audio 之后使新配置生效
参见：
    je_audio_buffer_load
*/
JE_API void je_audio_shared_buffer_mark_outdated(const char* path);

/*
je_audio_buffer_close [基本接口]
释放一个音频缓冲区
*/
JE_API void je_audio_buffer_close(const je_audio_buffer_t* buffer);


enum je_audio_filter_type_t
{
    JE_AUDIO_FILTER_LOWPASS,
    JE_AUDIO_FILTER_HIGHPASS,
    JE_AUDIO_FILTER_BANDPASS,
};

typedef struct je_audio_native_filter_instance_t je_audio_native_filter_instance_t;
typedef struct je_audio_filter_t
{
    je_audio_native_filter_instance_t*
        m_filter_instance;

    size_t m_references; // 引用计数

    // 滤波器类型, 默认值为 JE_AUDIO_FILTER_LOWPASS
    je_audio_filter_type_t    m_type;
    // 增益，默认值为 1.0，范围 [0.0, 1.0]
    float               m_gain;
    // 低频增益（对高通滤波器无效），0 表示完全压制低频部分，默认值为 0.5，范围 [0.0, 1.0]
    float               m_gain_lf;
    // 高频增益（对低通滤波器无效），0 表示完全压制高频部分，默认值为 0.5，范围 [0.0, 1.0]
    float               m_gain_hf;
} je_audio_filter_t;

/*
je_audio_filter_create [基本接口]
创建一个低通滤波器
*/
JE_API je_audio_filter_t* je_audio_filter_create();

/*
je_audio_filter_update [基本接口]
将对 je_audio_filter_t 的参数修改应用到实际的滤波器上
*/
JE_API void je_audio_filter_update(je_audio_filter_t* filter);

/*
je_audio_filter_close [基本接口]
关闭一个滤波器
*/
JE_API void je_audio_filter_close(je_audio_filter_t* filter);

/*
je_audio_source_create [基本接口]
创建一个音频源
    使用完毕后需要调用 je_audio_source_close 释放资源
参见：
    je_audio_source_close
*/
JE_API je_audio_source_t* je_audio_source_create();

/*
je_audio_source_update [基本接口]
将对 je_audio_source_t 的参数修改应用到实际的音频源上
*/
JE_API void je_audio_source_update(je_audio_source_t* source);

/*
je_audio_source_close [基本接口]
释放一个音频源
*/
JE_API void je_audio_source_close(je_audio_source_t* source);

/*
je_audio_source_buffer_set [基本接口]
设置音频源的播放音频
    只有音源设置有音频时，其他的播放等动作才会生效
    * 如果设置的音频与先前的音频不同，自动停止播放，返回 true
*/
JE_API bool je_audio_source_buffer_set(je_audio_source_t* source, const je_audio_buffer_t* buffer);

/*
je_audio_source_effect_slot_and_filter_set [基本接口]
设置音频源的效果槽和滤波器
    slot_idx 必须小于 jeecs::audio::MAX_AUXILIARY_SENDS
*/
JE_API void je_audio_source_effect_slot_and_filter_set(
    je_audio_source_t* source,
    size_t slot_idx,
    /* OPTIONAL */ je_audio_effect_slot_t* slot_may_null,
    /* OPTIONAL */ je_audio_filter_t* filter_may_null);

/*
je_audio_source_filter_set [基本接口]
设置音频源的滤波器
*/
JE_API void je_audio_source_filter_set(je_audio_source_t* source, /* OPTIONAL */ je_audio_filter_t* filter_may_null);

/*
je_audio_source_play [基本接口]
播放音频源
    如果音频源已经在播放或者音频源没有设置音频，则不会有任何效果
    如果音频源在此前被暂停，则继续播放
    如果音频源被停止，则从音频的起始位置开始播放
*/
JE_API void je_audio_source_play(je_audio_source_t* source);

/*
je_audio_source_pause [基本接口]
暂停音频源的播放
*/
JE_API void je_audio_source_pause(je_audio_source_t* source);

/*
je_audio_source_stop [基本接口]
停止音频源的播放
*/
JE_API void je_audio_source_stop(je_audio_source_t* source);

/*
je_audio_source_play_state_get [基本接口]
获取当前音频源的状态（是否播放、暂停或停止）
*/
JE_API je_audio_state_t je_audio_source_play_state_get(je_audio_source_t* source);

/*
je_audio_source_play_process_get [基本接口]
获取当前音频源的播放位置，单位是字节
*/
JE_API size_t je_audio_source_play_process_get(je_audio_source_t* source);

/*
je_audio_source_play_process_set [基本接口]
设置音频源的播放进度，单位是字节
    注意，设置的偏移量应当是目标播放音频的采样大小的整数倍；
    如果未对齐，会被强制对齐。
*/
JE_API void je_audio_source_play_process_set(je_audio_source_t* source, size_t offset);

/*
je_audio_listener_get [基本接口]
获取监听者的实例
*/
JE_API je_audio_listener_t* je_audio_listener_get();

/*
je_audio_listener_update [基本接口]
将对 je_audio_listener_t 的参数修改应用到实际的监听者上
*/
JE_API void je_audio_listener_update();

/*
je_audio_effect_slot_create [基本接口]
创建一个效果槽
    * 效果槽是一个用于存放效果的容器，效果槽可以被添加到声源上
    * 一个效果槽可以被添加到多个声源上
    * 一个声源亦可附加多个效果槽，但通常会受到平台限制，单个声源只能附加 1-4 个效果槽
*/
JE_API je_audio_effect_slot_t* je_audio_effect_slot_create();

/*
je_audio_effect_slot_bind [基本接口]
绑定一个效果到效果槽
    * 效果传入 nullptr 表示解除绑定
*/
JE_API void je_audio_effect_slot_bind(je_audio_effect_slot_t* slot, /* OPTIONAL */ void* effect_may_null);

/*
je_audio_effect_slot_close [基本接口]
关闭一个效果槽
*/
JE_API void je_audio_effect_slot_close(je_audio_effect_slot_t* slot);

/*
je_audio_effect_slot_update [基本接口]
将对 je_audio_effect_slot_t 的参数修改应用到实际的效果槽上
*/
JE_API void je_audio_effect_slot_update(je_audio_effect_slot_t* slot);

/*
je_audio_effect_reverb_create [基本接口]
创建一个混响效果实例
*/
JE_API je_audio_effect_reverb_t* je_audio_effect_reverb_create();

/*
je_audio_effect_chorus_create [基本接口]
创建一个合唱效果实例
*/
JE_API je_audio_effect_chorus_t* je_audio_effect_chorus_create();

/*
je_audio_effect_distortion_create [基本接口]
创建一个失真效果实例
*/
JE_API je_audio_effect_distortion_t* je_audio_effect_distortion_create();

/*
je_audio_effect_echo_create [基本接口]
创建一个回声效果实例
*/
JE_API je_audio_effect_echo_t* je_audio_effect_echo_create();

/*
je_audio_effect_flanger_create [基本接口]
创建一个镶边效果实例
*/
JE_API je_audio_effect_flanger_t* je_audio_effect_flanger_create();

/*
je_audio_effect_frequency_shifter_create [基本接口]
创建一个移频效果实例
*/
JE_API je_audio_effect_frequency_shifter_t* je_audio_effect_frequency_shifter_create();

/*
je_audio_effect_vocal_morpher_create [基本接口]
创建一个人声变形效果实例
*/
JE_API je_audio_effect_vocal_morpher_t* je_audio_effect_vocal_morpher_create();

/*
je_audio_effect_pitch_shifter_create [基本接口]
创建一个变调效果实例
*/
JE_API je_audio_effect_pitch_shifter_t* je_audio_effect_pitch_shifter_create();

/*
je_audio_effect_ring_modulator_create [基本接口]
创建一个环形调制效果实例
*/
JE_API je_audio_effect_ring_modulator_t* je_audio_effect_ring_modulator_create();

/*
je_audio_effect_autowah_create [基本接口]
创建一个自动哇音效果实例
*/
JE_API je_audio_effect_autowah_t* je_audio_effect_autowah_create();

/*
je_audio_effect_compressor_create [基本接口]
创建一个压缩效果实例
*/
JE_API je_audio_effect_compressor_t* je_audio_effect_compressor_create();

/*
je_audio_effect_equalizer_create [基本接口]
创建一个均衡效果实例
*/
JE_API je_audio_effect_equalizer_t* je_audio_effect_equalizer_create();

/*
je_audio_effect_eaxreverb_create [基本接口]
创建一个 EAX 混响效果实例
*/
JE_API je_audio_effect_eax_reverb_t* je_audio_effect_eaxreverb_create();

/*
je_audio_effect_close [基本接口]
关闭一个音频效果实例
*/
JE_API void je_audio_effect_close(void* effect);

/*
je_audio_effect_update [基本接口]
将对音频效果实例的参数修改应用到实际的效果上
    需要注意，对效果的更新不会应用到已经绑定的效果槽和音频源上，
    需要通过 je_audio_effect_slot_bind 重新绑定
参见：
    je_audio_effect_slot_bind
*/
JE_API void je_audio_effect_update(void* effect);

typedef const je_audio_play_device_t* (*
    je_audio_enumerate_device_callback_t)(
        const je_audio_play_device_t* enumerated_devices,
        size_t enumerated_device_count,
        void* userdata);

/*
je_audio_devices_enumerate_and_do [基本接口]
枚举获取当前所有已经链接的音频播放设备
    * 不应当将获取到的设备实例传递到回调函数作用域之外，无法保证实例在回调
        函数以外是否仍然有效；不要复制获取到的设备结构体。
    * 回调函数可以返回一个设备的地址，如果这么做，将指示音频使用此设备播放；
        返回 nullptr 表示不改变当前使用的设备
*/
JE_API void je_audio_devices_enumerate_and_do(
    je_audio_enumerate_device_callback_t callback, /* OPTIONAL */ void* userdata);

/*
je_audio_devices_disconnect_all [基本接口]
关闭所有现有的播放设备，如同没有枚举到任何播放设备一般
    * 正常情况下不需要调用这个接口，这个接口是为一些移动端平台，在程序切至
        后台时，需要暂停（终止）所有音频的播放；此时使用这个接口可以彻底终止
        所有音频效果
    * 恢复时，使用 je_audio_devices_enumerate_and_do 重新枚举所有设备以恢复播放
参见：
    je_audio_devices_enumerate_and_do
*/
JE_API void je_audio_devices_disconnect_all();
typedef je_audio_native_play_device_instance_t je_AudioNativePlayDeviceInstance;
typedef je_audio_play_device_t je_AudioPlayDevice;
typedef je_audio_state_t je_AudioState;
typedef je_audio_format_t je_AudioFormat;
typedef je_audio_native_buffer_instance_t je_AudioNativeBufferInstance;
typedef je_audio_buffer_t je_AudioBuffer;
typedef je_audio_native_source_instance_t je_AudioNativeSourceInstance;
typedef je_audio_source_t je_AudioSource;
typedef je_audio_listener_t je_AudioListener;
typedef je_audio_native_effect_slot_instance_t je_AudioNativeEffectSlotInstance;
typedef je_audio_effect_slot_t je_AudioEffectSlot;
typedef je_audio_effect_reverb_t je_AudioEffectReverb;
typedef je_audio_effect_chorus_t je_AudioEffectChorus;
typedef je_audio_effect_distortion_t je_AudioEffectDistortion;
typedef je_audio_effect_echo_t je_AudioEffectEcho;
typedef je_audio_effect_flanger_t je_AudioEffectFlanger;
typedef je_audio_effect_frequency_shifter_t je_AudioEffectFrequencyShifter;
typedef je_audio_effect_vocal_morpher_t je_AudioEffectVocalMorpher;
typedef je_audio_effect_pitch_shifter_t je_AudioEffectPitchShifter;
typedef je_audio_effect_ring_modulator_t je_AudioEffectRingModulator;
typedef je_audio_effect_autowah_t je_AudioEffectAutowah;
typedef je_audio_effect_compressor_t je_AudioEffectCompressor;
typedef je_audio_effect_equalizer_t je_AudioEffectEqualizer;
typedef je_audio_effect_eax_reverb_t je_AudioEffectEaxReverb;
typedef je_audio_filter_type_t je_AudioFilterType;
typedef je_audio_native_filter_instance_t je_AudioNativeFilterInstance;
typedef je_audio_filter_t je_AudioFilter;
typedef je_audio_enumerate_device_callback_t je_AudioEnumerateDeviceCallback;
typedef je_audio_effect_chorus_waveform_t je_AudioEffectChorusWaveform;
typedef je_audio_effect_flanger_waveform_t je_AudioEffectFlangerWaveform;
typedef je_audio_effect_frequency_shifter_direction_t je_AudioEffectFrequencyShifterDirection;
typedef je_audio_effect_vocal_morpher_phoneme_t je_AudioEffectVocalMorpherPhoneme;
typedef je_audio_effect_vocal_morpher_waveform_t je_AudioEffectVocalMorpherWaveform;
typedef je_audio_effect_ring_modulator_waveform_t je_AudioEffectRingModulatorWaveform;

JE_FORCE_CAPI_END
#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::audio [命名空间]
    此处定义引擎音频相关的接口、常量等
    */
    namespace audio
    {
        /*
        jeecs::audio::MAX_AUXILIARY_SENDS [常量]
        用于指示引擎支持的最大辅助发送数量（不等于设备支持的实际发射数量）
        */
        constexpr static size_t MAX_AUXILIARY_SENDS = 8;
    }
    namespace audio
    {
        template <typename T>
        class effect
        {
            JECS_DISABLE_MOVE_AND_COPY(effect);

            T* m_effect;

            effect(T* effect)
                : m_effect(effect)
            {
                assert(m_effect != nullptr);
            }

        public:
            ~effect()
            {
                je_audio_effect_close(m_effect);
            }

            static basic::resource<effect<T>> create()
            {
                T* instance;
                if constexpr (std::is_same_v<T, je_audio_effect_reverb_t>)
                    instance = je_audio_effect_reverb_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_chorus_t>)
                    instance = je_audio_effect_chorus_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_distortion_t>)
                    instance = je_audio_effect_distortion_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_echo_t>)
                    instance = je_audio_effect_echo_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_flanger_t>)
                    instance = je_audio_effect_flanger_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_frequency_shifter_t>)
                    instance = je_audio_effect_frequency_shifter_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_vocal_morpher_t>)
                    instance = je_audio_effect_vocal_morpher_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_pitch_shifter_t>)
                    instance = je_audio_effect_pitch_shifter_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_ring_modulator_t>)
                    instance = je_audio_effect_ring_modulator_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_autowah_t>)
                    instance = je_audio_effect_autowah_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_compressor_t>)
                    instance = je_audio_effect_compressor_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_equalizer_t>)
                    instance = je_audio_effect_equalizer_create();
                else if constexpr (std::is_same_v<T, je_audio_effect_eax_reverb_t>)
                    instance = je_audio_effect_eaxreverb_create();
                else
                    static_assert(
                        std::is_void_v<T> && !std::is_void_v<T> /* false */,
                        "Unsupported effect type");

                return basic::resource<effect<T>>(new effect<T>(instance));
            }

            T* handle() const
            {
                return m_effect;
            }
            void update(const std::function<void(T*)>& func)
            {
                func(m_effect);
                je_audio_effect_update(m_effect);
            }
        };
        class effect_slot
        {
            JECS_DISABLE_MOVE_AND_COPY(effect_slot);
            je_audio_effect_slot_t* m_effect_slot;
            effect_slot(je_audio_effect_slot_t* effect_slot)
                : m_effect_slot(effect_slot)
            {
                assert(m_effect_slot != nullptr);
            }

        public:
            ~effect_slot()
            {
                je_audio_effect_slot_close(m_effect_slot);
            }
            template <typename T>
            void bind_effect(const basic::resource<effect<T>>& effect)
            {
                je_audio_effect_slot_bind(m_effect_slot, effect->handle());
            }

            static basic::resource<effect_slot> create()
            {
                auto* slot = je_audio_effect_slot_create();
                assert(slot != nullptr);
                return basic::resource<effect_slot>(new effect_slot(slot));
            }
            je_audio_effect_slot_t* handle() const
            {
                return m_effect_slot;
            }
            void update(const std::function<void(je_audio_effect_slot_t*)>& func)
            {
                func(m_effect_slot);
                je_audio_effect_slot_update(m_effect_slot);
            }
        };
        class filter
        {
            JECS_DISABLE_MOVE_AND_COPY(filter);

            je_audio_filter_t* _m_audio_filter;

            filter(je_audio_filter_t* audio_filter)
                : _m_audio_filter(audio_filter)
            {
                assert(_m_audio_filter != nullptr);
            }
        public:
            ~filter()
            {
                je_audio_filter_close(_m_audio_filter);
            }

            inline static basic::resource<filter> create()
            {
                return basic::resource<filter>(new filter(je_audio_filter_create()));
            }

            je_audio_filter_t* handle() const
            {
                return _m_audio_filter;
            }
            void update(const std::function<void(je_audio_filter_t*)>& func)
            {
                func(_m_audio_filter);
                je_audio_filter_update(_m_audio_filter);
            }
        };
        class buffer
        {
            JECS_DISABLE_MOVE_AND_COPY(buffer);

            const je_audio_buffer_t* _m_audio_buffer;
            buffer(const je_audio_buffer_t* audio_buffer)
                : _m_audio_buffer(audio_buffer)
            {
                assert(_m_audio_buffer != nullptr);
            }

        public:
            ~buffer()
            {
                je_audio_buffer_close(_m_audio_buffer);
            }
            inline static std::optional<basic::resource<buffer>> create(
                const void* data,
                size_t length,
                size_t samplerate,
                je_audio_format_t format)
            {
                auto* buf = je_audio_buffer_create(data, length, samplerate, format);
                if (buf != nullptr)
                    return basic::resource<buffer>(new buffer(buf));
                return std::nullopt;
            }
            inline static std::optional<basic::resource<buffer>> load(const std::string& path)
            {
                auto* buf = je_audio_buffer_load(path.c_str());
                if (buf != nullptr)
                    return basic::resource<buffer>(new buffer(buf));
                return std::nullopt;
            }
            const je_audio_buffer_t* handle() const
            {
                return _m_audio_buffer;
            }
        };
        class source
        {
            JECS_DISABLE_MOVE_AND_COPY(source);

            je_audio_source_t* _m_audio_source;

            source(je_audio_source_t* audio_source)
                : _m_audio_source(audio_source)
            {
                assert(_m_audio_source != nullptr);
            }

        public:
            ~source()
            {
                stop();
                je_audio_source_close(_m_audio_source);
            }
            inline je_audio_state_t get_state() const
            {
                return je_audio_source_play_state_get(_m_audio_source);
            }
            inline static basic::resource<source> create()
            {
                return basic::resource<source>(new source(je_audio_source_create()));
            }
            inline bool set_playing_buffer(const basic::resource<buffer>& buffer)
            {
                return je_audio_source_buffer_set(_m_audio_source, buffer->handle());
            }
            je_audio_source_t* handle() const
            {
                return _m_audio_source;
            }
            void play()
            {
                je_audio_source_play(_m_audio_source);
            }
            void pause()
            {
                je_audio_source_pause(_m_audio_source);
            }
            void stop()
            {
                je_audio_source_stop(_m_audio_source);
            }
            size_t get_playing_offset() const
            {
                return je_audio_source_play_process_get(_m_audio_source);
            }
            void set_playing_offset(size_t offset)
            {
                je_audio_source_play_process_set(_m_audio_source, offset);
            }
            void update(const std::function<void(je_audio_source_t*)>& func)
            {
                func(_m_audio_source);
                je_audio_source_update(_m_audio_source);
            }
            void set_filter(const std::optional<basic::resource<filter>>& filter)
            {
                je_audio_source_filter_set(
                    _m_audio_source,
                    filter.has_value() ? filter.value()->handle() : nullptr);
            }
            void bind_effect_slot(
                size_t pass,
                const std::optional<basic::resource<effect_slot>>& slot,
                const std::optional<basic::resource<filter>>& filter)
            {
                if (pass < MAX_AUXILIARY_SENDS)
                    je_audio_source_effect_slot_and_filter_set(
                        _m_audio_source,
                        pass,
                        slot.has_value() ? slot.value()->handle() : nullptr,
                        filter.has_value() ? filter.value()->handle() : nullptr);
            }
        };
        class listener
        {
        public:
            static je_audio_listener_t* handle()
            {
                return je_audio_listener_get();
            }
            static void update(const std::function<void(je_audio_listener_t*)>& func)
            {
                func(je_audio_listener_get());
                je_audio_listener_update();
            }
        };
    }
}
#endif // __cplusplus
