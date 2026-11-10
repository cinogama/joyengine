# je_graphic_branch_new_update_command

## 函数签名

```c
JE_API je_gl_command_buffer_t* je_graphic_branch_new_update_command(
    je_render_branch_t* branch);
```

## 描述

从绘制组申请一个已经 reset、且**不开启任何 pass** 的命令缓冲区，用于录制资源
更新（`je_graphic_cmd_update_*` / `je_graphic_cmd_upload_texture_*`）。

宿主按绘制组的优先级提交命令缓冲：引擎以最低优先级
（`jeecs::graphic::GRAPHIC_UPDATE_BRANCH_QUEUE`，即 `INT32_MIN`）
调用 [je_graphic_branch_new_frame](je_graphic_branch_new_frame.md)，使更新先于
本帧所有绘制链提交，从而让绘制使用本帧录制的内容。

与绘制命令缓冲不同，更新命令缓冲在窗口被跳过（最小化、图形实现请求 SKIP）的
帧中仍会被提交：只跳过绘制，资源更新不丢失。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `je_render_branch_t*` | 提供命令缓冲池的绘制组 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_command_buffer_t*` | 已 reset 的更新命令缓冲区（不会返回 `nullptr`） |

## 用法

### 示例

```cpp
je_render_branch_t* update_branch = je_graphic_uhost_alloc_branch(host);

// 每帧
je_graphic_branch_new_frame(update_branch, INT32_MIN);
je_gl_command_buffer_t* cmd = je_graphic_branch_new_update_command(update_branch);

je_graphic_cmd_update_uniform_buffer(cmd, ubo, &data, sizeof(data));
je_graphic_cmd_update_vertex(cmd, vertices, staging, staging_size, index_count);
```

## 注意事项

- 和绘制命令缓冲一样，命令缓冲在下一次 `je_graphic_branch_new_frame` 时被复用，
  已录制内容在提交前必须保持有效（资源会被 retain）。
- 更新命令缓冲不开始/结束 pass；在其中调用绘制命令属于非法用法。
- 没有录任何命令的更新缓冲会在提交时被直接忽略，不触发后端动作。

## 相关接口

- [je_graphic_branch_new_command_buffer](je_graphic_branch_new_command_buffer.md) - 申请绘制命令缓冲
- [je_graphic_branch_new_frame](je_graphic_branch_new_frame.md) - 开始新一帧
- [je_graphic_cmd_update_vertex](je_graphic_cmd_update_vertex.md) - 录制顶点本帧内容
- [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) - 录制 UBO 本帧内容
- [je_graphic_cmd_upload_texture_2d](je_graphic_cmd_upload_texture_2d.md) - 持久纹理区段上传
