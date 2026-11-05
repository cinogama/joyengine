# je_file_size

## 函数签名

```c
JE_API size_t je_file_size(je_file_t* file);
```

## 描述

获取文件总大小（解压后的逻辑大小）。`je_file_t` 的布局由引擎内部维护，应通过此接口读取大小。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_file_t*` | 指向文件句柄的指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 文件大小（字节） |

## 示例

```cpp
je_file_t* file = je_file_open("@/data/config.bin");
if (file != nullptr) {
    size_t size = je_file_size(file);
    std::vector<char> buffer(size);
    je_file_read(buffer.data(), 1, size, file);
    je_file_close(file);
}
```

## 相关接口

- [je_file_open](je_file_open.md) - 打开文件
- [je_file_read](je_file_read.md) - 读取文件内容
