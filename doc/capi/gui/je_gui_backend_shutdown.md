# je_gui_backend_shutdown

## 函数签名

```c
JE_API void je_gui_backend_shutdown(bool reboot);
```

## 描述

图形接口关闭时，请调用此接口关闭 ImGUI。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `reboot` | `bool` | 当前是否正在重启图形接口 |

## 返回值

无返回值。

## 用法

此函数用于关闭 ImGUI 系统。

### 示例

```cpp
// 完全关闭
je_gui_backend_shutdown(false);

// 重启模式
je_gui_backend_shutdown(true);
```

## 注意事项

- `reboot` 参数用于指示当前是否正在重启图形接口，由统一基础图形接口给出
- 此接口仅适合用于对接自定义渲染 API 时使用

## 相关接口

- [je_gui_backend_init](je_gui_backend_init.md) - 初始化 ImGUI
