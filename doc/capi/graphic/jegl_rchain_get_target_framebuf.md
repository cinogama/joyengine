# jegl_rchain_get_target_framebuf

## 函数签名

```c
JE_API jegl_frame_buffer* jegl_rchain_get_target_framebuf(
    jegl_rendchain* chain);
```

## 描述

获取当前绘制链的目标帧缓冲区。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_frame_buffer*` | 目标帧缓冲区指针；若目标是屏幕缓冲区则返回 `nullptr` |

## 用法

此函数用于查询绘制链当前的渲染目标。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();
jegl_rchain_begin(chain, fbo, 0, 0, 1024, 1024);

jegl_frame_buffer* target = jegl_rchain_get_target_framebuf(chain);
if (target != nullptr) {
    printf("Rendering to off-screen framebuffer\n");
} else {
    printf("Rendering to screen\n");
}
```

## 相关接口

- [jegl_rchain_begin](jegl_rchain_begin.md) - 绑定绘制目标
