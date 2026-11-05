# je_audio_filter_update

## 函数签名

```cpp
void je_audio_filter_update(je_audio_filter_t* filter);
```

## 描述

将对 `je_audio_filter_t` 的参数修改应用到实际的滤波器上。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| filter | je_audio_filter_t* | 滤波器指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_filter_t* filter = je_audio_filter_create();

// 修改滤波器参数
filter->m_type = JE_AUDIO_FILTER_LOWPASS;
filter->m_gain_hf = 0.2f;

// 应用更改
je_audio_filter_update(filter);
```

## 相关接口

- [je_audio_filter_create](je_audio_filter_create.md) - 创建滤波器
