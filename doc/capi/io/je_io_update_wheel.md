# je_io_update_wheel

## 函数签名

```cpp
JE_API void je_io_update_wheel(size_t group, float x, float y);
```

## 描述

更新鼠标滚轮的计数。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| group | size_t | 鼠标分组索引 |
| x | float | 水平滚轮增量 |
| y | float | 垂直滚轮增量 |

## 返回值

无

## 用法示例

```cpp
// 更新主鼠标滚轮，垂直方向滚动 1 单位
je_io_update_wheel(0, 0.0f, 1.0f);
```

## 注意事项

- 此函数是基本接口，用于由图形后端或平台层向引擎报告滚轮状态

## 相关接口

- [je_io_get_wheel](je_io_get_wheel.md) - 获取鼠标滚轮的计数
