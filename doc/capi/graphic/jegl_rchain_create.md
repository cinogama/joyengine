# jegl_rchain_create

## 函数签名

```c
JE_API jegl_rendchain* jegl_rchain_create();
```

## 描述

创建绘制链实例。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_rendchain*` | 绘制链实例指针 |

## 用法

RendChain 是引擎提供的一致绘制操作接口，支持不同线程提交不同的渲染链，并最终完成渲染。

### 绘制链使用流程

1. 创建：`jegl_rchain_create`
2. 绑定缓冲区：`jegl_rchain_begin`
3. 清除缓冲区（可选）：`jegl_rchain_clear_color_buffer` / `jegl_rchain_clear_depth_buffer`
4. 绑定 UBO（可选）：`jegl_rchain_bind_uniform_buffer`
5. 进行若干次绘制：`jegl_rchain_draw`
6. 提交绘制链：`jegl_rchain_commit`
7. 如果需要复用，从第 2 步重新开始；否则销毁：`jegl_rchain_close`

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();

// 开始绘制
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);

// 清除缓冲区
float clear_color[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
jegl_rchain_clear_color_buffer(chain, 0, clear_color);
jegl_rchain_clear_depth_buffer(chain, 1.0f);

// 绘制
jegl_rchain_texture_group* tex_group = jegl_rchain_allocate_texture_group(chain);
jegl_rchain_bind_texture(chain, tex_group, 0, texture);

jegl_rendchain_rend_action* act = jegl_rchain_draw(chain, shader, mesh, tex_group);
jegl_rchain_set_uniform_float4x4(act, nullptr, mvp);

// 提交
jegl_rchain_commit(chain, ctx);

// 销毁
jegl_rchain_close(chain);
```

## 相关接口

- [jegl_rchain_begin](jegl_rchain_begin.md) - 绑定绘制目标
- [jegl_rchain_draw](jegl_rchain_draw.md) - 绘制操作
- [jegl_rchain_commit](jegl_rchain_commit.md) - 提交绘制链
- [jegl_rchain_close](jegl_rchain_close.md) - 销毁绘制链
