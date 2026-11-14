# je_ecs_world_command_flush

## 函数签名

```c
JE_API void je_ecs_world_command_flush(je_game_world_t* world);
```

## 描述

立即处理世界命令缓冲区中的结构性变更（实体/组件的增删、系统增删等），但不推进系统更新。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

无返回值。

## 用法

用于临时加载、未进入正常更新循环的世界：

```cpp
// 编辑器导出时临时 restore 世界后，先落实实体再烘焙音频环境
je_ecs_world_command_flush(world);
je_audio_environment_bake(world, world_path);
```

## 注意事项

- 正常游戏循环不需要调用：命令缓冲区在世界更新时自动处理。
- 该接口只处理命令缓冲区，不更新系统，也不推进 deltatime。
- 对正在运行的世界调用会提前落实尚未处理的结构性变更，通常只应在编辑器/工具代码的帧间调用。

## 相关接口

- [je_audio_environment_bake](../audio/je_audio_environment_bake.md) - 烘焙世界音频环境
