# jeecs_write_cache_file

## 函数签名

```c
JE_API size_t jeecs_write_cache_file(
    const void* write_buffer,
    size_t elem_size,
    size_t count,
    void* file);
```

## 描述

向已创建的缓存文件中写入若干个指定大小的元素。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `write_buffer` | `const void*` | 要写入的数据缓冲区 |
| `elem_size` | `size_t` | 每个元素的大小（字节） |
| `count` | `size_t` | 要写入的元素数量 |
| `file` | `void*` | 缓存文件句柄 |

## 返回值

| 类型 | 描述 |
|------|------|
| `size_t` | 成功写入的元素数量 |

## 用法

此函数用于向缓存文件写入数据。

### 示例

```cpp
void* cache = jeecs_create_cache_file("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入着色器字节码
    uint8_t bytecode[] = { /* ... */ };
    size_t written = jeecs_write_cache_file(bytecode, 1, sizeof(bytecode), cache);
    
    if (written == sizeof(bytecode)) {
        printf("Cache file written successfully\n");
    }
    
    jeecs_close_cache_file(cache);
}
```

## 注意事项

- 返回值小于 `count` 可能表示写入失败或磁盘空间不足

## 相关接口

- [jeecs_create_cache_file](jeecs_create_cache_file.md) - 创建缓存文件
- [jeecs_close_cache_file](jeecs_close_cache_file.md) - 关闭缓存文件
