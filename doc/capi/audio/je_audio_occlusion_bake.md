# je_audio_occlusion_bake

## 函数签名

```c
JE_API bool je_audio_occlusion_bake(
    je_game_world_t* world, const char* world_path);
```

## 描述

把世界中的静态遮挡烘焙到 `<world_path>.je4cache` 的 `audio_occlusion` 字段，生成一张世界空间 2D 遮挡场。

烘焙输入：

- `is_static = true` 的 `Audio::Occluder`（含 `Translation`，世界变换已解析）；
- 世界中全部 `Tilemap::Map` 的瓦片音频遮挡（瓦片地图遮挡一律视为静态）。

动态遮挡体（`is_static = false`）不烘焙，运行时继续逐帧测试。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 要烘焙的世界 |
| `world_path` | `const char*` | 世界文件路径（通常是 `.je4world`）；文件必须已存在于磁盘上 |

## 返回值

- `true`：烘焙成功，缓存已写入
- `false`：世界路径为空、世界文件不存在或写缓存失败

## 用法

```cpp
if (!je_audio_occlusion_bake(world, "@/resource/worlds/main.je4world"))
    logerr("Bake audio occlusion cache failed.");
```

## 注意事项

- 只读取静态遮挡体与瓦片地图；房间/门洞图（`Audio::ReverbZone`/`Audio::Portal`）没有缓存，始终在运行时实时构建。
- 遮挡场只用于 2D（`Audio::Listener::use_2d = true`）；3D 听者始终走逐遮挡体实时路径。
- 不要求世界安装 `AudioUpdatingSystem`；但实体/组件的结构性变更经命令缓冲区延迟落实——未启用/未 update 的临时世界必须先让一帧世界更新（命令缓冲区处理完）再调用本接口。
- 编辑器导出时在协程里恢复临时世界后 `yield()` 一帧，再交给后台导出任务线程逐世界烘焙，并把日志显示在导出进度窗口中。
- 该接口只读世界；在其它线程调用时应持有 [je_ecs_world_arch_defragmentation_lock](../ecs/je_ecs_world_arch_defragmentation_lock.md) / [unlock](../ecs/je_ecs_world_arch_defragmentation_unlock.md)，避免 chunk 碎片整理在读取期间搬运实体。
- 缓存键是世界文件的 CRC64；请在世界保存之后调用。世界保存后旧缓存自动失效。
- 场格式（`cell_size`、原点、尺寸与 `uint16` 密度缓冲）由 sidecar 内部版本约束；每格存“单位长度光学密度” `-ln(透过率)/厚度`，运行时按射线穿越长度积分，边缘格带 4×4 超采样覆盖率。
- 烘焙会把静态输入（遮挡体的世界变换与系数、地图/图集 `.je4cache` 的源 CRC）哈希进缓存，运行时周期性比对；不匹配时自动回退实时遮挡，因此修改静态遮挡后忘记重新烘焙也不会产生错误结果。
- 本接口只写缓存，不修改世界内容：缓存路径由持有 `World::Path` 组件的内部实体提供（该实体配对 `Editor::Invisible`，不显示、不随世界存档；加载世界与编辑器手动烘焙时创建），运行时音频系统按该实体加载缓存。
- 烘焙成功后自动调用 [je_audio_occlusion_reload](je_audio_occlusion_reload.md)，正在运行的音频系统会在下一次更新时加载新缓存。
- 该接口为编辑器手动烘焙与导出时烘焙共用；不要求世界已启用或已 update。

## 相关接口

- [je_audio_occlusion_reload](je_audio_occlusion_reload.md) - 通知音频系统重新读取遮挡缓存
