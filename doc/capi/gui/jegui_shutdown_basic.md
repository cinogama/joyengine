# jegui_shutdown_basic

## 函数签名

```c
JE_API void jegui_shutdown_basic(bool reboot);
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
jegui_shutdown_basic(false);

// 重启模式
jegui_shutdown_basic(true);
```

## 注意事项

- `reboot` 参数用于指示当前是否正在重启图形接口，由统一基础图形接口给出
- 此接口仅适合用于对接自定义渲染 API 时使用

## 相关接口

- [jegui_init_basic](jegui_init_basic.md) - 初始化 ImGUI
