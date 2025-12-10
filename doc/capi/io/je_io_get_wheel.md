# je_io_get_wheel

## 函数签名

```cpp
JE_API void je_io_get_wheel(size_t group, float* out_x, float* out_y);
```

## 描述

获取鼠标滚轮的计数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标分组索引 |
| out_x | float* | 输出参数，接收水平滚轮增量 |
| out_y | float* | 输出参数，接收垂直滚轮增量 |

## 返回值

无（通过输出参数返回）

## 用法示例

```cpp
float wheelX, wheelY;
je_io_get_wheel(0, &wheelX, &wheelY);
// wheelY > 0 表示向上滚动，wheelY < 0 表示向下滚动
```

## 相关接口

- [je_io_update_wheel](je_io_update_wheel.md) - 更新滚轮状态
