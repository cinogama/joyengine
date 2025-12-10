# jeecs_file_seek

## 函数签名

```c
JE_API void jeecs_file_seek(jeecs_file* file, int64_t offset, je_read_file_seek_mode mode);
```

## 描述

按照指定的模式，对文件即将读取的位置进行偏移和跳转。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `jeecs_file*` | 指向文件句柄的指针 |
| `offset` | `int64_t` | 偏移量（可为负数） |
| `mode` | `je_read_file_seek_mode` | 偏移模式 |

### 偏移模式

| 模式 | 值 | 描述 |
|------|-----|------|
| `JE_READ_FILE_SEEK_SET` | 0 | 从文件开头偏移 |
| `JE_READ_FILE_SEEK_CURRENT` | 1 | 从当前位置偏移 |
| `JE_READ_FILE_SEEK_END` | 2 | 从文件末尾偏移 |

## 返回值

无返回值。

## 用法

此函数用于在文件中定位读取位置。

### 示例

```cpp
jeecs_file* file = jeecs_file_open("@/data/test.bin");
if (file != nullptr) {
    // 跳转到文件开头后 100 字节处
    jeecs_file_seek(file, 100, JE_READ_FILE_SEEK_SET);
    
    // 向后跳过 50 字节
    jeecs_file_seek(file, 50, JE_READ_FILE_SEEK_CURRENT);
    
    // 跳转到文件末尾前 10 字节处
    jeecs_file_seek(file, -10, JE_READ_FILE_SEEK_END);
    
    jeecs_file_close(file);
}
```

## 相关接口

- [jeecs_file_tell](jeecs_file_tell.md) - 获取当前位置
- [jeecs_file_read](jeecs_file_read.md) - 读取文件内容
