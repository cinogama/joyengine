# je_file_close

## 函数签名

```c
JE_API void je_file_close(je_file_t* file);
```

## 描述

关闭一个已打开的文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `je_file_t*` | 指向需要关闭的文件句柄的指针 |

## 返回值

无返回值。

## 用法

此函数用于关闭通过 `je_file_open` 打开的文件。

### 示例

```cpp
je_file_t* file = je_file_open("@/data/config.txt");
if (file != nullptr) {
    // 使用文件...
    
    // 关闭文件
    je_file_close(file);
}
```

## 注意事项

- 传入 `nullptr` 时不做任何事
- 关闭后不要再使用该文件指针
- 未关闭的文件会导致资源泄漏

## 相关接口

- [je_file_open](je_file_open.md) - 打开文件
