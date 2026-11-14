# je_ecs_world_source_path_get

## 函数签名

```c
JE_API const char* je_ecs_world_source_path_get(je_game_world_t* world);
```

## 描述

获取世界的源文件路径。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

世界源文件路径；未设置时返回空字符串。返回的指针由世界持有，在世界销毁或路径被改写前有效。

## 用法

```cpp
const char* path = je_ecs_world_source_path_get(world);
if (path[0] != '\0')
    load_audio_environment_cache(path);
```

## 相关接口

- [je_ecs_world_source_path_set](je_ecs_world_source_path_set.md) - 设置世界源路径
