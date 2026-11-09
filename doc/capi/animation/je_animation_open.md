# je_animation_open

## 函数签名

```c
JE_API /* OPTIONAL */ je_animation_t* je_animation_open(const char* path);
```

## 描述

加载 path 指向的动画集（v2 轨道/关键帧模型）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 动画源文件的引擎路径（@/ 或 !/） |

## 返回值

| 类型 | 描述 |
|------|------|
| `je_animation_t*` | 动画集只读数据；空路径返回不含动作的空动画，加载失败返回 nullptr |

## 注意事项

- 读取 <path>.je4cache；缓存缺失/失效时先调用 builtin/script/internal/cache/animation.wo 生成
- 其他失败情况返回 nullptr 并记录日志
- 返回的数据使用 je_animation_close 释放
- 数据加载后不再修改，可被多个实体共享
