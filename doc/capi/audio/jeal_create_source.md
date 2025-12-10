# jeal_create_source

## 函数签名

```cpp
JE_API jeal_source* jeal_create_source();
```

## 描述

创建一个音频源。音频源用于播放音频缓冲区中的声音。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_source* | 指向音频源的指针 |

### jeal_source 结构体

```cpp
struct jeal_source {
    jeal_native_source_instance* m_source_instance;
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
jeal_source* source = jeal_create_source();

// 设置属性
source->m_loop = true;      // 循环播放
source->m_gain = 0.8f;      // 80% 音量
source->m_pitch = 1.0f;     // 正常播放速度
source->m_location[0] = 0;  // X 位置
source->m_location[1] = 0;  // Y 位置
source->m_location[2] = 0;  // Z 位置

// 应用属性更改
jeal_update_source(source);

// 设置要播放的音频
jeal_set_source_buffer(source, buffer);

// 播放
jeal_play_source(source);

// 清理
jeal_close_source(source);
```

## 注意事项

- 使用完毕后需要调用 `jeal_close_source` 释放资源
- 修改 source 结构体的属性后，需要调用 `jeal_update_source` 使更改生效

## 相关接口

- [jeal_close_source](jeal_close_source.md) - 释放音频源
- [jeal_update_source](jeal_update_source.md) - 应用音频源属性更改
- [jeal_set_source_buffer](jeal_set_source_buffer.md) - 设置音频源的缓冲区
- [jeal_play_source](jeal_play_source.md) - 播放音频源
