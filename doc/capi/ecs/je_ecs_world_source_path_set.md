# je_ecs_world_source_path_set

## 函数签名

```c
JE_API void je_ecs_world_source_path_set(je_game_world_t* world, const char* path);
```

## 描述

设置世界的源文件路径（通常是该世界对应的 `.je4world` 路径）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |
| `path` | `const char*` | 世界文件路径；`nullptr` 或空字符串表示清除 |

## 返回值

无返回值。

## 用法

```cpp
je_game_world_t* world = load_world_from_toml(wdat);
je_ecs_world_source_path_set(world, "@/resource/worlds/main.je4world");
```

## 注意事项

- 世界对象本身不保存/读取文件；该路径只用于世界级派生缓存，例如 `<世界路径>.je4cache` 中的 `audio_environment`。
- 引擎的 `JEGaming::Loading::World::load` 与编辑器的打开/保存流程会自动设置它。
- 设置路径本身不会触发重载；需要重新读取 sidecar 时调用 `je_audio_environment_reload`。

## 相关接口

- [je_ecs_world_source_path_get](je_ecs_world_source_path_get.md) - 获取世界源路径
- [je_audio_environment_reload](../audio/je_audio_environment_reload.md) - 重新加载音频环境
