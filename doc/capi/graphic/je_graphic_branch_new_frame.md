# je_graphic_branch_new_frame

## 函数签名

```c
JE_API void je_graphic_branch_new_frame(
    je_render_branch_t* branch,
    int priority);
```

## 描述

在绘制开始之前，指示绘制组开始新的一帧，并指定优先级。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `branch` | `je_render_branch_t*` | 绘制组指针 |
| `priority` | `int` | 绘制优先级 |

## 返回值

无返回值。

## 用法

此函数用于开始新的一帧渲染，优先级用于控制绘制顺序。

### 示例

```cpp
je_render_branch_t* branch = je_graphic_uhost_alloc_branch(host);

// 每帧开始时调用
je_graphic_branch_new_frame(branch, 0);  // 优先级 0

// 之后通过 je_graphic_cmd_* 录制并提交本帧的绘制命令
```

## 注意事项

- 优先级较低的绘制组会先被渲染
- 必须在向该绘制组提交任何绘制命令之前调用

## 相关接口

- [je_graphic_uhost_alloc_branch](je_graphic_uhost_alloc_branch.md) - 分配绘制组
- [je_graphic_cmd_begin_pass](je_graphic_cmd_begin_pass.md) - 开始渲染 pass
- [je_graphic_cmd_commit](je_graphic_cmd_commit.md) - 提交命令缓冲区
