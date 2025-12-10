# jegl_load_vertex

## 函数签名

```c
JE_API jegl_vertex* jegl_load_vertex(
    jegl_context* context,
    const char* path);
```

## 描述

从指定路径加载一个顶点（模型）资源。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `jegl_context*` | 图形上下文指针 |
| `path` | `const char*` | 模型文件路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `jegl_vertex*` | 顶点资源指针；若加载失败返回 `nullptr` |

## 用法

加载的路径规则与 `jeecs_file_open` 相同。

### 顶点数据格式

使用此方法加载的模型，始终按照三角面排布，其顶点数据格式如下：

| 属性 | 数据类型 | 大小 |
|------|----------|------|
| 顶点坐标 | `float` | 3 × f32 |
| UV 映射坐标 | `float` | 2 × f32 |
| 法线方向 | `float` | 3 × f32 |
| 骨骼索引 | `int` | 4 × i32 |
| 骨骼权重 | `float` | 4 × f32 |

### 示例

```cpp
jegl_vertex* mesh = jegl_load_vertex(ctx, "@/models/character.obj");

if (mesh != nullptr) {
    // 绘制模型
    jegl_draw_vertex(mesh);
    
    // 使用完毕后释放
    jegl_close_vertex(mesh);
}
```

## 注意事项

- 若指定的文件不存在或不是一个合法的模型，则返回 `nullptr`
- 对于绑定骨骼数量小于 4 的顶点，空余位置的骨骼索引被 0 填充，权重为 0.f
- 支持的模型格式取决于 assimp 库

## 相关接口

- [jegl_create_vertex](jegl_create_vertex.md) - 创建顶点
- [jegl_close_vertex](jegl_close_vertex.md) - 关闭顶点
- [jegl_draw_vertex](jegl_draw_vertex.md) - 绘制顶点
