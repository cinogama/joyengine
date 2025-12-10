# jegl_create_uniformbuf

## 函数签名

```c
JE_API jegl_uniform_buffer* jegl_create_uniformbuf(
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
| `jegl_uniform_buffer*` | 一致变量缓冲区资源指针 |

## 用法

此函数用于创建 Uniform Buffer Object (UBO)，可在多个着色器之间共享数据。

### 示例

```cpp
// 创建一个 256 字节的 UBO，绑定到 slot 0
jegl_uniform_buffer* ubo = jegl_create_uniformbuf(0, 256);

// 定义数据结构
struct CameraData {
    float view[16];
    float projection[16];
};

CameraData camera_data;
// 填充数据...

// 更新 UBO 数据
jegl_update_uniformbuf(ubo, &camera_data, 0, sizeof(CameraData));

// 绑定 UBO
jegl_bind_uniform_buffer(ubo);

// 使用完毕后释放
jegl_close_uniformbuf(ubo);
```

## 注意事项

- 图形资源创建时自动获取一个引用计数
- 使用此接口创建的缓冲，其资源路径为 `nullptr`
- `binding_place` 需要与着色器中声明的绑定位置一致

## 相关接口

- [jegl_update_uniformbuf](jegl_update_uniformbuf.md) - 更新一致变量缓冲
- [jegl_bind_uniform_buffer](jegl_bind_uniform_buffer.md) - 绑定一致变量缓冲
- [jegl_close_uniformbuf](jegl_close_uniformbuf.md) - 关闭一致变量缓冲
