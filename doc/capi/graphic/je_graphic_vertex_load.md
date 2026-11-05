# je_graphic_vertex_load

## 函数签名

```c
JE_API jegl_vertex* je_graphic_vertex_load(
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

加载的路径规则与 `je_file_open` 相同。

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
jegl_vertex* mesh = je_graphic_vertex_load(ctx, "@/models/character.obj");

if (mesh != nullptr) {
    // 绑定顶点，并通过命令缓冲区提交绘制
    // cmd 为当前命令缓冲区，vertex_count 为本次绘制的顶点数
    je_graphic_cmd_bind_vertex(cmd, mesh);
    je_graphic_cmd_draw(cmd, vertex_count, 0, 1);
    
    // 使用完毕后释放
    je_graphic_vertex_close(mesh);
}
```

## 注意事项

- 若指定的文件不存在或不是一个合法的模型，则返回 `nullptr`
- 对于绑定骨骼数量小于 4 的顶点，空余位置的骨骼索引被 0 填充，权重为 0.f
- 支持的模型格式取决于 assimp 库

## 相关接口

- [je_graphic_vertex_create](je_graphic_vertex_create.md) - 创建顶点
- [je_graphic_vertex_close](je_graphic_vertex_close.md) - 关闭顶点
- [je_graphic_cmd_bind_vertex](je_graphic_cmd_bind_vertex.md) - 绑定顶点缓冲
- [je_graphic_cmd_draw](je_graphic_cmd_draw.md) - 发出非索引绘制
