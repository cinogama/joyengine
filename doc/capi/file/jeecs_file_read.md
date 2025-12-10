# jeecs_file_read

## 函数签名

```c
JE_API size_t jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file);
```

## 描述

从文件中读取若干个指定大小的元素。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `out_buffer` | `void*` | 用于存储读取数据的缓冲区 |
| `elem_size` | `size_t` | 每个元素的大小（字节） |
| `count` | `size_t` | 要读取的元素数量 |
| `file` | `jeecs_file*` | 指向文件句柄的指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 成功读取的元素数量 |

## 用法

此函数用于从已打开的文件中读取数据。

### 示例

```cpp
jeecs_file* file = jeecs_file_open("@/data/model.bin");
if (file != nullptr) {
    // 读取文件头（假设是一个 struct header）
    struct header {
        uint32_t magic;
        uint32_t version;
    };
    
    struct header hdr;
    size_t read_count = jeecs_file_read(&hdr, sizeof(struct header), 1, file);
    
    if (read_count == 1) {
        printf("Magic: %08X, Version: %u\n", hdr.magic, hdr.version);
    }
    
    jeecs_file_close(file);
}
```

## 注意事项

- 确保 `out_buffer` 有足够的空间存储 `elem_size * count` 字节
- 如果到达文件末尾，返回的数量可能小于请求的数量
- 读取后文件指针会向前移动

## 相关接口

- [jeecs_file_open](jeecs_file_open.md) - 打开文件
- [jeecs_file_tell](jeecs_file_tell.md) - 获取当前位置
- [jeecs_file_seek](jeecs_file_seek.md) - 设置读取位置
