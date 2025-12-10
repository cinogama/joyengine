# jegl_branch_new_chain

## 函数签名

```c
JE_API jegl_rendchain* jegl_branch_new_chain(
    jeecs::rendchain_branch* branch,
    jegl_frame_buffer* framebuffer,
    int32_t x,
    int32_t y,
    uint32_t w,
    uint32_t h);
```

## 描述

从绘制组中获取一个新的 RendChain。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `jeecs::rendchain_branch*` | 绘制组指针 |
| `framebuffer` | `jegl_frame_buffer*` | 目标帧缓冲（`nullptr` 表示屏幕） |
| `x` | `int32_t` | 视口起始 X 坐标 |
| `y` | `int32_t` | 视口起始 Y 坐标 |
| `w` | `uint32_t` | 视口宽度 |
| `h` | `uint32_t` | 视口高度 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_rendchain*` | 渲染链指针 |

## 用法

此函数用于从绘制组中创建一个已初始化的渲染链。

### 示例

```cpp
jeecs::rendchain_branch* branch = jegl_uhost_alloc_branch(host);
jegl_branch_new_frame(branch, 0);

// 创建渲染到屏幕的链
jegl_rendchain* screen_chain = jegl_branch_new_chain(branch, nullptr, 0, 0, 1920, 1080);

// 创建渲染到 FBO 的链
jegl_rendchain* offscreen_chain = jegl_branch_new_chain(branch, fbo, 0, 0, 1024, 1024);

// 使用渲染链...
```

## 注意事项

- 返回的链已经调用过 `jegl_rchain_begin`
- 链的生命周期由绘制组管理，不需要手动关闭

## 相关接口

- [jegl_branch_new_frame](jegl_branch_new_frame.md) - 开始新帧
- [jegl_rchain_draw](jegl_rchain_draw.md) - 绘制操作
