# jegl_rchain_begin

## 函数签名

```c
JE_API void jegl_rchain_begin(
    jegl_rendchain* chain,
    jegl_frame_buffer* framebuffer,
    int32_t x, int32_t y, uint32_t w, uint32_t h);
```

## 描述

绑定绘制链的绘制目标，此操作是绘制周期的开始。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `framebuffer` | `jegl_frame_buffer*` | 目标帧缓冲（`nullptr` 表示屏幕） |
| `x` | `int32_t` | 视口起始 X 坐标（像素） |
| `y` | `int32_t` | 视口起始 Y 坐标（像素） |
| `w` | `uint32_t` | 视口宽度（像素） |
| `h` | `uint32_t` | 视口高度（像素） |

## 返回值

无返回值。

## 用法

此函数用于指定绘制链的渲染目标和视口区域。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();

// 渲染到屏幕
jegl_rchain_begin(chain, nullptr, 0, 0, 1920, 1080);

// 或渲染到帧缓冲
jegl_rchain_begin(chain, fbo, 0, 0, 1024, 1024);
```

## 注意事项

- `framebuffer == nullptr` 指示绘制目标为屏幕缓冲区
- `x, y` 为绘制剪裁空间左下角的位置

## 相关接口

- [jegl_rchain_create](jegl_rchain_create.md) - 创建绘制链
- [jegl_rchain_clear_color_buffer](jegl_rchain_clear_color_buffer.md) - 清除颜色缓冲
