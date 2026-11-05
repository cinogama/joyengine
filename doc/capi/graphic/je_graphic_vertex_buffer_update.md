# je_graphic_vertex_buffer_update

## 函数签名

```c
JE_API void je_graphic_vertex_buffer_update(
    jegl_vertex* vertex,
    /* OPTIONAL */ const void* vertex_data,
    size_t vertex_length,
    size_t active_index_count);
```

## 描述

整体更新一个顶点（模型）资源中的顶点数据，用于逐帧改写动态网格（如粒子系统）

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `vertex` | `jegl_vertex*` | — |
| `vertex_data` | `const void*` | 可选。 vertex_data 与 vertex_length 描述一份完整的顶点数据，将被拷贝替换掉资源中原有的顶点数据；vertex_data 传入 nullptr 时只调整激活的索引数量 |
| `vertex_length` | `size_t` | — |
| `active_index_count` | `size_t` | active_index_count 为本次更新后实际参与绘制的索引数量（不能超过资源创建时的索引总数），用于在容量固定的缓冲区上收缩绘制范围；传入 0 时会被钳制为 1——0 长度的绘制调用与 0 字节的缓冲区分配在部分后端上非法，而 1 个索引的 TRIANGLES 绘制不产生任何图元 |

## 返回值

无返回值。

## 注意事项

- vertex_data 与 vertex_length 描述一份完整的顶点数据，将被拷贝替换掉资源中原有的顶点数据；vertex_data 传入 nullptr 时只调整激活的索引数量
- active_index_count 为本次更新后实际参与绘制的索引数量（不能超过资源创建时的索引总数），用于在容量固定的缓冲区上收缩绘制范围；传入 0 时会被钳制为 1——0 长度的绘制调用与 0 字节的缓冲区分配在部分后端上非法，而 1 个索引的 TRIANGLES 绘制不产生任何图元
- 调用后资源的 m_modified 标记被置位，图形后端将在该资源的下次使用时通过vertex_update 钩子重新上传数据（索引缓冲本身不重传，只刷新绘制数量；索引缓冲在初始化时按创建时的索引总量分配，与激活数量无关）
- 与 je_graphic_uniformbuf_update 相同，本接口必须在逻辑帧窗口内（图形线程静默时）调用

## 相关接口

- [je_graphic_vertex_create](je_graphic_vertex_create.md) - 用指定的顶点数据创建一个顶点（模型）资源
- [je_graphic_uniformbuf_update](je_graphic_uniformbuf_update.md) - 更新一个一致变量缓冲区中，指定位置起，若干长度的数据
