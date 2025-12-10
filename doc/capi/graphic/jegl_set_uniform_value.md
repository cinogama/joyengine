# jegl_set_uniform_value

## 函数签名

```c
JE_API void jegl_set_uniform_value(
    uint32_t location,
    jegl_shader::uniform_type type,
    const void* data);
```

## 描述

向当前绑定着色器的指定位置的一致变量设置一个指定类型的数值。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `location` | `uint32_t` | 一致变量位置 |
| `type` | `jegl_shader::uniform_type` | 数据类型 |
| `data` | `const void*` | 数据指针 |

### 数据类型

常见的 uniform 类型包括：
- `INT` / `INT2` / `INT3` / `INT4` - 整型
- `FLOAT` / `FLOAT2` / `FLOAT3` / `FLOAT4` - 浮点型
- `FLOAT2X2` / `FLOAT3X3` / `FLOAT4X4` - 矩阵

## 返回值

无返回值。

## 用法

此函数用于设置着色器的 uniform 变量值。

### 示例

```cpp
jegl_bind_shader(shader);

// 设置 MVP 矩阵
float mvp[4][4] = { /* ... */ };
jegl_set_uniform_value(0, jegl_shader::FLOAT4X4, mvp);

// 设置颜色
float color[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
jegl_set_uniform_value(1, jegl_shader::FLOAT4, color);
```

## 注意事项

- 必须使用 `jegl_bind_shader` 在当前帧内事先绑定一个着色器
- 此函数只允许在图形线程内调用
- `data` 指向的数据必须至少包含 `type` 所指定的类型所需的字节数
- 请确保 `type` 和 `data` 指向的数据类型一致，否则结果未定义

## 相关接口

- [jegl_bind_shader](jegl_bind_shader.md) - 绑定着色器
