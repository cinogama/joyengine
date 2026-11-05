#define JE_IMPL
#include "jeecs.hpp"

#include "wo.h"
#include "jeecs_cache_version.hpp"

#include <vector>

using namespace std;

namespace jeecs
{
    static std::optional<woort_CodeEnv*> m_animation_cache_builder = nullopt;

    static std::optional<woort_CodeEnv*> _je_try_load_internal_script_with_cache(const char* path)
    {
        auto* const cache_file = jeecs_load_cache_file(path, INTERNAL_SCRIPT_CACHE_VERSION, 0);
        if (cache_file != nullptr)
        {
            do
            {
                jeecs_cache_binary_document doc;
                if (!jeecs_cache_binary_document_open(cache_file, &doc))
                    break;

                jeecs_cache_binary_buffer content;
                if (!jeecs_cache_binary_document_index(
                    &doc,
                    "content",
                    jeecs_cache_binary_value_type::JE_CACHE_BINARY_VALUE_BUFFER,
                    &content))
                {
                    // Bad cache.
                    break;
                }

                // Ok, trying to load binary buffer.
                size_t length = 0;
                if (!jeecs_cache_binary_buffer_read(nullptr, &length, &content))
                    break;

                vector<char> binary_content(length);
                if (!jeecs_cache_binary_buffer_read(binary_content.data(), &length, &content))
                    break;

                wo_CompileErrors* compile_errors;
                woort_CodeEnv* cenv = wo_load_binary(path, binary_content.data(), length, &compile_errors);
                if (cenv == nullptr)
                {
                    // Loading binary failed.
                    debug::logwarn("Failed to load internal script cache: \n%s",
                        wo_get_compile_error(compile_errors, wo_inform_style_t::WO_PLAIM));

                    wo_compile_errors_free(compile_errors);
                    break;
                }
                
                return cenv;

            } while (0);
            debug::logwarn("Bad internal script cache while loading `%s`, retry with source.", path);
        }
    }
}
