# je_animation_open

## 函数签名

```c
JE_API /* OPTIONAL */ je_animation_t* je_animation_open(const char* path);
```

## 描述

加载 path 指向的帧动画：

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | — |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_animation_t*` | 见描述。 |

## 注意事项

- 读取 <path>.je4cache；缓存缺失/失效时先调用builtin/script/cache_generator/animation.wo 生成
- 空路径返回一个不含任何动作的空动画
- 其他失败情况返回 nullptr 并记录日志
- 返回的数据使用 je_animation_close 释放
