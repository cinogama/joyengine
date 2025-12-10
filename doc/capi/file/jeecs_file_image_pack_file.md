# jeecs_file_image_pack_file

## 函数签名

```c
JE_API bool jeecs_file_image_pack_file(
    fimg_creating_context* context,
    const char* filepath,
    const char* packingpath);
```

## 描述

向指定镜像中写入由 `filepath` 指定的一个文件，此文件在镜像中的路径被指定为 `packingpath`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `fimg_creating_context*` | 镜像创建上下文 |
| `filepath` | `const char*` | 源文件的实际路径 |
| `packingpath` | `const char*` | 文件在镜像中的虚拟路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`，失败返回 `false` |

## 用法

此函数用于将磁盘上的文件添加到正在创建的镜像中。

### 示例

```cpp
fimg_creating_context* ctx = jeecs_file_image_begin("/output/assets", 256 * 1024 * 1024);

// 打包文件
bool result = jeecs_file_image_pack_file(
    ctx,
    "/project/textures/brick.png",  // 源文件路径
    "textures/brick.png"            // 镜像中的虚拟路径
);

if (!result) {
    printf("Failed to pack file\n");
}

jeecs_file_image_finish(ctx);
```

## 注意事项

- `packingpath` 是在镜像中的虚拟路径，用于后续通过 `jeecs_file_open` 访问
- 如果源文件不存在或无法读取，将返回 `false`

## 相关接口

- [jeecs_file_image_begin](jeecs_file_image_begin.md) - 开始创建镜像
- [jeecs_file_image_pack_buffer](jeecs_file_image_pack_buffer.md) - 添加缓冲区数据到镜像
- [jeecs_file_image_finish](jeecs_file_image_finish.md) - 完成镜像创建
