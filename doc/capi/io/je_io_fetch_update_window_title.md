# je_io_fetch_update_window_title

## 函数签名

```cpp
JE_API bool je_io_fetch_update_window_title(const char** out_title);
```

## 描述

获取当前是否需要对窗口标题进行调整及调整之后的内容。此函数供图形后端调用，以检查是否有窗口标题调整请求。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| out_title | const char** | 输出参数，接收目标窗口标题字符串指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| bool | true 表示有调整请求，false 表示无请求 |

## 用法示例

```cpp
const char* newTitle;
if (je_io_fetch_update_window_title(&newTitle)) {
    // 有窗口标题调整请求，执行调整
    // ... 设置窗口标题为 newTitle
}
```

## 注意事项

- 此函数是一次性获取，调用后请求会被清除
- 通常由图形后端在主循环中调用

## 相关接口

- [je_io_set_window_title](je_io_set_window_title.md) - 请求调整窗口标题
