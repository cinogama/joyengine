# jegl_rchain_set_uniform_int4

## 函数签名

```cpp
JE_API void jegl_rchain_set_uniform_int4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place_may_null,
    int x,
    int y,
    int z,
    int w);
```

## 描述

为 act 指定的绘制操作应用四维整型一致变量。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| act | jegl_rendchain_rend_action* | 渲染动作 |
| binding_place_may_null | const uint32_t* | 绑定位置，可为空 |
| x | int | X 分量 |
| y | int | Y 分量 |
| z | int | Z 分量 |
| w | int | W 分量 |

## 返回值

无

## 相关接口

- [jegl_rchain_set_uniform_int3](jegl_rchain_set_uniform_int3.md) - 设置三维整型一致变量
- [jegl_rchain_set_uniform_float4](jegl_rchain_set_uniform_float4.md) - 设置四维浮点一致变量
