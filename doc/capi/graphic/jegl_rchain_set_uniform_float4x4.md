# jegl_rchain_set_uniform_float4x4

## 函数签名

```c
JE_API void jegl_rchain_set_uniform_float4x4(
    jegl_rendchain_rend_action* act,
    const uint32_t* binding_place_may_null,
    const float (*mat)[4]);
```

## 描述

为指定的绘制操作应用 4x4 单精度浮点数矩阵一致变量。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `act` | `jegl_rendchain_rend_action*` | 绘制操作对象指针 |
| `binding_place_may_null` | `const uint32_t*` | 绑定位置（可为 `nullptr`） |
| `mat` | `const float (*)[4]` | 4x4 矩阵数据 |

## 返回值

无返回值。

## 用法

此函数用于设置 MVP 矩阵、变换矩阵等。

### 示例

```cpp
jegl_rendchain_rend_action* act = jegl_rchain_draw(chain, shader, mesh, tex_group);

// 设置模型矩阵
float model_matrix[4][4] = { /* ... */ };
jegl_rchain_set_uniform_float4x4(act, nullptr, model_matrix);

// 或指定绑定位置
uint32_t mvp_location = 0;
jegl_rchain_set_uniform_float4x4(act, &mvp_location, mvp_matrix);
```

## 注意事项

- 如果 `binding_place_may_null` 为 `nullptr`，使用默认绑定位置

## 相关接口

- [jegl_rchain_set_uniform_float3x3](jegl_rchain_set_uniform_float3x3.md) - 设置 3x3 矩阵
- [jegl_rchain_set_uniform_float4](jegl_rchain_set_uniform_float4.md) - 设置四维浮点数
