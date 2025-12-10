# jegl_rchain_close

## 函数签名

```c
JE_API void jegl_rchain_close(jegl_rendchain* chain);
```

## 描述

销毁绘制链实例。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `chain` | `jegl_rendchain*` | 绘制链实例指针 |

## 返回值

无返回值。

## 用法

此函数用于释放绘制链资源。

### 示例

```cpp
jegl_rendchain* chain = jegl_rchain_create();

// 使用绘制链...

// 销毁
jegl_rchain_close(chain);
```

## 注意事项

- 销毁后不要再使用该绘制链指针
- 如果需要复用绘制链，可以不销毁，从 `jegl_rchain_begin` 重新开始

## 相关接口

- [jegl_rchain_create](jegl_rchain_create.md) - 创建绘制链
