# je_audio_source_create

## 函数签名

```cpp
JE_API je_audio_source_t* je_audio_source_create();
```

## 描述

创建一个音频源。音频源用于播放音频缓冲区中的声音。

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
    bool m_loop;         // 是否循环播放
    float m_gain;        // 音量增益
    float m_pitch;       // 播放速度
    float m_location[3]; // 声音位置
    float m_velocity[3]; // 声源自身速度（非播放速度）
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

## 相关接口

- [je_audio_source_close](je_audio_source_close.md) - 释放音频源
- [je_audio_source_update](je_audio_source_update.md) - 应用音频源属性更改
- [je_audio_source_buffer_set](je_audio_source_buffer_set.md) - 设置音频源的缓冲区
- [je_audio_source_play](je_audio_source_play.md) - 播放音频源
