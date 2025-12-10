# jegui_set_font

## 函数签名

```c
JE_API void jegui_set_font(
    const char* general_font_path,
    const char* latin_font_path,
    size_t size);
```

## 描述

让 ImGUI 使用指定的路径和字体大小设置。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `general_font_path` | `const char*` | 通用字体路径（可为 `nullptr`） |
| `latin_font_path` | `const char*` | 拉丁字符集字体路径 |
| `size` | `size_t` | 字体大小 |

## 返回值

无返回值。

## 用法

此函数用于设置 ImGUI 使用的字体。

### 示例

```cpp
// 使用默认字体
jegui_set_font(nullptr, nullptr, 16);

// 使用自定义中文字体
jegui_set_font("@/fonts/chinese.ttf", "@/fonts/latin.ttf", 18);
```

## 注意事项

- 若 `general_font_path` 是空指针，则使用默认字体
- 若 `general_font_path` 非空，则针对拉丁字符集使用 `latin_font_path` 指示的字体
- 仅在 `jegui_init_basic` 前调用生效
- 此接口仅适合用于对接自定义渲染 API 时使用

## 相关接口

- [jegui_init_basic](jegui_init_basic.md) - 初始化 ImGUI
