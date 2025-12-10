# jegl_uhost_get_context

## 函数签名

```c
JE_API jegl_context* jegl_uhost_get_context(jeecs::graphic_uhost* host);
```

## 描述

从指定的可编程图形上下文接口获取图形线程的正式描述符。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `host` | `jeecs::graphic_uhost*` | 可编程图形上下文接口指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_context*` | 图形上下文指针 |

## 用法

此函数用于获取底层的图形上下文，以便进行低级图形操作。

### 示例

```cpp
jeecs::graphic_uhost* host = jegl_uhost_get_or_create_for_universe(universe, nullptr);

// 获取图形上下文
jegl_context* ctx = jegl_uhost_get_context(host);

// 使用上下文加载资源
jegl_shader* shader = jegl_load_shader(ctx, "@/shaders/basic.shader");
```

## 相关接口

- [jegl_uhost_get_or_create_for_universe](jegl_uhost_get_or_create_for_universe.md) - 获取或创建 uhost
