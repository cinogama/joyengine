# je_graphic_cmd_update_vertex

## 函数签名

```c
JE_API void je_graphic_cmd_update_vertex(
    je_gl_command_buffer_t* cmd, je_gl_vertex_t* vertex,
    size_t offset, const void* data, size_t size);
```

## 描述

录制一次顶点数据更新：把 `data` 起的 `size` 字节写入顶点资源顶点数据的 `offset` 处。顶点资源本身不保存可被外部改写的数据状态，更新只能以命令的形式录制，在命令缓冲区提交时才真正生效。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | 命令缓冲区 |
| `vertex` | `je_gl_vertex_t*` | 顶点资源 |
| `offset` | `size_t` | 顶点数据内的字节偏移 |
| `data` | `const void*` | 数据源指针，调用时被拷贝进命令缓冲区 |
| `size` | `size_t` | 数据长度（字节），必须大于 0 |

## 返回值

无返回值。

## 用法

用于逐帧改写的动态网格（如粒子系统、UI 文本批处理）。数据在调用时即被快照，调用返回后源缓冲可立即复用。

### 示例

```cpp
// 把整份暂存数据写入动态网格（先于绑定/绘制录制）
je_graphic_cmd_update_vertex(cmd, vertex, 0, staging.data(), staging_bytes);
je_graphic_cmd_bind_vertex(cmd, vertex);
```

## 注意事项

- 更新按录制顺序生效：同一命令缓冲区内，位于该更新之后的绘制读到新内容，之前的绘制不受影响；跨命令缓冲区的先后由提交顺序决定（绘制组按优先级从小到大提交）
- pass 内外均可调用
- `offset + size` 不得超过顶点资源创建时的数据长度；索引数据不可更新，参与绘制的索引数量由绘制命令（`je_graphic_cmd_draw_indexed`）给出
- 只在内容变化时才录制更新的资源，其更新由 `je_graphic_cmd_commit_updates` 在跳过绘制的帧里同样生效

## 相关接口

- [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) - 录制一致变量缓冲区更新
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 提交命令缓冲区
- [je_graphic_cmd_commit_updates](je_graphic_cmd_commit_updates.md) - 只提交资源更新
- [je_graphic_branch_new_update_command_buffer](je_graphic_branch_new_update_command_buffer.md) - 申请只用于录制资源更新的命令缓冲区
