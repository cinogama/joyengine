# je_audio_devices_disconnect_all

## 函数签名

```cpp
JE_API void je_audio_devices_disconnect_all();
```

## 描述

关闭所有现有的播放设备，如同没有枚举到任何播放设备一般。

## 参数

无

## 返回值

无

## 用法示例

```cpp
// 程序切至后台时停止所有音频
je_audio_devices_disconnect_all();

// 恢复时重新枚举设备
je_audio_devices_enumerate_and_do(device_selector, nullptr);
```

## 注意事项

- 正常情况下不需要调用这个接口
- 这个接口是为一些移动端平台，在程序切至后台时，需要暂停（终止）所有音频的播放
- 使用这个接口可以彻底终止所有音频效果
- 恢复时，使用 `je_audio_devices_enumerate_and_do` 重新枚举所有设备以恢复播放

## 相关接口

- [je_audio_devices_enumerate_and_do](je_audio_devices_enumerate_and_do.md) - 枚举音频设备
