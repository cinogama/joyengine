# je_audio_bus_gain_set

## 函数签名

```cpp
JE_API void je_audio_bus_gain_set(je_audio_bus_t* bus, float gain);
```

## 描述

设置音量总线的增益，并立即重新提交所有使用该总线的声源增益（最终 `AL_GAIN = 声源增益 × 总线增益`）。这适合实现设置菜单里的分类音量滑条。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| bus | je_audio_bus_t* | 由 `je_audio_bus_get` 取得的总线指针 |
| gain | float | 新的总线增益，常用范围 `[0, 1]` |

## 返回值

无

## 用法示例

```cpp
je_audio_bus_t* bgm = je_audio_bus_get("BGM");
je_audio_bus_gain_set(bgm, 0.5f); // 背景音乐整体降到 50%
```

## 注意事项

- 总线增益与声源增益、监听者增益相乘；把总线增益设为 0 等价于静音该分类
- 使用该总线的一次性声部同样受影响

## 相关接口

- [je_audio_bus_get](je_audio_bus_get.md) - 取得音量总线
- [je_audio_bus_gain_get](je_audio_bus_gain_get.md) - 获取总线增益
