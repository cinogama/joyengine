# je_graphic_thread_terminate

## 函数签名

```c
JE_API void je_graphic_thread_terminate(jegl_context* thread_handle);
```

## 描述

终止图形线程，将会阻塞直到图形线程完全退出。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `thread_handle` | `jegl_context*` | 图形上下文指针 |

## 返回值

无返回值。

## 用法

创建图形线程之后，无论图形绘制工作是否终止，都需要使用此接口释放图形线程。

### 示例

```cpp
jegl_context* ctx = je_graphic_thread_start(config, universe, api_func, render_func, arg);

// ... 使用图形线程 ...

// 终止图形线程
je_graphic_thread_terminate(ctx);
```

## 注意事项

- 此函数会阻塞直到图形线程完全退出
- 必须对每个 `je_graphic_thread_start` 创建的线程调用此函数

## 相关接口

- [je_graphic_thread_start](je_graphic_thread_start.md) - 创建图形线程
