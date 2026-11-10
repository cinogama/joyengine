# je_graphic_cmd_update_uniform_buffer

## 函数签名

```c
JE_API void je_graphic_cmd_update_uniform_buffer(
    je_gl_command_buffer_t* cmd,
    je_gl_uniform_buffer_t* buffer,
    const void* data,
    size_t size);
```

## 描述

录制一个一致变量缓冲区的「本帧完整内容」。命令**不直接修改资源**：数据被拷贝进
命令缓冲，提交命令时由图形后端落实，且只对本次提交中包含的绘制生效。

- `size` 必须等于创建时指定的一致变量缓冲区长度（本接口不做局部更新）。
- **必须在绑定它的每一帧都录制一次内容。** 没有录制更新的帧，缓冲区内容**未定义**：
  取决于图形后端，可能是此前某次更新的内容，也可能是未初始化的数据（缓冲区创建时
  不保证初始化），调用方不得依赖其中任何一种。
- 更新建议录制在 [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md)
  返回的更新命令缓冲中（引擎以最低优先级提交，先于本帧所有绘制链）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | 目标命令缓冲区 |
| `buffer` | `je_gl_uniform_buffer_t*` | 要更新的一致变量缓冲区 |
| `data` | `const void*` | 本帧的完整一致变量内容 |
| `size` | `size_t` | 内容字节数，必须等于创建长度 |

## 返回值

无。

## 用法

### 示例

```cpp
BasePipelineInterface::default_uniform_buffer_data_t data;
memcpy(data.m_v_float4x4, view, sizeof(view));
memcpy(data.m_p_float4x4, proj, sizeof(proj));
memcpy(data.m_vp_float4x4, vp, sizeof(vp));
data.m_time[0] = time.x;
data.m_time[1] = time.y;
data.m_time[2] = time.z;
data.m_time[3] = time.w;

je_graphic_cmd_update_uniform_buffer(
    update_cmd, projection.default_uniform_buffer->resource(),
    &data, sizeof(data));
```

## 注意事项

- 数据在录制时拷贝进命令缓冲，调用返回后可以立即复用源缓冲。
- 本命令不需要处于 pass 内，也不要求先绑定该资源。
- 同一个一致变量缓冲区在同一帧内被多次录制时，最后一次内容生效。

## 相关接口

- [je_graphic_uniformbuf_create](je_graphic_uniformbuf_create.md) - 创建一致变量缓冲区
- [je_graphic_cmd_bind_uniform_buffer](je_graphic_cmd_bind_uniform_buffer.md) - 绑定一致变量缓冲区
- [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md) - 申请更新命令缓冲
