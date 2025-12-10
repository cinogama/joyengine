# jeal_close_source

## 函数签名

```cpp
JE_API void jeal_close_source(jeal_source* source);
```

## 描述

释放一个音频源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| source | jeal_source* | 要释放的音频源指针 |

## 返回值

无

## 用法示例

```cpp
jeal_source* source = jeal_create_source();

// 使用音频源...

jeal_close_source(source);
```

## 相关接口

- [jeal_create_source](jeal_create_source.md) - 创建音频源
