# jegl_branch_new_frame

## 函数签名

```c
JE_API void jegl_branch_new_frame(
    jeecs::rendchain_branch* branch,
    int priority);
```

## 描述

在绘制开始之前，指示绘制组开始新的一帧，并指定优先级。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `jeecs::rendchain_branch*` | 绘制组指针 |
| `priority` | `int` | 绘制优先级 |

## 返回值

无返回值。

## 用法

此函数用于开始新的一帧渲染，优先级用于控制绘制顺序。

### 示例

```cpp
jeecs::rendchain_branch* branch = jegl_uhost_alloc_branch(host);

// 每帧开始时调用
jegl_branch_new_frame(branch, 0);  // 优先级 0

// 创建并使用渲染链
jegl_rendchain* chain = jegl_branch_new_chain(branch, nullptr, 0, 0, width, height);
```

## 注意事项

- 优先级较低的绘制组会先被渲染
- 必须在 `jegl_branch_new_chain` 之前调用

## 相关接口

- [jegl_uhost_alloc_branch](jegl_uhost_alloc_branch.md) - 分配绘制组
- [jegl_branch_new_chain](jegl_branch_new_chain.md) - 创建渲染链
