# je_animation_sample_buffer_create

## 函数签名

```c
JE_API je_animation_sample_buffer_t* je_animation_sample_buffer_create(void);
```

## 描述

创建一个采样累加缓冲，用于把一个或多个动作的加权采样结果混合后写入实体。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `—` | `—` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_animation_sample_buffer_t*` | 新缓冲 |

## 注意事项

- 用毕以 je_animation_sample_buffer_destroy 释放
- 可跨帧复用：每轮采样前先 je_animation_sample_buffer_clear
