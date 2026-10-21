# jeecs_fimg_default_skip_extensions

## 函数签名

```c
JE_API const char* jeecs_fimg_default_skip_extensions();
```

## 描述

获取内置的“已压缩扩展名”列表，按逗号分隔。该列表用于初始化编辑器中的可编辑跳过列表，避免默认值在多处重复维护。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 指向静态字符串的指针，不需要释放 |

## 示例

```cpp
jeecs_fimg_writer_options options;
options.compression_level = 6;
options.min_compress_size = 1024;
options.skip_extensions = jeecs_fimg_default_skip_extensions();
```

默认列表包含 PNG、JPEG、GIF、WebP、ICO、PSD、PDF、ZIP、JAR、APK、GZ、BZ2、XZ、7Z、RAR、ZST、BR、MP3、OGG、OPUS、AAC、FLAC、MP4、WEBM、MKV、AVI、MOV、WOFF/WOFF2、DDS、KTX 等。

## 相关接口

- [jeecs_fimg_writer_create](jeecs_fimg_writer_create.md)
- [fimg 格式说明](../../fimg_format.md)
