# je_graphic_uniformbuf_update

## 函数签名

```c
JE_API void je_graphic_uniformbuf_update(
    jegl_uniform_buffer* uniformbuf,
    const void* buf,
    size_t update_offset,
    size_t update_length);
```

## 描述

更新一个一致变量缓冲区中，指定位置起，若干长度的数据。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `uniformbuf` | `jegl_uniform_buffer*` | 一致变量缓冲区资源指针 |
| `buf` | `const void*` | 数据源指针 |
| `update_offset` | `size_t` | 更新起始偏移量（字节） |
| `update_length` | `size_t` | 更新数据长度（字节） |

## 返回值

无返回值。

## 用法

此函数用于更新 UBO 中的数据。

### 示例

```cpp
jegl_uniform_buffer* ubo = je_graphic_uniformbuf_create(0, 256);

// 定义数据结构
struct LightData {
    float position[4];
    float color[4];
    float intensity;
};

LightData light;
light.position[0] = 10.0f;
light.position[1] = 20.0f;
light.position[2] = 30.0f;
light.position[3] = 1.0f;
light.color[0] = 1.0f;
light.color[1] = 1.0f;
light.color[2] = 1.0f;
light.color[3] = 1.0f;
light.intensity = 1.0f;

// 更新整个结构
je_graphic_uniformbuf_update(ubo, &light, 0, sizeof(LightData));

// 只更新强度值
float new_intensity = 0.5f;
je_graphic_uniformbuf_update(ubo, &new_intensity, offsetof(LightData, intensity), sizeof(float));

je_graphic_uniformbuf_close(ubo);
```

## 注意事项

- 确保 `update_offset + update_length` 不超过缓冲区大小
- 部分更新可以提高效率

## 相关接口

- [je_graphic_uniformbuf_create](je_graphic_uniformbuf_create.md) - 创建一致变量缓冲
- [je_graphic_cmd_bind_uniform_buffer](je_graphic_cmd_bind_uniform_buffer.md) - 绑定一致变量缓冲
