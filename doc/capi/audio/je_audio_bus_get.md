# je_audio_bus_get

## 函数签名

```cpp
JE_API je_audio_bus_t* je_audio_bus_get(const char* name);
```

## 描述

按名称取得（必要时创建）一个音量总线。

音量总线用于按类别（如 `"SFX"` / `"BGM"` / `"UI"`）统一调整音量。总线在进程内唯一、指针稳定，不需要释放；第一次访问某个名称时自动创建，增益初始为 `1.0`。

声源的最终增益为：

```
声源增益 × 总线增益 × 监听者增益
```

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| name | const char* | 总线名称，不能为空字符串 |

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_bus_t* | 总线实例指针（进程内稳定，无需释放） |

### je_audio_bus_t 结构体

```cpp
struct je_audio_bus_t {
    const char* m_name; // 总线名称，进程内稳定
    float m_gain;       // 总线增益，默认 1.0
};
```

## 用法示例

```cpp
je_audio_bus_t* sfx = je_audio_bus_get("SFX");
je_audio_source_bus_set(source, sfx);
je_audio_bus_gain_set(sfx, 0.8f);

// 背景音乐走另一条总线
je_audio_bus_t* bgm = je_audio_bus_get("BGM");
je_audio_source_bus_set(music_source, bgm);
```

## 注意事项

- 总线没有关闭接口：它随进程存活，重复调用总是返回同一实例
- 组件 `Audio::Source` 的 `bus` 字段按名称解析到同一套总线

## 相关接口

- [je_audio_bus_gain_get](je_audio_bus_gain_get.md) - 获取总线增益
- [je_audio_bus_gain_set](je_audio_bus_gain_set.md) - 设置总线增益
- [je_audio_source_bus_set](je_audio_source_bus_set.md) - 设置声源所属总线
