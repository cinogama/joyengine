# je_audio_source_create

## 函数签名

```cpp
JE_API je_audio_source_t* je_audio_source_create();
```

## 描述

创建一个音频源。音频源用于播放音频缓冲区中的声音。

音频源是**虚拟**的：创建时不会占用播放设备的硬件声源（OpenAL 的 source），
只有实际处于播放状态的声源才会按需占用。因此游戏可以按需创建大量声源，
而不会触碰 OpenAL/OpenAL Soft 的声源数量上限（默认 256 个）；暂停、停止或
自然播放结束的声源会立即把硬件声源归还给引擎复用。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_source_t* | 指向音频源的指针 |

### je_audio_source_t 结构体

```cpp
struct je_audio_source_t {
    je_audio_native_source_instance_t* m_source_instance;
    bool m_loop;         // 是否循环播放，默认 false
    float m_gain;        // 音量增益，默认 1.0
    float m_pitch;       // 播放速度，默认 1.0
    float m_location[3]; // 声音位置
    float m_velocity[3]; // 声源自身速度（非播放速度）

    // 距离衰减参数（仅对非 relative 声源生效）
    float m_reference_distance; // 参考距离，默认 1.0
    float m_max_distance;       // 最大距离，默认 FLT_MAX
    float m_rolloff;            // 衰减率，默认 1.0
    float m_min_gain;           // 距离衰减下限，默认 0.0
    float m_max_gain;           // 距离衰减上限，默认 1.0

    bool m_relative;      // true = 相对监听者，默认 false

    // 定向声源的锥形参数，默认内外角 360°（无方向性）
    float m_direction[3];
    float m_cone_inner_angle;
    float m_cone_outer_angle;
    float m_cone_outer_gain;

    je_audio_bus_t m_bus; // 所属音量总线，默认 JE_AUDIO_BUS_NONE
};
```

## 用法示例

```cpp
// 创建音频源
je_audio_source_t* source = je_audio_source_create();

// 设置属性
source->m_loop = true;      // 循环播放
source->m_gain = 0.8f;      // 80% 音量
source->m_pitch = 1.0f;     // 正常播放速度
source->m_location[0] = 0;  // X 位置
source->m_location[1] = 0;  // Y 位置
source->m_location[2] = 0;  // Z 位置

// 应用属性更改
je_audio_source_update(source);

// 设置要播放的音频
je_audio_source_buffer_set(source, buffer);

// 播放
je_audio_source_play(source);

// 清理
je_audio_source_close(source);
```

## 注意事项

- 使用完毕后需要调用 `je_audio_source_close` 释放资源
- 修改 source 结构体的属性后，需要调用 `je_audio_source_update` 使更改生效
- 创建声源本身很轻量，不需要为了节省资源而复用声源

## 相关接口

- [je_audio_source_close](je_audio_source_close.md) - 释放音频源
- [je_audio_source_update](je_audio_source_update.md) - 应用音频源属性更改
- [je_audio_source_buffer_set](je_audio_source_buffer_set.md) - 设置音频源的缓冲区
- [je_audio_source_play](je_audio_source_play.md) - 播放音频源
