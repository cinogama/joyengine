# jegui_update_basic

## 函数签名

```c
JE_API void jegui_update_basic(
    jegui_platform_draw_callback_t platform_draw_callback,
    void* data);
```

## 描述

一帧渲染开始之后，需要调用此接口以完成 ImGUI 的绘制和更新操作。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `platform_draw_callback` | `jegui_platform_draw_callback_t` | 平台绘制回调函数 |
| `data` | `void*` | 传递给回调函数的用户数据 |

### 回调函数类型

```cpp
typedef void (*jegui_platform_draw_callback_t)(void*);
```

## 返回值

无返回值。

## 用法

此函数用于更新和绘制 ImGUI 界面。

### 示例

```cpp
void my_draw_callback(void* data) {
    // 在这里添加 ImGUI 绘制代码
    ImGui::Begin("Test Window");
    ImGui::Text("Hello, World!");
    ImGui::End();
}

// 每帧调用
jegui_update_basic(my_draw_callback, my_data);
```

## 注意事项

- 此接口仅适合用于对接自定义渲染 API 时使用
- 必须在 `jegui_init_basic` 之后调用

## 相关接口

- [jegui_init_basic](jegui_init_basic.md) - 初始化 ImGUI
