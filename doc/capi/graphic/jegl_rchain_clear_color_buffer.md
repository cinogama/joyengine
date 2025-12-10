# jegl_rchain_clear_color_buffer

## 函数签名

```c
JE_API void jegl_rchain_clear_color_buffer(
    jegl_rendchain* chain,
    size_t attachment_index,
    const float clear_color_rgba[4]);
```

## 描述

指示此链绘制开始时需要清除目标缓冲区的颜色缓存。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `attachment_index` | `size_t` | 颜色附件索引 |
| `clear_color_rgba` | `const float[4]` | 清除颜色（RGBA） |

## 返回值

无返回值。

## 用法

此函数用于在绘制开始前清除颜色缓冲区。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);

// 清除第一个颜色附件为深蓝色
float clear_color[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
jegl_rchain_clear_color_buffer(chain, 0, clear_color);
```

## 注意事项

- `attachment_index` 对应帧缓冲区的颜色附件索引
- 对于屏幕缓冲区，通常只有一个颜色附件（索引 0）

## 相关接口

- [jegl_rchain_clear_depth_buffer](jegl_rchain_clear_depth_buffer.md) - 清除深度缓冲
