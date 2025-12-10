# jeecs_file_open

## 函数签名

```c
JE_API jeecs_file* jeecs_file_open(const char* path);
```

## 描述

从指定路径打开一个文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs_file*` | 指向打开的文件句柄的指针；若打开失败返回 `nullptr` |

## 用法

此函数用于打开文件进行读取操作。

### 路径前缀

| 前缀 | 描述 |
|------|------|
| `@` | 运行时目录（优先加载实际存在的文件，然后尝试默认镜像） |
| `!` | 可执行文件所在目录 |

### 示例

```cpp
// 打开运行时目录下的文件
jeecs_file* file = jeecs_file_open("@/config/settings.json");

if (file != nullptr) {
    // 获取文件大小
    size_t file_size = file->m_file_length;
    
    // 读取文件内容
    char* buffer = (char*)je_mem_alloc(file_size + 1);
    jeecs_file_read(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    
    // 使用文件内容...
    
    je_mem_free(buffer);
    jeecs_file_close(file);
}
```

## 注意事项

- 若文件成功打开，使用完毕后需要使用 `jeecs_file_close` 关闭
- 以 `@` 开头的路径会优先查找实际文件，如不存在则尝试从默认镜像中读取
- 文件以只读二进制模式打开

## 相关接口

- [jeecs_file_close](jeecs_file_close.md) - 关闭文件
- [jeecs_file_read](jeecs_file_read.md) - 读取文件内容
- [jeecs_file_set_runtime_path](jeecs_file_set_runtime_path.md) - 设置运行时路径
