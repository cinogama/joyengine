# jegl_rchain_bind_uniform_buffer

## 函数签名

```c
JE_API void jegl_rchain_bind_uniform_buffer(
    jegl_rendchain* chain,
    jegl_uniform_buffer* uniformbuffer);
```

## 描述

绑定绘制链的一致变量缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `uniformbuffer` | `jegl_uniform_buffer*` | 一致变量缓冲区指针 |

## 返回值

无返回值。

## 用法

此函数用于将 UBO 绑定到绘制链，链中所有后续绘制操作都可以使用此 UBO。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);

// 绑定相机 UBO
jegl_rchain_bind_uniform_buffer(chain, camera_ubo);

// 绘制操作...
```

## 相关接口

- [jegl_create_uniformbuf](jegl_create_uniformbuf.md) - 创建一致变量缓冲
- [jegl_rchain_set_uniform_buffer](jegl_rchain_set_uniform_buffer.md) - 为单个绘制操作设置 UBO
