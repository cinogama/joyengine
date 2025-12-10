# jeecs_close_cache_file

## 函数签名

```c
JE_API void jeecs_close_cache_file(void* file);
```

## 描述

关闭创建出的缓存文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `void*` | 缓存文件句柄 |

## 返回值

无返回值。

## 用法

此函数用于关闭通过 `jeecs_create_cache_file` 创建的缓存文件。

### 示例

```cpp
void* cache = jeecs_create_cache_file("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入数据...
    jeecs_write_cache_file(data, 1, size, cache);
    
    // 关闭缓存文件
    jeecs_close_cache_file(cache);
}
```

## 注意事项

- 此函数用于关闭 `jeecs_create_cache_file` 创建的文件
- 对于 `jeecs_load_cache_file` 加载的文件，应使用 `jeecs_file_close` 关闭

## 相关接口

- [jeecs_create_cache_file](jeecs_create_cache_file.md) - 创建缓存文件
- [jeecs_write_cache_file](jeecs_write_cache_file.md) - 写入缓存文件
