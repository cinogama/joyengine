# je_graphic_cmd_commit_updates

## 函数签名

```c
JE_API void je_graphic_cmd_commit_updates(je_gl_command_buffer_t* cmd);
```

## 描述

在图形线程只提交命令缓冲区中录制的资源更新（`je_graphic_cmd_update_vertex` / `je_graphic_cmd_update_uniform_buffer`），跳过全部 pass 与绘制。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | — |

## 返回值

无返回值。

## 注意事项

- 用于图形实现请求跳过本帧绘制（窗口最小化等）时：绘制可以丢弃，但已录制的资源更新必须生效，否则只在内容变化时才录制更新的资源会永久停留在旧内容
- 与 `je_graphic_cmd_commit` 相同，只允许在命令缓冲区所属的图形线程调用；引擎的绘制组（branch）在跳过本帧绘制时自动改用本接口

## 相关接口

- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 提交命令缓冲区
- [je_graphic_cmd_update_vertex](je_graphic_cmd_update_vertex.md) - 录制顶点数据更新
- [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) - 录制一致变量缓冲区更新
