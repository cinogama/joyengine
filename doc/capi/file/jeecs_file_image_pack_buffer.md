# jeecs_file_image_pack_buffer

## 函数签名

```c
JE_API bool jeecs_file_image_pack_buffer(
    fimg_creating_context* context,
    const void* buffer,
    size_t len,
    const char* packingpath);
```

## 描述

向指定镜像中写入一个缓冲区指定的内容作为文件，此文件在镜像中的路径被指定为 `packingpath`。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `context` | `fimg_creating_context*` | 镜像创建上下文 |
| `buffer` | `const void*` | 数据缓冲区指针 |
| `len` | `size_t` | 缓冲区数据长度（字节） |
| `packingpath` | `const char*` | 文件在镜像中的虚拟路径 |

## 返回值

| 类型 | 描述 |
|------|------|
| `bool` | 成功返回 `true`，失败返回 `false` |

## 用法

此函数用于将内存中的数据作为文件添加到正在创建的镜像中。

### 示例

```cpp
fimg_creating_context* ctx = jeecs_file_image_begin("/output/assets", 256 * 1024 * 1024);

// 准备数据
const char* config_data = "{ \"version\": 1, \"name\": \"My Game\" }";

// 打包缓冲区数据
bool result = jeecs_file_image_pack_buffer(
    ctx,
    config_data,
    strlen(config_data),
    "config/game.json"      // 镜像中的虚拟路径
);

if (!result) {
    printf("Failed to pack buffer\n");
}

jeecs_file_image_finish(ctx);
```

## 注意事项

- 数据会被复制到镜像中
- `packingpath` 是在镜像中的虚拟路径

## 相关接口

- [jeecs_file_image_begin](jeecs_file_image_begin.md) - 开始创建镜像
- [jeecs_file_image_pack_file](jeecs_file_image_pack_file.md) - 添加文件到镜像
- [jeecs_file_image_finish](jeecs_file_image_finish.md) - 完成镜像创建
