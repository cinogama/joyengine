# je_cache_file_close

## 函数签名

```c
JE_API void je_cache_file_close(void* file);
```

## 描述

关闭创建出的缓存文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | 缓存文件句柄 |

## 返回值

无返回值。

## 用法

此函数用于关闭通过 `je_cache_file_create` 创建的缓存文件。

### 示例

```cpp
auto* const cache = je_cache_file_create("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入数据...
    je_cache_file_write(data, 1, size, cache);
    
    // 关闭缓存文件
    je_cache_file_close(cache);
}
```

## 注意事项

- 此函数用于关闭 `je_cache_file_create` 创建的文件
- 对于 `je_cache_file_load` 加载的文件，应使用 `je_file_close` 关闭

## 相关接口

- [je_cache_file_create](je_cache_file_create.md) - 创建缓存文件
- [je_cache_file_write](je_cache_file_write.md) - 写入缓存文件
