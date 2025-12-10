# je_io_set_lock_mouse

## 函数签名

```cpp
JE_API void je_io_set_lock_mouse(bool lock);
```

## 描述

设置是否需要将鼠标锁定。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| lock | bool | true 表示锁定鼠标，false 表示解锁鼠标 |

## 返回值

无

## 用法示例

```cpp
// 锁定鼠标（用于第一人称视角控制）
je_io_set_lock_mouse(true);

// 解锁鼠标
je_io_set_lock_mouse(false);
```

## 注意事项

- 此函数设置的是锁定请求，实际锁定由图形后端实现
- 鼠标锁定常用于第一人称游戏或需要无限拖拽的场景

## 相关接口

- [je_io_get_lock_mouse](je_io_get_lock_mouse.md) - 获取当前是否应该锁定鼠标
