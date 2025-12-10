# je_mem_realloc

## 函数签名

```c
JE_API void* je_mem_realloc(void* mem, size_t sz);
```

## 描述

引擎的统一内存重新申请函数。用于调整已分配内存块的大小。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `mem` | `void*` | 指向先前使用 `je_mem_alloc` 或 `je_mem_realloc` 分配的内存块的指针 |
| `sz` | `size_t` | 新的内存大小（字节数） |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向重新分配后内存的指针；如果分配失败，返回 `nullptr`，原内存块保持不变 |

## 用法

此函数用于调整已分配内存块的大小。如果新大小大于原大小，新增部分的内容是未定义的；如果新大小小于原大小，超出部分的数据将丢失。

### 示例

```cpp
// 初始分配 100 字节
void* buffer = je_mem_alloc(100);

// 需要更大的空间，扩展到 200 字节
void* new_buffer = je_mem_realloc(buffer, 200);
if (new_buffer != nullptr) {
    buffer = new_buffer;
    // 继续使用 buffer...
}

// 使用完毕后释放
je_mem_free(buffer);
```

## 注意事项

- 如果 `mem` 为 `nullptr`，此函数的行为等同于 `je_mem_alloc(sz)`
- 如果 `sz` 为 0 且 `mem` 不为 `nullptr`，此函数的行为等同于 `je_mem_free(mem)`
- 重新分配可能会返回不同的地址，原指针在成功重分配后将失效
- 如果重分配失败，原内存块不会被释放

## 相关接口

- [je_mem_alloc](je_mem_alloc.md) - 分配内存
- [je_mem_free](je_mem_free.md) - 释放内存
