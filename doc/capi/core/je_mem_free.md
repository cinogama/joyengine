# je_mem_free

## 函数签名

```c
JE_API void je_mem_free(void* ptr);
```

## 描述

引擎的统一内存释放函数。用于释放先前通过 `je_mem_alloc` 或 `je_mem_realloc` 分配的内存。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `ptr` | `void*` | 指向需要释放的内存块的指针 |

## 返回值

无返回值。

## 用法

此函数用于释放引擎分配的内存。释放后，指针指向的内存区域不再有效，不应再被访问。

### 示例

```cpp
// 分配内存
void* buffer = je_mem_alloc(1024);

// 使用 buffer...

// 释放内存
je_mem_free(buffer);
buffer = nullptr;  // 建议将指针置空，避免悬空指针
```

## 注意事项

- 只能释放由 `je_mem_alloc` 或 `je_mem_realloc` 分配的内存
- 释放 `nullptr` 是安全的，不会产生任何效果
- 不要重复释放同一块内存（双重释放）
- 释放后不要再访问该内存区域

## 相关接口

- [je_mem_alloc](je_mem_alloc.md) - 分配内存
- [je_mem_realloc](je_mem_realloc.md) - 重新分配内存
