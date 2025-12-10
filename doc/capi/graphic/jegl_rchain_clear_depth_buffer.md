# jegl_rchain_clear_depth_buffer

## 函数签名

```c
JE_API void jegl_rchain_clear_depth_buffer(
    jegl_rendchain* chain,
    float clear_depth);
```

## 描述

指示此链绘制开始时需要清除目标缓冲区的深度缓存。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `clear_depth` | `float` | 清除深度值（通常为 1.0） |

## 返回值

无返回值。

## 用法

此函数用于在绘制开始前清除深度缓冲区。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);

// 清除深度缓冲
jegl_rchain_clear_depth_buffer(chain, 1.0f);
```

## 注意事项

- 深度值通常在 0.0 到 1.0 之间
- 1.0 表示最远距离，0.0 表示最近距离

## 相关接口

- [jegl_rchain_clear_color_buffer](jegl_rchain_clear_color_buffer.md) - 清除颜色缓冲
