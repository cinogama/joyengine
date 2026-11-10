# je_graphic_cmd_update_uniform_buffer

## 函数签名

```c
JE_API void je_graphic_cmd_update_uniform_buffer(
    je_gl_command_buffer_t* cmd, je_gl_uniform_buffer_t* buffer,
    size_t offset, const void* data, size_t size);
```

## 描述

录制一次一致变量缓冲区更新：把 `data` 起的 `size` 字节写入缓冲区的 `offset` 处。缓冲区内容自创建起为全 0，此后只能以命令的形式录制更新，在命令缓冲区提交时才真正生效。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | 命令缓冲区 |
| `buffer` | `je_gl_uniform_buffer_t*` | 一致变量缓冲区资源 |
| `offset` | `size_t` | 缓冲区内的字节偏移 |
| `data` | `const void*` | 数据源指针，调用时被拷贝进命令缓冲区 |
| `size` | `size_t` | 数据长度（字节），必须大于 0 |

## 返回值

无返回值。

## 用法

用于按帧更新的共享 uniform 数据（如相机视图/投影矩阵）。

### 示例

```cpp
struct CameraData { float view[4][4]; float proj[4][4]; float vp[4][4]; float time[4]; };

// 更新录制在绑定之前：本帧随后的绘制读到新内容
je_graphic_cmd_update_uniform_buffer(cmd, ubo,
    offsetof(CameraData, view), &mat_view, sizeof(mat_view));
je_graphic_cmd_update_uniform_buffer(cmd, ubo,
    offsetof(CameraData, time), &shader_time, sizeof(shader_time));
je_graphic_cmd_bind_uniform_buffer(cmd, ubo);
```

## 注意事项

- 更新按录制顺序生效：同一命令缓冲区内，位于该更新之后的绘制读到新内容，之前的绘制不受影响；跨命令缓冲区的先后由提交顺序决定（绘制组按优先级从小到大提交）
- pass 内外均可调用
- `offset + size` 不得超过缓冲区创建时的大小
- 只在内容变化时才录制更新的资源，其更新由 `je_graphic_cmd_commit_updates` 在跳过绘制的帧里同样生效

## 相关接口

- [je_graphic_cmd_update_vertex](je_graphic_cmd_update_vertex.md) - 录制顶点数据更新
- [je_graphic_uniformbuf_create](je_graphic_uniformbuf_create.md) - 创建一致变量缓冲区
- [je_graphic_cmd_bind_uniform_buffer](je_graphic_cmd_bind_uniform_buffer.md) - 绑定一致变量缓冲区
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 提交命令缓冲区
