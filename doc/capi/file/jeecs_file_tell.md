# jeecs_file_tell

## 函数签名

```c
JE_API size_t jeecs_file_tell(jeecs_file* file);
```

## 描述

获取当前文件下一个读取的位置偏移量。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `jeecs_file*` | 指向文件句柄的指针 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 当前读取位置的偏移量（字节） |

## 用法

此函数用于获取当前在文件中的读取位置。

### 示例

```cpp
jeecs_file* file = jeecs_file_open("@/data/test.bin");
if (file != nullptr) {
    // 读取一些数据
    char buffer[100];
    jeecs_file_read(buffer, 1, 100, file);
    
    // 获取当前位置
    size_t pos = jeecs_file_tell(file);
    printf("Current position: %zu\n", pos);  // 输出: 100
    
    jeecs_file_close(file);
}
```

## 相关接口

- [jeecs_file_seek](jeecs_file_seek.md) - 设置读取位置
- [jeecs_file_read](jeecs_file_read.md) - 读取文件内容
