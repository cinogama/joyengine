# je_cache_file_close

## 函数签名

```c
JE_API void je_cache_file_close(void* file, bool finished);
```

## 描述

关闭创建出的缓存文件。`finished` 指示缓存是否已完整写入：

- `true`：关闭前回写缓存主版本号，使缓存生效（`je_cache_file_create` 创建时主版本号留空占位）
- `false`：直接关闭且不回写主版本号，这样关闭的缓存文件无法通过 `je_cache_file_load` 的校验，始终无效

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_writing_cache_file_t*` | 缓存文件句柄 |
| `finished` | `bool` | 缓存内容是否已完整写入 |

## 返回值

无返回值。

## 用法

此函数用于关闭通过 `je_cache_file_create` 创建的缓存文件：写入完整时传 `true` 使缓存生效；写入失败时传 `false`，使缓存文件保持无效，下次加载时会重新生成。

### 示例

```cpp
auto* const cache = je_cache_file_create("@/shader/basic.shader", 1, 0);

if (cache != nullptr) {
    // 写入数据...
    const size_t written = je_cache_file_write(data, 1, size, cache);

    // 写入完整传 true 使缓存生效，失败传 false 保持缓存无效
    je_cache_file_close(cache, written == size);
}
```

## 注意事项

- 此函数用于关闭 `je_cache_file_create` 创建的文件
- 对于 `je_cache_file_load` 加载的文件，应使用 `je_file_close` 关闭
- `finished == false` 关闭的缓存文件始终无效

## 相关接口

- [je_cache_file_create](je_cache_file_create.md) - 创建缓存文件
- [je_cache_file_write](je_cache_file_write.md) - 写入缓存文件
