# jeecs_create_cache_file

## 函数签名

```c
JE_API void* jeecs_create_cache_file(
    const char* filepath,
    uint32_t format_version,
    wo_integer_t usecrc64);
```

## 描述

为 `filepath` 指定的文件创建缓存文件，将覆盖已有的缓存文件（如果已有的话）。若创建文件失败，则返回 `nullptr`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `filepath` | `const char*` | 原始文件路径 |
| `format_version` | `uint32_t` | 缓存文件格式版本 |
| `usecrc64` | `wo_integer_t` | 校验值设置 |

### usecrc64 参数说明

| 值 | 描述 |
|----|------|
| `0` | 自动计算 `filepath` 指定文件的校验值 |
| 其他 | 直接使用指定值作为校验值 |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 缓存文件句柄；若创建失败返回 `nullptr` |

## 用法

此函数用于创建缓存文件，创建后使用 `jeecs_write_cache_file` 写入数据。

### 示例

```cpp
// 为着色器文件创建缓存
void* cache = jeecs_create_cache_file("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入编译后的着色器数据
    const char* compiled_data = /* ... */;
    size_t data_size = /* ... */;
    
    jeecs_write_cache_file(compiled_data, 1, data_size, cache);
    
    // 关闭缓存文件
    jeecs_close_cache_file(cache);
}
```

## 注意事项

- 创建后的缓存文件需要使用 `jeecs_close_cache_file` 关闭
- 使用 `jeecs_write_cache_file` 写入数据
- 会覆盖已存在的同名缓存文件

## 相关接口

- [jeecs_write_cache_file](jeecs_write_cache_file.md) - 写入缓存文件
- [jeecs_close_cache_file](jeecs_close_cache_file.md) - 关闭缓存文件
- [jeecs_load_cache_file](jeecs_load_cache_file.md) - 加载缓存文件
