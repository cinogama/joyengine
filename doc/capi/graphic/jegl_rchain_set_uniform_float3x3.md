# jegl_rchain_set_uniform_float3x3

## 函数签名

```cpp
JE_API void jegl_rchain_set_uniform_float3x3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place_may_null,
    const float (*mat)[3]);
```

## 描述

为 act 指定的绘制操作应用 3x3 单精度浮点数矩阵一致变量。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| act | jegl_rendchain_rend_action* | 渲染动作 |
| binding_place_may_null | const uint32_t* | 绑定位置，可为空 |
| mat | const float (*)[3] | 3x3 矩阵数据 |

## 返回值

无

## 相关接口

- [jegl_rchain_set_uniform_float2x2](jegl_rchain_set_uniform_float2x2.md) - 设置 2x2 矩阵一致变量
- [jegl_rchain_set_uniform_float4x4](jegl_rchain_set_uniform_float4x4.md) - 设置 4x4 矩阵一致变量
