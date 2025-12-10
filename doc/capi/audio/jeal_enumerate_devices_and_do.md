# jeal_enumerate_devices_and_do

## 函数签名

```cpp
JE_API void jeal_enumerate_devices_and_do(
    jeal_enumerate_device_callback_t callback, void* userdata);
```

## 描述

枚举获取当前所有已经链接的音频播放设备，并通过回调函数处理。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| callback | jeal_enumerate_device_callback_t | 回调函数 |
| userdata | void* | 传递给回调函数的用户数据 |

### 回调函数类型

```cpp
typedef const jeal_play_device* (*jeal_enumerate_device_callback_t)(
    const jeal_play_device* enumerated_devices,
    size_t enumerated_device_count,
    void* userdata);
```

### jeal_play_device 结构体

```cpp
struct jeal_play_device {
    jeal_native_play_device_instance* m_device_instance;
    const char* m_name;        // 设备名称，保证唯一
    bool m_active;             // 是否是当前使用的设备
    bool m_is_default;         // 是否是默认播放设备
    int m_max_auxiliary_sends; // 最大辅助发送数量
};
```

## 返回值

无

## 用法示例

```cpp
const jeal_play_device* device_selector(
    const jeal_play_device* devices,
    size_t count,
    void* userdata)
{
    // 打印所有设备
    for (size_t i = 0; i < count; ++i) {
        printf("设备 %zu: %s%s\n", 
            i, 
            devices[i].m_name,
            devices[i].m_is_default ? " (默认)" : "");
    }
    
    // 返回要使用的设备（返回 nullptr 表示不更改）
    return &devices[0];  // 使用第一个设备
}

// 枚举设备
jeal_enumerate_devices_and_do(device_selector, nullptr);
```

## 注意事项

- 不应当将获取到的设备实例传递到回调函数作用域之外，无法保证实例在回调函数以外是否仍然有效
- 不要复制获取到的设备结构体
- 回调函数可以返回一个设备的地址，如果这么做，将指示音频使用此设备播放
- 返回 nullptr 表示不改变当前使用的设备

## 相关接口

- [jeal_disconnect_all_devices](jeal_disconnect_all_devices.md) - 断开所有设备
