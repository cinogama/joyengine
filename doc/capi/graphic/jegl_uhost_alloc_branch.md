# jegl_uhost_alloc_branch

## 函数签名

```c
JE_API jeecs::rendchain_branch* jegl_uhost_alloc_branch(
    jeecs::graphic_uhost* host);
```

## 描述

从指定的可编程图形上下文接口申请一个绘制组。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `host` | `jeecs::graphic_uhost*` | 可编程图形上下文接口指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs::rendchain_branch*` | 绘制组指针 |

## 用法

绘制组用于组织和管理多个渲染链。

### 示例

```cpp
jeecs::graphic_uhost* host = jegl_uhost_get_or_create_for_universe(universe, nullptr);

// 分配绘制组
jeecs::rendchain_branch* branch = jegl_uhost_alloc_branch(host);

// 开始新帧
jegl_branch_new_frame(branch, 0);

// 创建渲染链
jegl_rendchain* chain = jegl_branch_new_chain(branch, nullptr, 0, 0, width, height);

// 使用完毕后释放
jegl_uhost_free_branch(host, branch);
```

## 注意事项

- 所有申请出的绘制组都需要在对应 uhost 关闭之前，通过 `jegl_uhost_free_branch` 释放

## 相关接口

- [jegl_uhost_free_branch](jegl_uhost_free_branch.md) - 释放绘制组
- [jegl_branch_new_frame](jegl_branch_new_frame.md) - 开始新帧
- [jegl_branch_new_chain](jegl_branch_new_chain.md) - 创建渲染链
