# je_audio_occlusion_reload

## 函数签名

```c
JE_API void je_audio_occlusion_reload(je_game_world_t* world);
```

## 描述

通知世界的音频系统重新读取 `World::Path` 组件指向的 `<world>.je4cache` 遮挡场，在音频系统的下一次更新（`CommitUpdate`）时生效。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界；不需要已安装 `AudioUpdatingSystem` |

## 返回值

无返回值。

## 用法

```cpp
je_audio_occlusion_reload(world);
```

## 注意事项

- 缓存路径来自持有 `World::Path` 组件的内部实体，不由世界对象保存；组件不存在、缓存缺失或格式不匹配时按无缓存处理，运行时直接走逐帧实时遮挡。
- 载入后立即校验静态输入哈希，之后约每秒复核一次；输入变化（例如改了 `is_static` 遮挡体的值或地图/图集缓存）时弃用缓存并 `loginfo` 提示，回退实时遮挡。
- 世界保存后世界文件 CRC 变化，旧缓存本身已失效；编辑器保存世界后也会调用本接口。
- [je_audio_occlusion_bake](je_audio_occlusion_bake.md) 成功后会自行调用本接口，通常不需要手动调用。
- 3D 听者（`Audio::Listener::use_2d = false`）不使用缓存场。

## 相关接口

- [je_audio_occlusion_bake](je_audio_occlusion_bake.md) - 烘焙静态遮挡缓存
