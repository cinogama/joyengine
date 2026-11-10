#pragma once

// ============================================================================
// jeecs/file.hpp —— 文件：je_file_* 与 fimg 镜像（je_fimg_*）C API
// =============================================================================

#include "jeecs/prefix.hpp"

JE_FORCE_CAPI

/////////////////////////// FILE /////////////////////////////////

/*
je_file_t [类型]
文件，用于保存引擎读取的文件。具体布局由引擎内部维护，不对外公开。
一般使用此类型的指针类型
请参见：
    je_file_open
    je_file_close
    je_file_read
    je_file_size
*/
typedef struct je_file_t je_file_t;

/*
je_fimg_archive_t [类型]
只读资源镜像，对应一个目录下的 fimg.je4index 清单与若干 fimg.<N>.je4image 分片
一般使用此类型的指针类型
请参见：
    je_fimg_archive_open
    je_fimg_archive_close
    je_fimg_default_archive_set
*/
typedef struct je_fimg_archive_t je_fimg_archive_t;

/*
je_fimg_writer_t [类型]
资源镜像打包器
一般使用此类型的指针类型
请参见：
    je_fimg_writer_create
    je_fimg_writer_pack_file
    je_fimg_writer_pack_buffer
    je_fimg_writer_finish
    je_fimg_writer_abort
*/
typedef struct je_fimg_writer_t je_fimg_writer_t;

enum je_file_seek_mode_t
{
    JE_READ_FILE_SEEK_SET = SEEK_SET,
    JE_READ_FILE_SEEK_CURRENT = SEEK_CUR,
    JE_READ_FILE_SEEK_END = SEEK_END,
};

typedef struct _je_raw_file_s_t* je_raw_file_t;

typedef je_raw_file_t(*je_read_file_open_func_t)(const char*, size_t*);
typedef size_t(*je_read_file_func_t)(void*, size_t, size_t, je_raw_file_t);
typedef size_t(*je_read_file_tell_func_t)(je_raw_file_t);
typedef int (*je_read_file_seek_func_t)(je_raw_file_t, int64_t, je_file_seek_mode_t);
typedef int (*je_read_file_close_func_t)(je_raw_file_t);

/*
je_file_host_path_set [基本接口]
设置当前引擎的宿主环境在路径，不影响“实际可执行文件所在路径”
    * 设置此路径将影响以 ! 开头的路径的实际位置
    * 正常情况下，这个接口不需要调用，在初始化时，
    引擎会自动设置为实际二进制文件所在路径，但是对于一些特殊平台，引擎
    不能使用这些路径（出于需要创建缓存文件或需要读取镜像等资源文件）
    使用此接口可以使得引擎内置机制使用指定的路径
*/
JE_API void je_file_host_path_set(const char* path);

/*
je_file_runtime_path_set [基本接口]
设置当前引擎的运行时路径，不影响“工作路径”
    * 设置此路径将影响以 @ 开头的路径的实际位置
    * 此接口不会自动加载镜像，需要另行调用 je_fimg_default_archive_set
请参考：
    je_fimg_default_archive_set
*/
JE_API void je_file_runtime_path_set(const char* path);

/*
je_fimg_default_archive_set [基本接口]
从指定目录加载 fimg 镜像作为默认镜像
    * 以 @/ 或 !/ 开头的路径在实际文件不存在时将从默认镜像读取
    * 无论加载是否成功，之前打开的默认镜像都将被关闭
    * 若 root == nullptr，则仅关闭旧的镜像并返回 true
    * 目录下不存在 fimg.je4index 时静默返回 false
    * 清单存在但损坏时记录错误并返回 false
*/
JE_API bool je_fimg_default_archive_set(/* OPTIONAL: If root == nullptr, only close the old archive */ const char* root);

/*
je_file_host_path_get [基本接口]
获取当前引擎的宿主环境路径
若没有使用 je_file_host_path_set 设置此路径，则此路径默认为引擎的可
执行文件所在路径
请参见：
    je_file_host_path_set
*/
JE_API const char* je_file_host_path_get();

/*
je_file_runtime_path_get [基本接口]
获取当前引擎的运行时路径，与“工作路径”无关
若没有使用 je_file_runtime_path_set 设置此路径，则此路径默认为引擎的可
执行文件所在路径
请参见：
    je_file_runtime_path_set
*/
JE_API const char* je_file_runtime_path_get();

/*
je_file_native_operator_register [基本接口]
设置引擎底层的文件读取接口
*/
JE_API void je_file_native_operator_register(
    je_read_file_open_func_t opener,
    je_read_file_func_t reader,
    je_read_file_tell_func_t teller,
    je_read_file_seek_func_t seeker,
    je_read_file_close_func_t closer);

/*
je_file_open [基本接口]
从指定路径打开一个文件，若打开失败返回nullptr
若文件成功打开，使用完毕后需要使用 je_file_close 关闭
    * 若路径由 '@' 开头，则如同加载指定运行目录下的文件：
      优先打开实际存在的文件，不存在时从默认镜像读取
    * 若路径由 '!' 开头，则如同加载可执行文件所在目录下的文件：
      优先打开实际存在的文件，不存在时从默认镜像读取
请参见：
    je_file_close
*/
JE_API /* OPTIONAL */ je_file_t* je_file_open(const char* path);

/*
je_file_close [基本接口]
关闭一个文件
*/
JE_API void je_file_close(je_file_t* file);

/*
je_file_read [基本接口]
从文件中读取若干个指定大小的元素，返回成功读取的元素数量
*/
JE_API size_t je_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    je_file_t* file);

/*
je_file_tell [基本接口]
获取当前文件下一个读取的位置偏移量
*/
JE_API size_t je_file_tell(je_file_t* file);

/*
je_file_seek [基本接口]
按照指定的模式，对文件即将读取的位置进行偏移和跳转
镜像文件的位置会被钳制在 [0, 文件大小] 范围内；成功返回 true
请参见：
    je_file_seek_mode_t
*/
JE_API bool je_file_seek(je_file_t* file, int64_t offset, je_file_seek_mode_t mode);

/*
je_file_size [基本接口]
获取文件总大小
*/
JE_API size_t je_file_size(je_file_t* file);

/*
je_file_crc64 [基本接口]
计算 filepath 指定文件内容的 crc64 校验值，路径按 je_file_open 的规则解析
    * crc64_continue 作为种子传入，返回在其基础上链式计算后的结果，
      传 0 即从头计算
    * 文件不存在或为空时原样返回 crc64_continue，因此链式计算时拼接
      缺失的可选文件不会影响之前的状态
请参见：
    je_file_open
*/
JE_API uint64_t je_file_crc64(const char* filepath, uint64_t crc64_continue);

/*
je_fimg_archive_open [基本接口]
从指定目录打开一个 fimg 镜像（fimg.je4index + fimg.<N>.je4image）
目录下不存在清单或清单损坏时返回 nullptr
使用完毕后需要使用 je_fimg_archive_close 关闭
*/
JE_API /* OPTIONAL */ je_fimg_archive_t* je_fimg_archive_open(const char* root);

/*
je_fimg_archive_close [基本接口]
关闭一个镜像；已打开的文件句柄仍然有效，直到它们自身被关闭
*/
JE_API void je_fimg_archive_close(je_fimg_archive_t* archive);

/*
je_fimg_archive_has_file [基本接口]
判断镜像中是否存在指定路径的文件
*/
JE_API bool je_fimg_archive_has_file(const je_fimg_archive_t* archive, const char* path);

/*
je_fimg_archive_file_size [基本接口]
获取镜像中指定文件的大小；文件不存在时返回 0
*/
JE_API size_t je_fimg_archive_file_size(const je_fimg_archive_t* archive, const char* path);

/*
je_fimg_writer_options_t [类型]
fimg 打包选项
*/
typedef struct je_fimg_writer_options_t
{
    /* -1 使用默认等级 6；0 仅存储；1-9 使用 deflate */
    int compression_level;
    /* 小于该大小的文件不压缩 */
    size_t min_compress_size;
    /* OPTIONAL: nullptr 使用内置默认列表；空字符串表示不跳过任何扩展名
       逗号/分号/空白分隔，大小写不敏感，前导 '.' 可有可无 */
    const char* skip_extensions;
} je_fimg_writer_options_t;

/*
je_fimg_writer_create [基本接口]
准备在 root 目录下创建 fimg 镜像
    * root 必须已经存在且可写
    * shard_size 是单个分片文件的目标大小，传 0 使用默认值（1 MiB）
    * options 为 nullptr 时使用默认压缩配置（等级 6、最小 64 字节、内置跳过列表）
*/
JE_API je_fimg_writer_t* je_fimg_writer_create(
    const char* root,
    size_t shard_size,
    /* OPTIONAL */ const je_fimg_writer_options_t* options);

/*
je_fimg_default_skip_extensions [基本接口]
获取内置的“已压缩扩展名”列表，按逗号分隔
用于编辑器初始化可编辑的跳过列表
*/
JE_API const char* je_fimg_default_skip_extensions();

/*
je_fimg_writer_pack_file [基本接口]
将 filepath 指定的文件写入镜像，镜像内路径指定为 archive_path
archive_path 重复、源文件读取失败或 writer 已结束/失败时返回 false
*/
JE_API bool je_fimg_writer_pack_file(
    je_fimg_writer_t* writer,
    const char* filepath,
    const char* archive_path);

/*
je_fimg_writer_pack_buffer [基本接口]
将缓冲区内容作为文件写入镜像，镜像内路径指定为 archive_path
archive_path 重复、参数非法或 writer 已结束/失败时返回 false
*/
JE_API bool je_fimg_writer_pack_buffer(
    je_fimg_writer_t* writer,
    /* OPTIONAL: If len == 0 */ const void* buffer,
    size_t len,
    const char* archive_path);

/*
je_fimg_writer_finish [基本接口]
结束镜像创建：关闭最后一个分片并写出 fimg.je4index 清单
成功返回 true；失败返回 false 且 writer 进入失败状态，需要调用
je_fimg_writer_abort 清理
*/
JE_API bool je_fimg_writer_finish(je_fimg_writer_t* writer);

/*
je_fimg_writer_abort [基本接口]
结束并释放 writer
    * 如果尚未成功 finish，仅关闭当前分片并释放 writer，已经写出的文件保留
    * 输出目录由调用方负责清理
*/
JE_API void je_fimg_writer_abort(je_fimg_writer_t* writer);
typedef je_file_t je_File;
typedef je_fimg_archive_t je_FimgArchive;
typedef je_fimg_writer_t je_FimgWriter;
typedef je_file_seek_mode_t je_FileSeekMode;
typedef je_raw_file_t je_RawFile;
typedef je_read_file_open_func_t je_ReadFileOpenFunc;
typedef je_read_file_func_t je_ReadFileFunc;
typedef je_read_file_tell_func_t je_ReadFileTellFunc;
typedef je_read_file_seek_func_t je_ReadFileSeekFunc;
typedef je_read_file_close_func_t je_ReadFileCloseFunc;
typedef je_fimg_writer_options_t je_FimgWriterOptions;

JE_FORCE_CAPI_END
