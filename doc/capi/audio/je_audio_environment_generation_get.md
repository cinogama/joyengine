# je_audio_environment_generation_get

## 函数签名

```c
JE_API uint64_t je_audio_environment_generation_get(
    je_game_world_t* world);
```

## 描述

获取世界的音频环境重载代数。每次调用 `je_audio_environment_reload` 或 `je_audio_environment_bake` 成功后，代数递增。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

当前重载代数（从 0 开始）。

## 用法

音频系统用它判断是否需要重新读取 `<世界源路径>.je4cache`：

```cpp
static uint64_t s_seen_generation = 0;
uint64_t generation = je_audio_environment_generation_get(world);
if (generation != s_seen_generation)
{
    s_seen_generation = generation;
    reload_audio_environment(world);
}
```

## 相关接口

- [je_audio_environment_reload](je_audio_environment_reload.md) - 递增代数并请求重载
