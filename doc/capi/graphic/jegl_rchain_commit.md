# jegl_rchain_commit

## 函数签名

```c
JE_API void jegl_rchain_commit(
    jegl_rendchain* chain,
    jegl_context* glthread);
```

## 描述

将指定的绘制链在图形线程中进行提交。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |
| `glthread` | `jegl_context*` | 图形上下文指针 |

## 返回值

无返回值。

## 用法

此函数用于执行绘制链中的所有绘制操作。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();

// 配置绘制链
jegl_rchain_begin(chain, nullptr, 0, 0, width, height);
jegl_rchain_clear_color_buffer(chain, 0, clear_color);
jegl_rchain_clear_depth_buffer(chain, 1.0f);

// 添加绘制操作
jegl_rchain_draw(chain, shader, mesh, tex_group);

// 提交绘制链
jegl_rchain_commit(chain, ctx);

// 销毁或复用
jegl_rchain_close(chain);
```

## 注意事项

- 此函数只允许在图形线程内调用
- 提交后绘制链可以被复用或销毁

## 相关接口

- [jegl_rchain_create](jegl_rchain_create.md) - 创建绘制链
- [jegl_rchain_close](jegl_rchain_close.md) - 销毁绘制链
