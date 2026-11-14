# je_audio_bus_gain_get

## 函数签名

```cpp
JE_API float je_audio_bus_gain_get(je_audio_bus_t bus);
```

## 描述

获取音量总线的当前增益；`JE_AUDIO_BUS_NONE` 的增益恒为 `1.0`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| bus | je_audio_bus_t | 音量总线 |

## 返回值

| 类型 | 描述 |
|------|------|
| float | 总线增益；未被设置过的总线为 1.0 |

## 用法示例

```cpp
float current = je_audio_bus_gain_get(JE_AUDIO_BUS_SE);
```

## 注意事项

- 该接口会与声源增益提交在同一把声源锁下读取，可在任意线程调用

## 相关接口

- [je_audio_bus_gain_set](je_audio_bus_gain_set.md) - 设置总线增益
- [je_audio_source_bus_set](je_audio_source_bus_set.md) - 设置声源所属总线
