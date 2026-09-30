# je_font_load

## 函数签名

```cpp
JE_API je_font* je_font_load(
    const char* font_path,
    size_t board_blank_size_x,
    size_t board_blank_size_y,
    je_font_char_updater_t char_texture_updater);
```

## 描述

加载一个字体文件。

字体对象与字号解耦：加载时不需要指定字号，字号在调用 [je_font_get_char](je_font_get_char.md) 时给出，同一字体对象可以按任意字号渲染字符。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| font_path | const char* | 字体文件路径 |
| board_blank_size_x | size_t | 字符板 X 方向边距 |
| board_blank_size_y | size_t | 字符板 Y 方向边距 |
| char_texture_updater | je_font_char_updater_t | 字符纹理更新回调函数 |

## 返回值

| 类型 | 描述 |
|------|------|
| je_font* | 字体对象指针，失败返回 nullptr |

## 相关接口

- [je_font_free](je_font_free.md) - 释放字体
- [je_font_get_char](je_font_get_char.md) - 获取字符信息
