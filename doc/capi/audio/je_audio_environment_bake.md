# je_audio_environment_bake

## 函数签名

```c
JE_API bool je_audio_environment_bake(
    je_game_world_t* world, const char* world_path);
```

## 描述

把世界中由 `Audio::ReverbZone`（房间）与 `Audio::Portal`（门洞）构成的音频环境烘焙到 `<world_path>.je4cache` 的 `audio_environment` 字段。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `je_game_world_t*` | 要烘焙的世界 |
| `world_path` | `const char*` | 世界文件路径（通常是 `.je4world`）；文件必须已存在于磁盘上 |

## 返回值

- `true`：烘焙成功，缓存已写入
- `false`：世界路径不可用、世界文件不存在或写缓存失败

## 用法

```cpp
if (!je_audio_environment_bake(world, "@/resource/worlds/main.je4world"))
    logerr("Bake audio environment failed.");
```

## 注意事项

- 只读取世界中的组件与实体变换，**不读取 tilemap**；tilemap 若需要生成房间/门洞，应先把它生成为普通 `Audio::ReverbZone` / `Audio::Portal` 实体。
- 不要求世界安装 `AudioUpdatingSystem`；未启用/未 update 的临时世界会先处理命令缓冲区，烘焙仍能读取组件。
- 缓存键是世界文件的 CRC64；请在世界保存之后调用。世界保存后旧缓存自动失效。
- 烘焙内容包括房间声学参数、门洞连接关系与全房间对路径表；缓存自包含，运行时不需要再用组件重建。
- 本接口只写缓存，不修改世界内容：缓存路径由持有 `WorldCache` 组件的内部实体提供（该实体配对 `Editor::Invisible`，不显示、不随世界存档；世界加载时若文件旁存在 `<world>.je4cache` 自动创建，编辑器手动烘焙时也会创建），运行时音频系统按该实体加载缓存。
- 烘焙成功后自动调用 `je_audio_environment_reload`，正在运行的音频系统会在下一次更新时加载新缓存。
- 该接口为编辑器手动烘焙与导出时烘焙共用；不要求世界已启用或已 update。

## 相关接口

- [je_audio_environment_reload](je_audio_environment_reload.md) - 通知音频系统重新读取缓存
