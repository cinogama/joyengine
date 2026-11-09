# je_cache_file_create

## 函数签名

```c
JE_API je_writing_cache_file_t* je_cache_file_create(
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
| `je_writing_cache_file_t*` | 缓存文件句柄；若创建失败返回 `nullptr` |

## 用法

此函数用于创建缓存文件，创建后使用 `je_cache_file_write` 写入数据。

### 示例

```cpp
// 为着色器文件创建缓存
auto* const cache = je_cache_file_create("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入编译后的着色器数据
    const char* compiled_data = /* ... */;
    size_t data_size = /* ... */;
    
    const size_t written = je_cache_file_write(compiled_data, 1, data_size, cache);
    
    // 写入完整传 true 使缓存生效，失败传 false 保持缓存无效
    je_cache_file_close(cache, written == data_size);
}
```

## 注意事项

- 创建时缓存主版本号留空占位，此时缓存文件始终无效，直到 `je_cache_file_close` 以 `finished == true` 关闭回写主版本号后缓存才生效
- 创建后的缓存文件需要使用 `je_cache_file_close` 关闭
- 使用 `je_cache_file_write` 写入数据
- 会覆盖已存在的同名缓存文件

## 相关接口

- [je_cache_file_write](je_cache_file_write.md) - 写入缓存文件
- [je_cache_file_close](je_cache_file_close.md) - 关闭缓存文件
- [je_cache_file_load](je_cache_file_load.md) - 加载缓存文件
