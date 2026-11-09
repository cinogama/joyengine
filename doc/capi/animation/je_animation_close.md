# je_animation_close

## 函数签名

```c
JE_API void je_animation_close(je_animation_t* animation);
```

## 描述

释放 je_animation_open 返回的动画集数据（含全部轨道、关键帧与事件）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `animation` | `je_animation_t*` | je_animation_open 的返回值 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 不得在仍有组件/采样缓冲引用其轨道数据（如 uniform 名指针）时释放
