# fimg 资源镜像格式（v3）

fimg 是 JoyEngine 使用的资源镜像格式，用于把项目资源、编译后的脚本和着色器缓存打包到一组文件中。运行时 `jeecs_file_open` 在真实文件不存在时，会从默认镜像读取 `@/...` 与 `!/...` 路径。

v3 在 v2 的基础上为每个文件增加了独立的压缩信息。每个条目要么原样存储，要么使用一条独立的 raw deflate 流压缩。

## 文件布局

一个镜像对应一个目录：

```
<root>/
    fimg.je4index          清单（manifest）
    fimg.0.je4image        分片 0
    fimg.1.je4image        分片 1
    ...
```

分片按需创建：只有实际写入数据时才会创建分片文件，因此不会出现空分片。分片大小由打包时传入的 `shard_size` 决定（默认 1 MiB）。

## 清单格式

清单使用小端字节序，所有整数显式序列化，不直接写入结构体内存布局。

### 头部（56 字节）

| 偏移 | 类型 | 字段 | 说明 |
|------|------|------|------|
| 0 | `char[8]` | magic | `"JE4FIMG\0"` |
| 8 | `uint32` | version | 固定为 3 |
| 12 | `uint32` | flags | 保留，当前必须为 0 |
| 16 | `uint64` | shard_size | 单个分片的目标大小 |
| 24 | `uint64` | file_count | 条目数量 |
| 32 | `uint64` | shard_count | 分片数量 |
| 40 | `uint64` | total_stored_size | 所有条目 `stored_size` 之和 |
| 48 | `uint64` | index_crc64 | 条目区的 CRC-64（用 `wo_crc64_u8` 从 0 累积） |

### 条目区

条目按路径排序，保证相同输入产生确定性输出：

| 类型 | 字段 | 说明 |
|------|------|------|
| `uint32` | path_len | 路径字节数 |
| `uint8[path_len]` | path | UTF-8 路径，不含 NUL；通常是 `@/...` 或 `!/...` |
| `uint64` | shard | 起始分片编号 |
| `uint64` | offset | 在起始分片内的偏移 |
| `uint64` | stored_size | 分片中实际占用的字节数 |
| `uint64` | size | 解压后的逻辑大小 |
| `uint32` | codec | 0 = store，1 = raw deflate |

路径不允许为空或包含 NUL，也不允许重复（读取端遇到重复时保留第一条并记录警告）。

### codec

- `0`（store）：`stored_size == size`，分片中的内容就是文件内容。
- `1`（deflate）：一条独立的 raw deflate 流（无 zlib 头），`size > 0` 且 `stored_size > 0`。
- 空文件始终使用 codec 0，`stored_size == size == 0`。

## 读取校验

加载清单时会拒绝以下情况：

- magic 或版本不匹配、存在未知 flags
- `shard_size == 0`，或清单超过 64 MiB
- 条目数量超过上限，或条目区被截断
- CRC-64 不匹配
- 路径为空/含 NUL/越界
- 未知 codec
- store 条目 `stored_size != size`
- deflate 条目 `size == 0` 或 `stored_size == 0`
- `stored_size > 0` 时 `offset >= shard_size`，或跨分片范围超出 `shard_count`
- 条目 `stored_size` 之和与 `total_stored_size` 不一致
- 末尾存在多余字节

清单不存在时视为“没有镜像”，这是开发期加载松散文件的正常情况，不会报错；清单存在但损坏会记录错误并拒绝加载。

## 读取行为

- 每个 `jeecs_file` 句柄同一时刻最多持有一个分片文件句柄；压缩输入跨分片读取时按需切换。
- store 条目直接按逻辑偏移读取分片。
- deflate 条目按需解压，`tell`/`size` 始终返回解压后的逻辑位置/大小。
- 向前 seek 只更新逻辑位置，下一次读取时再解压丢弃到目标位置。
- 向后 seek 会重置解压器，并从文件头重新解压到目标位置；复杂度为 O(target)。
- `jeecs_file_read` 与 `fread` 语义一致：返回完整元素个数；文件末尾的部分元素不会计入返回值。
- 压缩数据损坏/截断时记录错误，后续读取返回 0，不会返回未初始化的数据。

## 写入行为

通过 `jeecs_fimg_writer_*` 创建镜像：

- `shard_size == 0` 时使用 1 MiB。
- 分片惰性创建，直接流式写入，不会分配 `shard_size` 大小的整块内存。
- 每个文件独立选择 codec：
  - 压缩等级为 0（或未启用压缩）时使用 store。
  - 文件小于 `min_compress_size` 时使用 store。
  - 扩展名命中跳过列表时使用 store（默认列表包含 PNG/JPEG/ZIP/MP3/MP4 等已压缩格式）。
  - 其余使用 raw deflate 流式压缩。
- 压缩输出按 64 KiB 块直接写入分片，不缓冲整个文件。
- 不做“压缩后更大就回退”的判断；通过跳过列表和最小大小规避无意义压缩。
- `finish` 关闭最后一个分片并直接写出 `fimg.je4index`；任何写入/关闭失败都会返回 `false` 并记录错误，不会生成可用的新清单。
- `abort` 只释放 writer；已经写出的分片和清单保留，输出目录由调用方负责清理。
- 重复的 `archive_path` 会被拒绝（保留第一次写入）并记录警告。

## 路径约定

- `@/...`：运行时资源路径。打开时先查运行时目录下的真实文件，再查默认镜像。
- `!/...`：引擎内部资源路径。打开时先查宿主目录下的真实文件，再查默认镜像。
- 镜像内存放的是带前缀的原始键，查找时不会改写路径。

## 压缩选项

`jeecs_fimg_writer_options` 提供：

- `compression_level`：`-1` 使用默认等级 6；`0` 仅存储；`1-9` 使用 deflate。
- `min_compress_size`：小于该值的文件不压缩。
- `skip_extensions`：逗号/分号/空白分隔的扩展名列表，大小写不敏感，前导 `.` 可有可无；传 `nullptr` 使用 `jeecs_fimg_default_skip_extensions()` 的默认列表，空字符串表示不跳过任何扩展名。

默认跳过列表包含：PNG、JPEG、GIF、WebP、ICO、PSD、PDF、ZIP、JAR、APK、GZ、BZ2、XZ、7Z、RAR、ZST、BR、MP3、OGG、OPUS、AAC、FLAC、MP4、WEBM、MKV、AVI、MOV、WOFF/WOFF2、DDS、KTX 等。
