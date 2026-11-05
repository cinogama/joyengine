# je_file_seek

## 函数签名

```c
JE_API bool je_file_seek(je_file_t* file, int64_t offset, je_file_seek_mode_t mode);
```

## 描述

按照指定的模式，对文件即将读取的位置进行偏移和跳转。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_file_t*` | 指向文件句柄的指针 |
| `offset` | `int64_t` | 偏移量（可为负数） |
| `mode` | `je_file_seek_mode_t` | 偏移模式 |

### 偏移模式

| 模式 | 值 | 描述 |
|------|-----|------|
| `JE_READ_FILE_SEEK_SET` | 0 | 从文件开头偏移 |
| `JE_READ_FILE_SEEK_CURRENT` | 1 | 从当前位置偏移 |
| `JE_READ_FILE_SEEK_END` | 2 | 从文件末尾偏移 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`；参数非法或底层跳转失败返回 `false` |

镜像文件的目标位置会被钳制在 `[0, 文件大小]`：负偏移从 0 开始，超出末尾则停在末尾。

## 用法

此函数用于在文件中定位读取位置。

### 示例

```cpp
je_file_t* file = je_file_open("@/data/test.bin");
if (file != nullptr) {
    // 跳转到文件开头后 100 字节处
    je_file_seek(file, 100, JE_READ_FILE_SEEK_SET);
    
    // 向后跳过 50 字节
    je_file_seek(file, 50, JE_READ_FILE_SEEK_CURRENT);
    
    // 跳转到文件末尾前 10 字节处
    je_file_seek(file, -10, JE_READ_FILE_SEEK_END);
    
    je_file_close(file);
}
```

## 相关接口

- [je_file_tell](je_file_tell.md) - 获取当前位置
- [je_file_read](je_file_read.md) - 读取文件内容
