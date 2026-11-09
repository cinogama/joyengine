# je_file_crc64

## 函数签名

```c
JE_API uint64_t je_file_crc64(const char* filepath, uint64_t crc64_continue);
```

## 描述

计算 `filepath` 指定文件内容的 crc64 校验值，路径按 `je_file_open` 的规则解析（支持 `@`/`!` 前缀与镜像回退）。

`crc64_continue` 作为种子传入，返回在其基础上链式计算后的结果；传 `0` 即从头计算。文件不存在或为空时原样返回 `crc64_continue`，因此链式计算时拼接缺失的可选文件不会影响之前的状态。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `filepath` | `const char*` | 文件路径 |
| `crc64_continue` | `uint64_t` | 链式计算的种子值，传 `0` 从头计算 |

## 返回值

| 类型 | 描述 |
|------|------|
| `uint64_t` | 文件内容（连同种子）的 crc64 校验值；文件缺失或为空时返回 `crc64_continue` |

## 示例

```cpp
// 计算单个文件的校验值
uint64_t key = je_file_crc64("@/shader/basic.shader", 0);

// 链式计算多个文件：任一文件变化都会改变结果，
// 可选文件缺失（如导出包中只有缓存）时结果保持之前的链
uint64_t chained = je_file_crc64(
    "@/texture/hero.png.je4texture",
    je_file_crc64("@/texture/hero.png", 0));
```

## 相关接口

- [je_file_open](je_file_open.md) - 路径解析规则
- [je_cache_file_load](je_cache_file_load.md) - 使用校验值验证缓存
- [je_cache_file_create](je_cache_file_create.md) - 使用校验值创建缓存
