# je_font_get_char

## 函数签名

```cpp
JE_API const jeecs::graphic::character* je_font_get_char(
    je_font* font,
    float size,
    char32_t unicode32_char);
```

## 描述

以指定的字号从字体中加载指定的一个字符的纹理及其他信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| font | je_font* | 字体对象 |
| size | float | 字号，单位像素，必须大于 0 |
| unicode32_char | char32_t | Unicode32 编码的字符 |

## 返回值

| 类型 | 描述 |
|------|------|
| const jeecs::graphic::character* | 字符信息，包含纹理和布局数据 |

## 注意事项

- 返回的字符信息结构包含字符的纹理坐标、大小、偏移等渲染所需信息
- 同一（字号, 字符）的字形会被缓存，重复获取直接返回缓存结果；返回指针在 je_font_free 之前保持有效
- 参见 `jeecs::graphic::character` 结构定义

## 相关接口

- [je_font_load](je_font_load.md) - 加载字体
- [je_font_free](je_font_free.md) - 释放字体
