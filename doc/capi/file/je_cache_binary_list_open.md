# je_cache_binary_list_open

## 函数签名

```c
JE_API bool je_cache_binary_list_open(
    je_file_t* file,
    je_cache_binary_list_t* out_list);
```

## 描述

从文件当前位置读取一个 LIST 标记并初始化列表句柄

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_file_t*` | — |
| `out_list` | `je_cache_binary_list_t*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 见描述。 |

## 注意事项

- 当前位置不是 LIST 标记时返回 false
- 成功时 out_list 被填入列表句柄，文件位置移动到列表的第一个元素

## 相关接口

- [je_cache_binary_list_index](je_cache_binary_list_index.md) - 获取列表中指定下标的值
- [je_cache_binary_document_open](je_cache_binary_document_open.md) - 从文件当前位置读取一个 DOCUMENT 标记并初始化文档句柄
