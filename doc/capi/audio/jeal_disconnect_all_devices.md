# jeal_disconnect_all_devices

## 函数签名

```cpp
JE_API void jeal_disconnect_all_devices();
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
jeal_disconnect_all_devices();

// 恢复时重新枚举设备
jeal_enumerate_devices_and_do(device_selector, nullptr);
```

## 注意事项

- 正常情况下不需要调用这个接口
- 这个接口是为一些移动端平台，在程序切至后台时，需要暂停（终止）所有音频的播放
- 使用这个接口可以彻底终止所有音频效果
- 恢复时，使用 `jeal_enumerate_devices_and_do` 重新枚举所有设备以恢复播放

## 相关接口

- [jeal_enumerate_devices_and_do](jeal_enumerate_devices_and_do.md) - 枚举音频设备
