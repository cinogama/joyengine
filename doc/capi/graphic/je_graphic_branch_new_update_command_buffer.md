# je_graphic_branch_new_update_command_buffer

## 函数签名

```c
JE_API je_gl_command_buffer_t* je_graphic_branch_new_update_command_buffer(
    je_render_branch_t* branch);
```

## 描述

从绘制组申请一个已经 reset、未开始任何 pass 的命令缓冲区，用于只录制资源更新（`je_graphic_cmd_update_vertex` / `je_graphic_cmd_update_uniform_buffer`）。供不参与绘制、但需要改写图形资源内容的系统使用（如粒子系统）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `je_render_branch_t*` | 绘制组 |

## 返回值

`je_gl_command_buffer_t*` — 命令缓冲区，非空。

## 用法

为绘制组指定比使用这些资源的绘制组更小的优先级（`je_graphic_branch_new_frame`），即可保证更新先于绘制提交（绘制组按优先级从小到大提交）。

### 示例

```cpp
je_graphic_branch_new_frame(branch, INT_MIN);
je_gl_command_buffer_t* update_cmd =
    je_graphic_branch_new_update_command_buffer(branch);

je_graphic_cmd_update_vertex(update_cmd, vertex, 0, staging.data(), staging_bytes);
```

## 注意事项

- 图形实现请求跳过本帧绘制时，其中录制的更新仍会生效（经 `je_graphic_cmd_commit_updates`）
- 与 `je_graphic_branch_new_command_buffer` 相同，命令缓冲区在绘制组内的位置由申请顺序决定

## 相关接口

- [je_graphic_branch_new_command_buffer](je_graphic_branch_new_command_buffer.md) - 申请以 pass 开始的命令缓冲区
- [je_graphic_branch_new_frame](je_graphic_branch_new_frame.md) - 开始新的一帧并指定优先级
- [je_graphic_cmd_update_vertex](je_graphic_cmd_update_vertex.md) - 录制顶点数据更新
