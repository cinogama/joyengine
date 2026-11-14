# je_audio_oneshot_play

## 函数签名

```cpp
JE_API bool je_audio_oneshot_play(const je_audio_oneshot_desc_t* desc);
```

## 描述

按描述播放一次性音频（fire-and-forget）。引擎分配临时声部，播放结束后自动回收，适合脚步、金币、弹壳、受击等事件音——不需要创建实体或持有声源句柄。

同时最多存在 32 个一次性声部，超出时回收最早创建的声部。`desc->m_lowpass_hf < 1` 时自动为该声部挂一个低通滤波器模拟闷音。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| desc | const je_audio_oneshot_desc_t* | 播放描述，不可为空 |

### je_audio_oneshot_desc_t 结构体

```cpp
struct je_audio_oneshot_desc_t {
    const je_audio_buffer_t* m_buffer; // 要播放的缓冲区
    je_audio_bus_t m_bus;              // 所属音量总线

    bool m_spatial;      // true = 按 m_location 空间化；false = 相对监听者
    float m_location[3]; // 空间化位置

    float m_gain;  // 音量增益
    float m_pitch; // 播放速度

    float m_reference_distance; // 参考距离
    float m_max_distance;       // 最大距离
    float m_rolloff;            // 衰减率

    float m_lowpass_hf; // 高频增益，1.0 = 不闷音
};
```

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 已提交播放返回 true；当前没有可用播放设备返回 false |

## 用法示例

```cpp
const je_audio_buffer_t* buffer = je_audio_buffer_load("@/sounds/coin.wav");

je_audio_oneshot_desc_t desc;
desc.m_buffer = buffer;
desc.m_bus = JE_AUDIO_BUS_SE;
desc.m_spatial = true;
desc.m_location[0] = 3.0f;
desc.m_location[1] = 1.5f;
desc.m_location[2] = 0.0f;
desc.m_gain = 1.0f;
desc.m_pitch = 1.0f;
desc.m_reference_distance = 1.0f;
desc.m_max_distance = 32.0f;
desc.m_rolloff = 1.0f;
desc.m_lowpass_hf = 1.0f;

je_audio_oneshot_play(&desc);
```

## 注意事项

- 播放期间声部持有缓冲区引用，调用方可以立即 `je_audio_buffer_close` 自己的引用
- 距离模型由监听者（`je_audio_listener_t::m_distance_model`）决定
- 全局一次性播放只播放直达声，不参与 `Audio::Occluder`、瓦片遮挡与 `Audio::ReverbZone`；需要场景遮挡/混响请使用实体 `Audio::Source`
- 流式缓冲区同样可用于一次性播放

## 相关接口

- [je_audio_buffer_load](je_audio_buffer_load.md) - 加载音频缓冲
- [je_audio_bus_gain_set](je_audio_bus_gain_set.md) - 设置总线增益（含总线枚举说明）
- [je_audio_listener_update](je_audio_listener_update.md) - 提交距离模型
