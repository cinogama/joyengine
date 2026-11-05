# je_cache_binary_document_open

## 函数签名

```c
JE_API bool je_cache_binary_document_open(
    jeecs_file* file,
    jeecs_cache_binary_document* out_doc);
```

## 描述

从文件当前位置读取一个 DOCUMENT 标记并初始化文档句柄

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `jeecs_file*` | — |
| `out_doc` | `jeecs_cache_binary_document*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 当前位置不是 DOCUMENT 标记时返回 false
- 成功时 out_doc 被填入文档句柄，文件位置移动到文档的第一个键

## 相关接口

- [je_cache_binary_document_index](je_cache_binary_document_index.md) - 查找文档中指定键对应的值
- [je_cache_binary_list_open](je_cache_binary_list_open.md) - 从文件当前位置读取一个 LIST 标记并初始化列表句柄
