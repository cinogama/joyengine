# jeal_update_filter

## 函数签名

```cpp
void jeal_update_filter(jeal_filter* filter);
```

## 描述

将对 `jeal_filter` 的参数修改应用到实际的滤波器上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| filter | jeal_filter* | 滤波器指针 |

## 返回值

无

## 用法示例

```cpp
jeal_filter* filter = jeal_create_filter();

// 修改滤波器参数
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.2f;

// 应用更改
jeal_update_filter(filter);
```

## 相关接口

- [jeal_create_filter](jeal_create_filter.md) - 创建滤波器
