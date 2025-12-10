# jeecs_file_image_finish

## 函数签名

```c
JE_API void jeecs_file_image_finish(fimg_creating_context* context);
```

## 描述

结束镜像创建，将最后剩余数据写入镜像并创建镜像索引文件。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `fimg_creating_context*` | 镜像创建上下文 |

## 返回值

无返回值。

## 用法

此函数用于完成镜像文件的创建，写入索引并关闭所有文件句柄。

### 示例

```cpp
fimg_creating_context* ctx = jeecs_file_image_begin("/output/game", 256 * 1024 * 1024);

// 添加文件
jeecs_file_image_pack_file(ctx, "/source/texture.png", "textures/texture.png");
jeecs_file_image_pack_file(ctx, "/source/model.obj", "models/model.obj");

// 完成镜像创建
jeecs_file_image_finish(ctx);
// 此时会生成：
// /output/game.fimg      (如果只有一个分片)
// 或
// /output/game_0.fimg    (第一个分片)
// /output/game_1.fimg    (第二个分片，如果有的话)
// /output/game.fimg      (索引文件)
```

## 注意事项

- 调用后 `context` 将被释放，不能再使用
- 必须对每个 `jeecs_file_image_begin` 的返回值调用此函数

## 相关接口

- [jeecs_file_image_begin](jeecs_file_image_begin.md) - 开始创建镜像
- [jeecs_file_image_pack_file](jeecs_file_image_pack_file.md) - 添加文件到镜像
