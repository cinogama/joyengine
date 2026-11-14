# je_audio_environment_reload

## 函数签名

```c
JE_API void je_audio_environment_reload(je_game_world_t* world);
```

## 描述

通知世界的音频系统重新加载 `<世界源路径>.je4cache` 中的音频环境。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

无返回值。

## 用法

```cpp
// 世界保存后旧 sidecar 已因世界 CRC 变化失效，让音频系统立刻重载/回退
je_audio_environment_reload(world);
```

## 注意事项

- 每次调用都会递增世界的音频环境重载代数；音频系统在下一帧检查代数并重新读取 sidecar。
- sidecar 缺失或版本不符时音频系统会自动回退到 `Audio::ReverbZone` / `Audio::Portal` 组件实时构建的环境图。
- `je_audio_environment_bake` 成功后会自动调用本接口；手动保存世界后编辑器也会调用它。

## 相关接口

- [je_audio_environment_bake](je_audio_environment_bake.md) - 烘焙音频环境 sidecar
- [je_audio_environment_generation_get](je_audio_environment_generation_get.md) - 查询重载代数
