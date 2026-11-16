# je_audio_listener_update

## 函数签名

```cpp
JE_API void je_audio_listener_update();
```

## 描述

将对 `je_audio_listener_t` 的参数修改应用到实际的监听者上。

除位置、速度、朝向与增益外，此函数还会提交**距离模型**（`m_distance_model`）与**多普勒系数**（`m_doppler_factor`）——它们是 OpenAL 上下文级状态，作用于当前播放设备上的所有声源。设备热插拔或切换后，引擎会自动重新提交监听者状态。

## 参数

无

## 返回值

无

## 用法示例

```cpp
je_audio_listener_t* listener = je_audio_listener_get();

// 更新监听器位置（跟随玩家）
listener->m_location[0] = player_x;
listener->m_location[1] = player_y;
listener->m_location[2] = player_z;

// 2D 游戏常用的线性钳制衰减
listener->m_distance_model = JE_AUDIO_DISTANCE_MODEL_LINEAR_CLAMPED;
listener->m_doppler_factor = 0.0f; // 关闭多普勒

// 应用更改
je_audio_listener_update();
```

## 注意事项

- 直接修改 `je_audio_listener_t` 结构体的成员不会立即生效，需要调用此函数
- 距离模型与多普勒系数是设备级状态；切换到新设备后引擎会自动补提交

## 相关接口

- [je_audio_listener_get](je_audio_listener_get.md) - 获取监听器实例
- [je_audio_source_update](je_audio_source_update.md) - 提交声源的距离衰减参数
