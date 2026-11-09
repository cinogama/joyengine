# je_animation_sample_buffer_apply

## 函数签名

```c
JE_API void je_animation_sample_buffer_apply(
    const je_game_entity_t* entity,
    je_animation_sample_buffer_t* buffer);
```

## 描述

把缓冲中的采样结果写入实体：组件成员轨道直接写成员值，uniform 轨道经 Renderer::Shaders 写自定义 uniform。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `entity` | `const je_game_entity_t*` | 目标实体 |
| `buffer` | `je_animation_sample_buffer_t*` | 采样累加缓冲 |

## 返回值

| 类型 | 描述 |
|------|------|
| ``void`` | — |

## 注意事项

- 组件成员轨道要求实体持有对应组件，否则该轨道无效果
- uniform 轨道要求实体持有 Renderer::Shaders 组件
- uniform 轨道修改的是共享的着色器资源（影响所有使用该着色器的实体）
- 叠加（offset）轨道在原值上 +=，QUAT4 为四元数相乘
