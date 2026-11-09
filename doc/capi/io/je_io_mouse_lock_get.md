# je_io_mouse_lock_get

## 函数签名

```cpp
JE_API bool je_io_mouse_lock_get();
```

## 描述

获取当前是否应该锁定鼠标。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示应该锁定鼠标，false 表示不锁定 |

## 用法示例

```cpp
if (je_io_mouse_lock_get()) {
    // 执行鼠标锁定逻辑
}
```

## 相关接口

- [je_io_mouse_lock_set](je_io_mouse_lock_set.md) - 设置是否锁定鼠标
