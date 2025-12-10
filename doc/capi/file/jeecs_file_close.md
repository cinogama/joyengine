# jeecs_file_close

## 函数签名

```c
JE_API void jeecs_file_close(jeecs_file* file);
```

## 描述

关闭一个已打开的文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `file` | `jeecs_file*` | 指向需要关闭的文件句柄的指针 |

## 返回值

无返回值。

## 用法

此函数用于关闭通过 `jeecs_file_open` 打开的文件。

### 示例

```cpp
jeecs_file* file = jeecs_file_open("@/data/config.txt");
if (file != nullptr) {
    // 使用文件...
    
    // 关闭文件
    jeecs_file_close(file);
}
```

## 注意事项

- 关闭后不要再使用该文件指针
- 未关闭的文件会导致资源泄漏

## 相关接口

- [jeecs_file_open](jeecs_file_open.md) - 打开文件
