# jegl_create_vertex

## 函数签名

```c
JE_API jegl_vertex* jegl_create_vertex(
    jegl_vertex::type type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const jegl_vertex::data_layout* format,
    size_t format_count);
```

## 描述

用指定的顶点数据创建一个顶点（模型）资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `type` | `jegl_vertex::type` | 顶点图元类型 |
| `datas` | `const void*` | 顶点数据指针 |
| `data_length` | `size_t` | 顶点数据长度（字节） |
| `indices` | `const uint32_t*` | 索引数据指针 |
| `index_count` | `size_t` | 索引数量 |
| `format` | `const jegl_vertex::data_layout*` | 顶点布局格式数组 |
| `format_count` | `size_t` | 顶点布局格式数量 |

### 图元类型

| 类型 | 描述 |
|------|------|
| `POINTS` | 点 |
| `LINES` | 线段 |
| `LINE_LOOP` | 线环 |
| `LINE_STRIP` | 线条 |
| `TRIANGLES` | 三角形 |
| `TRIANGLE_STRIP` | 三角形条带 |
| `TRIANGLE_FAN` | 三角形扇 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_vertex*` | 顶点资源指针 |

## 用法

此函数用于从内存数据创建顶点缓冲。

### 示例

```cpp
// 定义顶点数据
float vertices[] = {
    // 位置 (x, y, z), UV (u, v)
    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
     0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
     0.0f,  0.5f, 0.0f, 0.5f, 1.0f,
};

uint32_t indices[] = { 0, 1, 2 };

// 定义顶点布局
jegl_vertex::data_layout layouts[] = {
    { jegl_vertex::FLOAT, 3 },  // 位置：3个float
    { jegl_vertex::FLOAT, 2 },  // UV：2个float
};

jegl_vertex* triangle = jegl_create_vertex(
    jegl_vertex::TRIANGLES,
    vertices, sizeof(vertices),
    indices, 3,
    layouts, 2
);

// 使用顶点...

jegl_close_vertex(triangle);
```

## 注意事项

- 使用此接口创建的顶点，其资源路径为 `nullptr`
- 数据会被复制到图形资源中

## 相关接口

- [jegl_load_vertex](jegl_load_vertex.md) - 从文件加载顶点
- [jegl_close_vertex](jegl_close_vertex.md) - 关闭顶点
