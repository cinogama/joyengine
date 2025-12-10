# jegl_uhost_free_branch

## 函数签名

```c
JE_API void jegl_uhost_free_branch(
    jeecs::graphic_uhost* host,
    jeecs::rendchain_branch* free_branch);
```

## 描述

从指定的可编程图形上下文接口释放一个绘制组。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `host` | `jeecs::graphic_uhost*` | 可编程图形上下文接口指针 |
| `free_branch` | `jeecs::rendchain_branch*` | 要释放的绘制组指针 |

## 返回值

无返回值。

## 用法

此函数用于释放通过 `jegl_uhost_alloc_branch` 分配的绘制组。

### 示例

```cpp
jeecs::rendchain_branch* branch = jegl_uhost_alloc_branch(host);

// 使用绘制组...

// 释放
jegl_uhost_free_branch(host, branch);
```

## 注意事项

- 必须在 uhost 关闭之前释放所有分配的绘制组

## 相关接口

- [jegl_uhost_alloc_branch](jegl_uhost_alloc_branch.md) - 分配绘制组
