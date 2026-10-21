# jeecs_fimg_writer_pack_buffer

## 函数签名

```c
JE_API bool jeecs_fimg_writer_pack_buffer(
    jeecs_fimg_writer* writer,
    const void* buffer,
    size_t len,
    const char* archive_path);
```

## 描述

将一段内存内容作为文件写入正在创建的镜像。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `writer` | `jeecs_fimg_writer*` | writer 句柄 |
| `buffer` | `const void*` | 数据指针；`len == 0` 时可为 `nullptr` |
| `len` | `size_t` | 数据长度（字节） |
| `archive_path` | `const char*` | 镜像内的键 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`；参数非法、路径重复、writer 已结束/失败时返回 `false` |

## 示例

```cpp
const char* config = "{ \"version\": 1 }";
jeecs_fimg_writer_pack_buffer(writer, config, strlen(config), "@/config/game.json");

// 空文件
jeecs_fimg_writer_pack_buffer(writer, nullptr, 0, "@/empty.txt");
```

## 注意事项

- 文件是否压缩由 writer 的压缩选项和归档路径的扩展名决定

## 相关接口

- [jeecs_fimg_writer_pack_file](jeecs_fimg_writer_pack_file.md)
- [jeecs_fimg_writer_finish](jeecs_fimg_writer_finish.md)
