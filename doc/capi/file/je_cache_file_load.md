# je_cache_file_load

## 函数签名

```c
JE_API je_file_t* je_cache_file_load(
    const char* filepath,
    uint32_t format_version,
    wo_integer_t virtual_crc64);
```

## 描述

尝试读取 `filepath` 对应的缓存文件，将校验缓存文件的格式版本和校验码。缓存文件存在且校验通过则成功读取，否则返回 `nullptr`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `filepath` | `const char*` | 缓存文件路径 |
| `format_version` | `uint32_t` | 缓存文件格式版本 |
| `virtual_crc64` | `wo_integer_t` | 校验值设置 |

### virtual_crc64 参数说明

| 值 | 描述 |
|----|------|
| `-1` | 忽略原始文件一致性校验 |
| `0` | 使用 `filepath` 指定文件的内容计算校验值 |
| 其他 | 直接使用指定值作为校验值 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 缓存文件句柄；若加载失败返回 `nullptr` |

## 用法

此函数用于加载缓存文件，返回的文件句柄可使用 `je_file_read` 读取数据。

### 示例

```cpp
// 加载缓存文件，自动计算校验值
je_file_t* cache = je_cache_file_load("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 读取缓存数据
    char buffer[1024];
    size_t read = je_file_read(buffer, 1, 1024, cache);
    
    // 关闭文件
    je_file_close(cache);
}

// 加载缓存文件，忽略校验
je_file_t* cache2 = je_cache_file_load("@/shader/basic.shader", 1, -1);
```

## 注意事项

- 打开后的缓存文件需要使用 `je_file_close` 关闭
- 使用 `je_file_read` 读取数据

## 相关接口

- [je_cache_file_create](je_cache_file_create.md) - 创建缓存文件
- [je_file_read](je_file_read.md) - 读取文件内容
- [je_file_close](je_file_close.md) - 关闭文件
