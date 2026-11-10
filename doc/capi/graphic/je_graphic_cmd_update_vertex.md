# je_graphic_cmd_update_vertex

## 函数签名

```c
JE_API void je_graphic_cmd_update_vertex(
    je_gl_command_buffer_t* cmd,
    je_gl_vertex_t* vertex,
    /* OPTIONAL: nullptr 时只调整激活索引数量 */ const void* vertices,
    size_t vertex_length,
    size_t active_index_count);
```

## 描述

录制一个顶点的「本帧内容」。命令**不直接修改资源**：数据被拷贝进命令缓冲，
提交命令时由图形后端落实，且只对本次提交中包含的绘制生效。

- **带动态内容的缓冲，必须在绘制它的每一帧都录制一次内容。** 没有录制内容的帧，
  绑定到的顶点内容**未定义**：取决于图形后端，可能是创建期内容，也可能是此前某次
  更新的内容，调用方不得依赖其中任何一种。
- 从未录制过内容的缓冲是静态缓冲，始终绑定创建期内容（`je_graphic_vertex_create`
  传入的数据）。
- 更新建议录制在 [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md)
  返回的更新命令缓冲中（引擎以最低优先级提交，先于本帧所有绘制链）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | 目标命令缓冲区 |
| `vertex` | `je_gl_vertex_t*` | 要更新的顶点资源 |
| `vertices` | `const void*` | 一份完整的顶点前缀数据；传 `nullptr` 时只调整激活索引数量 |
| `vertex_length` | `size_t` | 顶点数据的字节数，不得超过资源创建时的大小 |
| `active_index_count` | `size_t` | 本帧实际参与绘制的索引数量（不得超过创建时的索引总数） |

## 返回值

无。

## 用法

### 示例

```cpp
je_gl_command_buffer_t* update_cmd = je_graphic_branch_new_update_command(update_branch);

// 只录存活粒子占用的前缀（每粒子 4 顶点 6 索引）
je_graphic_cmd_update_vertex(
    update_cmd, vertex_resource,
    staging.data(), alive_particle_count * 4 * FLOATS_PER_VERTEX * sizeof(float),
    alive_particle_count * 6);
```

## 注意事项

- 数据在录制时拷贝进命令缓冲，调用返回后可以立即复用源缓冲。
- 本命令只定义 `vertices` 覆盖的前缀：前缀之外的顶点内容未定义，
  `active_index_count` 范围内的索引不得引用它们。只录实际用到的前缀可以减少拷贝，
  并节省后端的每帧瞬态存储。
- `vertices` 传 `nullptr`（此时 `vertex_length` 必须为 0）只调整激活索引数量，
  **不算作一次内容录制**：本帧绑定到的顶点内容按上文「没有录制内容的帧」处理。
- `active_index_count` 为 0 时会被钳制为 1：0 长度的绘制调用在部分后端上非法，
  而 1 个索引的 TRIANGLES 绘制不产生任何图元。
- 本命令同步资源的激活索引数量（绘制数量属于 CPU 侧状态，仅录制线程读取）。
- 索引缓冲内容创建后不变，不会被本命令修改。
- 本命令不需要处于 pass 内，也不要求先绑定该资源。
- 包围盒不会重算：依赖视锥剔除的调用方应以覆盖完整范围的初始数据创建缓冲。

## 相关接口

- [je_graphic_vertex_create](je_graphic_vertex_create.md) - 创建顶点资源
- [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) - 录制一致变量缓冲区内容
- [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md) - 申请更新命令缓冲
