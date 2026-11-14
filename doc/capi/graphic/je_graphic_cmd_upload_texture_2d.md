# je_graphic_cmd_upload_texture_2d / je_graphic_cmd_upload_texture_3d / je_graphic_cmd_upload_texture_array / je_graphic_cmd_upload_texture_cube

## 函数签名

```c
JE_API void je_graphic_cmd_upload_texture_2d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_2d_t* texture,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);

JE_API void je_graphic_cmd_upload_texture_3d(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_3d_t* texture,
    size_t x, size_t y, size_t z,
    size_t width, size_t height, size_t depth,
    const je_gl_texture_pixel_data_t* pixels);

JE_API void je_graphic_cmd_upload_texture_array(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_array_t* texture,
    size_t x, size_t y, size_t layer,
    size_t width, size_t height, size_t layers,
    const je_gl_texture_pixel_data_t* pixels);

JE_API void je_graphic_cmd_upload_texture_cube(
    je_gl_command_buffer_t* cmd,
    je_gl_texture_cube_t* texture,
    size_t face,
    size_t x, size_t y,
    size_t width, size_t height,
    const je_gl_texture_pixel_data_t* pixels);
```

## 描述

录制一个**持久**纹理区段上传。与顶点/UBO 的「本帧内容」不同，写入的是纹理自身的
存储：提交后对后续所有帧持续可见（资产语义，对应 WebGPU 的 `writeTexture`）。

图形后端负责与仍在执行的帧同步，可能在提交时等待一次在飞帧；这是稀有路径
（字形图集增页、视频帧等），不应逐帧使用。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `cmd` | `je_gl_command_buffer_t*` | 目标命令缓冲区 |
| `texture` | 对应类别的纹理指针 | 要上传的纹理 |
| `x` / `y` / `z` | `size_t` | 区段在纹理中的起点（`z` 仅 3D） |
| `width` / `height` / `depth` | `size_t` | 区段尺寸（`depth` 仅 3D） |
| `layer` / `layers` | `size_t` | 数组纹理的起始层与层数 |
| `face` | `size_t` | 立方体贴图的面索引（0..5） |
| `pixels` | `const je_gl_texture_pixel_data_t*` | 区段像素数据 |

像素数据行序与创建期快照一致（自底向上），按区段行连续排布：

- 2D：`height` 行；
- 3D：`depth` 个 2D 区段；
- array：`layer` 起连续 `layers` 个 2D 区段；
- cube：指定面的 `height` 行。

## 返回值

无。

## 用法

### 示例

```cpp
// 把字形位图写入图集页的 (px, py) 处
je_graphic_cmd_upload_texture_2d(
    update_cmd, page_texture,
    px, py, glyph_w, glyph_h, glyph_bitmap);
```

## 注意事项

- 区段越界，或对不含 CPU 像素的格式（`FLOAT16` / `DEPTH` / `FRAMEBUF`）上传，
  均属调用错误，由断言检查。
- 块压缩纹理（`BC1` / `BC3`）只支持创建时整块上传：区段上传无法把像素坐标对齐到
  4x4 块，同样由断言检查。
- 数据在录制时拷贝进命令缓冲，调用返回后可以立即复用源缓冲。
- MSAA 纹理仅存在于 GPU 侧，不支持上传。
- 上传命令属于资源更新，建议录制在
  [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md)
  返回的更新命令缓冲中。

## 相关接口

- [je_graphic_texture_2d_create](je_graphic_texture_2d_create.md) - 创建纹理（含初始像素）
- [je_graphic_branch_new_update_command](je_graphic_branch_new_update_command.md) - 申请更新命令缓冲
