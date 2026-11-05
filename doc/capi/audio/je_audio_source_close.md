# je_audio_source_close

## 函数签名

```cpp
JE_API void je_audio_source_close(je_audio_source_t* source);
```

## 描述

释放一个音频源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | je_audio_source_t* | 要释放的音频源指针 |

## 返回值

无

## 用法示例

```cpp
je_audio_source_t* source = je_audio_source_create();

// 使用音频源...

je_audio_source_close(source);
```

## 相关接口

- [je_audio_source_create](je_audio_source_create.md) - 创建音频源
