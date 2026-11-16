# je_audio_bus_gain_set

## 函数签名

```cpp
JE_API void je_audio_bus_gain_set(je_audio_bus_t bus, float gain);
```

## 描述

设置音量总线的增益，并立即重新提交所有使用该总线的声源增益（最终 `AL_GAIN = 声源增益 × 总线增益`）。这适合实现设置菜单里的分类音量滑条。

音量总线是预设的枚举分组，用于按类别统一调节音量：

```cpp
enum je_audio_bus_t
{
    JE_AUDIO_BUS_NONE,     // 不经过总线（增益恒为 1.0，不可设置）
    JE_AUDIO_BUS_BGM,      // 背景音乐
    JE_AUDIO_BUS_BGS,      // 背景环境声
    JE_AUDIO_BUS_ME,       // 音乐效果（胜利、升级等短乐句）
    JE_AUDIO_BUS_SE,       // 音效
    JE_AUDIO_BUS_VOICE,    // 语音
    JE_AUDIO_BUS_CUSTOM_0, // 自定义槽位
    JE_AUDIO_BUS_CUSTOM_1,
    JE_AUDIO_BUS_CUSTOM_2,
    JE_AUDIO_BUS_CUSTOM_3,
    JE_AUDIO_BUS_COUNT,    // 总线数量哨兵，不是有效总线
};
```

声源的最终增益为：

```
声源增益 × 总线增益 × 监听者增益
```

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| bus | je_audio_bus_t | 音量总线；不能为 `JE_AUDIO_BUS_NONE` 或 `JE_AUDIO_BUS_COUNT` |
| gain | float | 新的总线增益，常用范围 `[0, 1]` |

## 返回值

无

## 用法示例

```cpp
je_audio_bus_gain_set(JE_AUDIO_BUS_BGM, 0.5f); // 背景音乐整体降到 50%
```

## 注意事项

- 总线增益与声源增益、监听者增益相乘；把总线增益设为 0 等价于静音该分类
- 使用该总线的一次性声部同样受影响
- 所有总线增益在进程内共享，不参与播放设备的 dump/restore；进程启动时全部为 1.0

## 相关接口

- [je_audio_bus_gain_get](je_audio_bus_gain_get.md) - 获取总线增益
- [je_audio_source_bus_set](je_audio_source_bus_set.md) - 设置声源所属总线
