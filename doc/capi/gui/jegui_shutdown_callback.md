# jegui_shutdown_callback

## 函数签名

```c
JE_API bool jegui_shutdown_callback(void);
```

## 描述

当通过关闭窗口等方式请求关闭图形接口时，可以调用此函数用于询问 ImGUI 注册的退出回调，获取是否确认关闭。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | `true` 表示注册的回调函数不存在或建议关闭图形接口 |

## 用法

此函数用于在用户尝试关闭窗口时进行确认。

### 示例

```cpp
// 当用户点击关闭按钮时
if (jegui_shutdown_callback()) {
    // 可以安全关闭
    close_window();
} else {
    // 回调函数阻止了关闭
    // 可能需要显示确认对话框
}
```

## 注意事项

- 返回 `true` 表示可以安全关闭
- 返回 `false` 表示有回调函数阻止了关闭

## 相关接口

- [jegui_shutdown_basic](jegui_shutdown_basic.md) - 关闭 ImGUI
