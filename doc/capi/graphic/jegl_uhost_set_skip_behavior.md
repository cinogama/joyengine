# jegl_uhost_set_skip_behavior

## 函数签名

```cpp
JE_API void jegl_uhost_set_skip_behavior(
    jeecs::graphic_uhost* host, bool skip_all_draw);
```

## 描述

设置图形实现请求跳过这一帧时，uhost 的绘制动作行为。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| host | jeecs::graphic_uhost* | 图形上下文主机 |
| skip_all_draw | bool | 是否跳过全部绘制动作 |

## 返回值

无

## 注意事项

- 当 `skip_all_draw` 为真时，跳过全部绘制动作
- 当 `skip_all_draw` 为假时，只跳过以屏幕缓冲区为目标的绘制动作
- uhost 实例创建时默认为真

## 相关接口

- [jegl_uhost_alloc_branch](jegl_uhost_alloc_branch.md) - 分配绘制组
- [jegl_uhost_free_branch](jegl_uhost_free_branch.md) - 释放绘制组
