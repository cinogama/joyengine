# jeecs_file_image_begin

## 函数签名

```c
JE_API fimg_creating_context* jeecs_file_image_begin(
    const char* storing_path,
    size_t max_image_size);
```

## 描述

开始创建一个镜像文件（`.fimg`）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `storing_path` | `const char*` | 镜像文件存储路径（不含扩展名） |
| `max_image_size` | `size_t` | 每个镜像分片的最大大小（字节） |

## 返回值

| 类型 | 描述 |
|------|------|
| `fimg_creating_context*` | 镜像创建上下文指针 |

## 用法

此函数用于开始创建一个新的镜像文件，镜像文件可用于打包多个资源文件。

### 示例

```cpp
// 开始创建镜像，每个分片最大 256MB
fimg_creating_context* ctx = jeecs_file_image_begin(
    "/output/game_assets",
    256 * 1024 * 1024
);

// 添加文件到镜像
jeecs_file_image_pack_file(ctx, "/source/texture.png", "assets/texture.png");
jeecs_file_image_pack_file(ctx, "/source/model.obj", "assets/model.obj");

// 完成镜像创建
jeecs_file_image_finish(ctx);
```

## 注意事项

- 如果文件大于 `max_image_size`，镜像会自动分片
- 创建完成后必须调用 `jeecs_file_image_finish` 结束创建
- 生成的文件为 `.fimg` 格式（可能包含多个分片文件和索引文件）

## 相关接口

- [jeecs_file_image_pack_file](jeecs_file_image_pack_file.md) - 添加文件到镜像
- [jeecs_file_image_pack_buffer](jeecs_file_image_pack_buffer.md) - 添加缓冲区数据到镜像
- [jeecs_file_image_finish](jeecs_file_image_finish.md) - 完成镜像创建
