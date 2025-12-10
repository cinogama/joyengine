# je_mem_alloc

## 函数签名

```c
JE_API void* je_mem_alloc(size_t sz);
```

## 描述

引擎的统一内存申请函数。用于在引擎内部分配指定大小的内存空间。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `sz` | `size_t` | 需要申请的内存大小（字节数） |

## 返回值

| 类型 | 描述 |
|------|------|
| `void*` | 指向新分配内存的指针；如果分配失败，返回 `nullptr` |

## 用法

此函数是引擎内部统一的内存分配接口，底层实现调用标准库的 `malloc` 函数。建议在引擎相关的代码中使用此函数进行内存分配，以便于统一管理和跟踪内存使用。

### 示例

```cpp
// 分配 1024 字节的内存
void* buffer = je_mem_alloc(1024);
if (buffer != nullptr) {
    // 使用 buffer...
    
    // 使用完毕后释放
    je_mem_free(buffer);
}
```

## 注意事项

- 使用此函数分配的内存必须使用 `je_mem_free` 释放
- 如果申请的大小为 0，行为取决于底层实现

## 相关接口

- [je_mem_realloc](je_mem_realloc.md) - 重新分配内存
- [je_mem_free](je_mem_free.md) - 释放内存
