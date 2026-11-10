# je_graphic_uniformbuf_create

## 函数签名

```c
JE_API je_gl_uniform_buffer_t* je_graphic_uniformbuf_create(
    size_t binding_place,
    size_t length);
```

## 描述

创建一个指定大小和绑定位置的一致变量缓冲区资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `binding_place` | `size_t` | 绑定位置（对应着色器中的 binding） |
| `length` | `size_t` | 缓冲区大小（字节） |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_gl_uniform_buffer_t*` | 一致变量缓冲区资源指针 |

## 用法

此函数用于创建 Uniform Buffer Object (UBO)，可在多个着色器之间共享数据。

### 示例

```cpp
// 创建一个 128 字节的 UBO，绑定到 slot 0
je_gl_uniform_buffer_t* ubo = je_graphic_uniformbuf_create(0, 128);

// 定义数据结构（大小必须与创建长度一致）
struct CameraData {
    float view[16];
    float projection[16];
};

CameraData camera_data;
// 填充数据...

// 录制本帧的完整 UBO 内容（建议录制在更新命令缓冲中）
je_graphic_cmd_update_uniform_buffer(update_cmd, ubo, &camera_data, sizeof(CameraData));

// 绑定 UBO
// cmd 为当前绘制命令缓冲区
je_graphic_cmd_bind_uniform_buffer(cmd, ubo);

// 使用完毕后释放
je_graphic_uniformbuf_close(ubo);
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用此接口创建的缓冲，其资源路径为 `nullptr`
- `binding_place` 需要与着色器中声明的绑定位置一致
- 创建期内容为全 0；运行时内容通过
  [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) 按帧录制完整数据

## 相关接口

- [je_graphic_cmd_update_uniform_buffer](je_graphic_cmd_update_uniform_buffer.md) - 录制本帧的完整一致变量内容
- [je_graphic_cmd_bind_uniform_buffer](je_graphic_cmd_bind_uniform_buffer.md) - 绑定一致变量缓冲
- [je_graphic_uniformbuf_close](je_graphic_uniformbuf_close.md) - 关闭一致变量缓冲
