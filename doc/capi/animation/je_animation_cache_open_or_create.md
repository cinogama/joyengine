# je_animation_cache_open_or_create

## 函数签名

```c
JE_API /* OPTIONAL */ je_file_t* je_animation_cache_open_or_create(const char* path);
```

## 描述

打开指定的动画集缓存文件；缓存缺失或失效时调用内置缓存生成器重建。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 动画源文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_file_t*` | 缓存文件句柄；无法生成时返回 nullptr |

## 注意事项

- 返回的句柄由调用方负责关闭（见 je_file_close）
