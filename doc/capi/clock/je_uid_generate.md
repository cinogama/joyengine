# je_uid_generate

## 函数签名

```c
JE_API void je_uid_generate(jeecs::typing::uid_t* out_uid);
```

## 描述

生成一个全局唯一标识符（UUID）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `out_uid` | `jeecs::typing::uid_t*` | 用于接收生成的 UUID 的指针 |

## 返回值

无返回值。生成的 UUID 通过 `out_uid` 参数返回。

## 用法

此函数用于生成唯一标识符，常用于实体标识、资源管理等场景。

### 示例

```cpp
jeecs::typing::uid_t uid;
je_uid_generate(&uid);

// UUID 可以转换为字符串格式
printf("Generated UUID: %016llX-%016llX\n", 
    (unsigned long long)uid.a, 
    (unsigned long long)uid.b);
```

### UUID 结构

```cpp
struct uuid {
    union {
        struct { uint64_t a; uint64_t b; };
        struct {
            uint32_t x;  // 时间戳
            uint16_t y;  // 时间戳
            uint16_t z;  // 随机数
            uint16_t w;  // 递增值低16位
            uint16_t u;  // 递增值高16位
            uint32_t v;  // 随机数
        };
    };
};
```

## 注意事项

- 生成的 UUID 保证在当前进程内唯一
- 不同进程生成的 UUID 理论上也是唯一的（基于时间戳和随机数）
- `uid_t` 是 `uuid` 的类型别名

## 相关接口

- 无直接相关接口
