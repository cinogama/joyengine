#pragma once

// 通用“脚本生成二进制缓存”支持：TOML 源 + .je4cache 的资源（帧动画、
// 瓦片图集、瓦片地图）共用同一套流程——源缓存缺失或 CRC 失效时，加载
// 侧读取内置生成器脚本并调用其 generate(cache_writer, source_path)。

#include <cstddef>
#include <cstdint>
#include <string>

namespace jeecs
{
    namespace cache
    {
        constexpr size_t CACHE_FILE_HEADER_SIZE =
            sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t);

        // 把 @/!/ 前缀展开成脚本侧可直接访问的本地路径；普通路径原样返回。
        std::string resolve_source_path(const char* filepath);

        // 调用生成器脚本，把 source_filepath 指向的 TOML 源转换为
        // <source_filepath>.je4cache。返回 true 表示缓存已写出并通过
        // jeecs_load_cache_file 校验；失败时删除半成品缓存。
        bool generate_from_script(
            const char* source_filepath,
            uint32_t format_version,
            const char* generator_script_path,
            const char* generator_entry);
    }
}
