# je_audio_filter_close

## 函数签名

```cpp
void je_audio_filter_close(jeal_filter* filter);
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
jeal_filter* filter = je_audio_filter_create();

// 使用滤波器...

je_audio_filter_close(filter);
```

## 相关接口

- [je_audio_filter_create](je_audio_filter_create.md) - 创建滤波器
