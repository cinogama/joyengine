# je_audio_environment_reload

## 函数签名

```c
JE_API void je_audio_environment_reload(je_game_world_t* world);
```

## 描述

通知世界的音频系统重新读取 `WorldCache` 内部实体指向的音频环境缓存。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 目标世界 |

## 返回值

无返回值。

## 用法

```cpp
// 世界保存后旧缓存已因世界 CRC 变化失效，让音频系统立刻重载/回退
je_audio_environment_reload(world);
```

## 注意事项

- 缓存路径来自持有 `WorldCache` 组件的内部实体（`cache_path`；该实体配对 `Editor::Invisible`，不显示、不随世界存档，由世界加载或编辑器烘焙创建），不由世界对象保存；实体不存在时音频系统使用 `Audio::ReverbZone` / `Audio::Portal` 组件实时构建的环境图。
- 音频系统在下一帧的更新中重新读取缓存；缓存缺失或版本不符时自动回退实时构建。
- `je_audio_environment_bake` 成功后会自动调用本接口；编辑器保存世界后也会调用它，因为世界文件 CRC 变化使旧缓存失效。

## 相关接口

- [je_audio_environment_bake](je_audio_environment_bake.md) - 烘焙音频环境缓存
