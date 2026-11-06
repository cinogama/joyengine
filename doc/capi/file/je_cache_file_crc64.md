# je_cache_file_crc64

## 函数签名

```c
JE_API uint64_t je_cache_file_crc64(const char* filepath, uint64_t crc64_seed);
```

## 描述

以 `crc64_seed` 为初值，按字节继续累加计算 `filepath` 指定文件内容的 crc64 校验值。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `filepath` | `const char*` | 文件路径，路径规则与 `je_file_open` 相同 |
| `crc64_seed` | `uint64_t` | 校验值初值；为 `0` 时结果即为该文件内容本身的校验值 |

## 返回值

| 类型 | 描述 |
|------|------|
| `uint64_t` | 累加后的校验值；文件不存在或为空时原样返回 `crc64_seed` |

## 用法

以前一个文件的结果作为初值，可得到多个文件内容依次拼接后的校验值，用于由多个源文件共同决定的缓存：把结果作为 `je_cache_file_create` 的 `usecrc64` 与 `je_cache_file_load` 的 `virtual_crc64` 传入。

### 示例

```cpp
// 图片纹理缓存的校验码：图片与属性源文件内容依次拼接后的 crc64
uint64_t crc64 = je_cache_file_crc64("@/textures/brick.png", 0);
if (crc64 != 0)
    crc64 = je_cache_file_crc64("@/textures/brick.png.je4texture", crc64);

je_file_t* cache = je_cache_file_load("@/textures/brick.png", format_version, crc64);
```

## 注意事项

- 结果为 `0` 表示源文件缺失或为空；`je_cache_file_create` 拒绝以 `0` 创建缓存，`je_cache_file_load` 在 `virtual_crc64` 为 `0` 且源文件缺失时跳过一致性校验
- 文件缺失与文件为空无法通过返回值区分

## 相关接口

- [je_cache_file_load](je_cache_file_load.md) - 加载缓存文件
- [je_cache_file_create](je_cache_file_create.md) - 创建缓存文件
- [je_image_texture_cache_open_or_create](../graphic/je_image_texture_cache_open_or_create.md) - 打开或生成图片纹理缓存
