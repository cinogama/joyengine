# jeal_close_filter

## 函数签名

```cpp
void jeal_close_filter(jeal_filter* filter);
```

## 描述

关闭一个滤波器。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| filter | jeal_filter* | 要关闭的滤波器指针 |

## 返回值

无

## 用法示例

```cpp
jeal_filter* filter = jeal_create_filter();

// 使用滤波器...

jeal_close_filter(filter);
```

## 相关接口

- [jeal_create_filter](jeal_create_filter.md) - 创建滤波器
