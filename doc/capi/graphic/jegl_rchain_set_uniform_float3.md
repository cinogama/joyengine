# jegl_rchain_set_uniform_float3

## 函数签名

```cpp
JE_API void jegl_rchain_set_uniform_float3(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place_may_null,
    float x,
    float y,
    float z);
```

## 描述

为 act 指定的绘制操作应用三维单精度浮点数矢量一致变量。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| act | jegl_rendchain_rend_action* | 渲染动作 |
| binding_place_may_null | const uint32_t* | 绑定位置，可为空 |
| x | float | X 分量 |
| y | float | Y 分量 |
| z | float | Z 分量 |

## 返回值

无

## 相关接口

- [jegl_rchain_set_uniform_float2](jegl_rchain_set_uniform_float2.md) - 设置二维浮点一致变量
- [jegl_rchain_set_uniform_float4](jegl_rchain_set_uniform_float4.md) - 设置四维浮点一致变量
