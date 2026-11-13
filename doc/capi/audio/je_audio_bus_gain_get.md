# je_audio_bus_gain_get

## 函数签名

```cpp
JE_API float je_audio_bus_gain_get(const je_audio_bus_t* bus);
```

## 描述

获取音量总线的当前增益。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| bus | const je_audio_bus_t* | 由 `je_audio_bus_get` 取得的总线指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| float | 总线增益；新建总线为 1.0 |

## 用法示例

```cpp
je_audio_bus_t* sfx = je_audio_bus_get("SFX");
float current = je_audio_bus_gain_get(sfx);
```

## 注意事项

- 总线指针进程内稳定，无需释放
- 该接口会与声源增益提交在同一把声源锁下读取，可在任意线程调用

## 相关接口

- [je_audio_bus_get](je_audio_bus_get.md) - 取得音量总线
- [je_audio_bus_gain_set](je_audio_bus_gain_set.md) - 设置总线增益
