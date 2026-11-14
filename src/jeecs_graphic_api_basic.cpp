#define JE_IMPL
#include "jeecs/basic.hpp"
#include "jeecs/cache.hpp"
#include "jeecs/clock.hpp"
#include "jeecs/core.hpp"
#include "jeecs/ecs.hpp"
#include "jeecs/file.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_basic_bootup.hpp"
#include "jeecs_graphic_render_internal.hpp"
#include "jeecs_basic_cache_version.hpp"
#include "jeecs_graphic_shader_wrapper.hpp"

#include "wo.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix3x3.h>

#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

#include <forward_list>
#include <algorithm>
#include <cstring>
#include <condition_variable>
#include <list>
#include <future>
#include <stack>
#include <queue>

namespace jeecs::graphic {}

using namespace std;
using namespace jeecs;
using namespace jeecs::graphic;

namespace Assimp
{
    class je_file_io_stream : public IOStream
    {
        JECS_DISABLE_MOVE_AND_COPY(je_file_io_stream);

        je_file_t* m_file;

    public:
        je_file_io_stream(je_file_t* fres)
            : m_file(fres)
        {
            assert(m_file != nullptr);
        }

        virtual ~je_file_io_stream()
        {
            je_file_close(m_file);
        }

        virtual size_t Read(
            void* pvBuffer,
            size_t pSize,
            size_t pCount) override
        {
            return je_file_read(pvBuffer, pSize, pCount, m_file);
        }

        virtual size_t Write(
            const void* pvBuffer,
            size_t pSize,
            size_t pCount) override
        {
            // NOT IMPL!
            abort();
        }

        virtual aiReturn Seek(
            size_t pOffset,
            aiOrigin pOrigin) override
        {
            static_assert((int)aiOrigin::aiOrigin_CUR == (int)je_file_seek_mode_t::JE_READ_FILE_SEEK_CURRENT);
            static_assert((int)aiOrigin::aiOrigin_END == (int)je_file_seek_mode_t::JE_READ_FILE_SEEK_END);
            static_assert((int)aiOrigin::aiOrigin_SET == (int)je_file_seek_mode_t::JE_READ_FILE_SEEK_SET);

            je_file_seek(m_file, (int64_t)pOffset, (je_file_seek_mode_t)pOrigin);
            return aiReturn::aiReturn_SUCCESS;
        }

        // -------------------------------------------------------------------
        /** @brief Get the current position of the read/write cursor
         *
         * See ftell() for more details */
        virtual size_t Tell() const override
        {
            return je_file_tell(m_file);
        }

        // -------------------------------------------------------------------
        /** @brief Returns filesize
         *  Returns the filesize. */
        virtual size_t FileSize() const override
        {
            return je_file_size(m_file);
        }

        // -------------------------------------------------------------------
        /** @brief Flush the contents of the file buffer (for writers)
         *  See fflush() for more details.
         */
        virtual void Flush() override
        {
            // ...
        }
    };
    class je_file_io_system : public IOSystem
    {
        JECS_DISABLE_MOVE_AND_COPY(je_file_io_system);

        string m_current_dir;

    public:
        je_file_io_system()
            : m_current_dir(je_file_runtime_path_get())
        {
        }

        bool Exists(const char* pFile) const override
        {
            auto* f = je_file_open(pFile);
            if (f == nullptr)
                return false;

            je_file_close(f);
            return true;
        }

        char getOsSeparator() const override
        {
            return '/'; // why not? it doesn't care
        }

        IOStream* Open(const char* pFile, const char* pMode = "rb") override
        {
            auto* f = je_file_open(pFile);
            if (f == nullptr)
                return nullptr;

            return new je_file_io_stream(f);
        }

        void Close(IOStream* pFile) override
        {
            delete pFile;
        }

        bool ComparePaths(const char* one, const char* second) const override
        {
            return strcmp(one, second) == 0;
        }

        bool PushDirectory(const string& path) override
        {
            return false;
        }

        const string& CurrentDirectory() const override
        {
            return m_current_dir;
        }

        size_t StackSize() const override
        {
            return 0;
        }

        bool PopDirectory() override
        {
            return false;
        }

        bool CreateDirectory(const string& path) override
        {
            return false;
        }

        bool ChangeDirectory(const string& path) override
        {
            return false;
        }

        bool DeleteFile(const string& file) override
        {
            return false;
        }
    };
}

struct je_gl_resource_bind_counter_t
{
    JECS_DISABLE_MOVE_AND_COPY(je_gl_resource_bind_counter_t);

    atomic_int32_t m_binding_count;
    je_gl_resource_bind_counter_t(int32_t init)
        : m_binding_count(init)
    {
    }
};

namespace jeecs::graphic
{
    namespace
    {
        struct created_resource
        {
            enum class kind
            {
                UNKNOWN,
                SHADER,
                TEXTURE_2D,
                TEXTURE_3D,
                TEXTURE_ARRAY,
                TEXTURE_CUBE,
                TEXTURE_MSAA,
                VERTEX,
                UNIFORM_BUFFER,
                FRAME_BUFFER,
                PIPELINE,
                SAMPLER
            };
            union deleting_graphic_resource
            {
                void* m_raw_ptr;

                je_gl_shader_t* m_shader;
                je_gl_texture_2d_t* m_texture_2d;
                je_gl_texture_3d_t* m_texture_3d;
                je_gl_texture_array_t* m_texture_array;
                je_gl_texture_cube_t* m_texture_cube;
                je_gl_texture_msaa_t* m_texture_msaa;
                je_gl_vertex_t* m_vertex;
                je_gl_uniform_buffer_t* m_uniform_buffer;
                je_gl_frame_buffer_t* m_frame_buffer;
                je_gl_pipeline_t* m_pipeline;
                je_gl_sampler_t* m_sampler;
            };

            kind m_type;
            deleting_graphic_resource m_resource;

            created_resource(const created_resource&) = default;
            created_resource(created_resource&&) = default;
            created_resource& operator = (const created_resource&) = default;
            created_resource& operator = (created_resource&&) = default;
            ~created_resource() = default;

            constexpr created_resource(je_gl_shader_t* shader)
                : m_type(kind::SHADER)
            {
                m_resource.m_shader = shader;
            }
            constexpr created_resource(je_gl_texture_2d_t* texture)
                : m_type(kind::TEXTURE_2D)
            {
                m_resource.m_texture_2d = texture;
            }
            constexpr created_resource(je_gl_texture_3d_t* texture)
                : m_type(kind::TEXTURE_3D)
            {
                m_resource.m_texture_3d = texture;
            }
            constexpr created_resource(je_gl_texture_array_t* texture)
                : m_type(kind::TEXTURE_ARRAY)
            {
                m_resource.m_texture_array = texture;
            }
            constexpr created_resource(je_gl_texture_cube_t* texture)
                : m_type(kind::TEXTURE_CUBE)
            {
                m_resource.m_texture_cube = texture;
            }
            constexpr created_resource(je_gl_texture_msaa_t* texture)
                : m_type(kind::TEXTURE_MSAA)
            {
                m_resource.m_texture_msaa = texture;
            }
            constexpr created_resource(je_gl_vertex_t* vertex)
                : m_type(kind::VERTEX)
            {
                m_resource.m_vertex = vertex;
            }
            constexpr created_resource(je_gl_uniform_buffer_t* uniform_buffer)
                : m_type(kind::UNIFORM_BUFFER)
            {
                m_resource.m_uniform_buffer = uniform_buffer;
            }
            constexpr created_resource(je_gl_frame_buffer_t* frame_buffer)
                : m_type(kind::FRAME_BUFFER)
            {
                m_resource.m_frame_buffer = frame_buffer;
            }
            constexpr created_resource(je_gl_pipeline_t* pipeline)
                : m_type(kind::PIPELINE)
            {
                m_resource.m_pipeline = pipeline;
            }
            constexpr created_resource(je_gl_sampler_t* sampler)
                : m_type(kind::SAMPLER)
            {
                m_resource.m_sampler = sampler;
            }
            bool operator == (const created_resource& another) const noexcept
            {
                return m_resource.m_raw_ptr == another.m_resource.m_raw_ptr;
            }

            static constexpr bool is_texture_kind(kind k) noexcept
            {
                return k == kind::TEXTURE_2D
                    || k == kind::TEXTURE_3D
                    || k == kind::TEXTURE_ARRAY
                    || k == kind::TEXTURE_CUBE
                    || k == kind::TEXTURE_MSAA;
            }

            template<requirements::basic_graphic_resource T>
            T* resource() const
            {
                if constexpr (is_same_v<T, je_gl_shader_t>)
                {
                    assert(m_type == kind::SHADER);
                    return m_resource.m_shader;
                }
                else if constexpr (is_same_v<T, je_gl_texture_2d_t>)
                {
                    assert(m_type == kind::TEXTURE_2D);
                    return m_resource.m_texture_2d;
                }
                else if constexpr (is_same_v<T, je_gl_texture_3d_t>)
                {
                    assert(m_type == kind::TEXTURE_3D);
                    return m_resource.m_texture_3d;
                }
                else if constexpr (is_same_v<T, je_gl_texture_array_t>)
                {
                    assert(m_type == kind::TEXTURE_ARRAY);
                    return m_resource.m_texture_array;
                }
                else if constexpr (is_same_v<T, je_gl_texture_cube_t>)
                {
                    assert(m_type == kind::TEXTURE_CUBE);
                    return m_resource.m_texture_cube;
                }
                else if constexpr (is_same_v<T, je_gl_texture_msaa_t>)
                {
                    assert(m_type == kind::TEXTURE_MSAA);
                    return m_resource.m_texture_msaa;
                }
                else if constexpr (is_same_v<T, je_gl_vertex_t>)
                {
                    assert(m_type == kind::VERTEX);
                    return m_resource.m_vertex;
                }
                else if constexpr (is_same_v<T, je_gl_uniform_buffer_t>)
                {
                    assert(m_type == kind::UNIFORM_BUFFER);
                    return m_resource.m_uniform_buffer;
                }
                else if constexpr (is_same_v<T, je_gl_frame_buffer_t>)
                {
                    assert(m_type == kind::FRAME_BUFFER);
                    return m_resource.m_frame_buffer;
                }
                else if constexpr (is_same_v<T, je_gl_pipeline_t>)
                {
                    assert(m_type == kind::PIPELINE);
                    return m_resource.m_pipeline;
                }
                else if constexpr (is_same_v<T, je_gl_sampler_t>)
                {
                    assert(m_type == kind::SAMPLER);
                    return m_resource.m_sampler;
                }
                else
                {
                    static_assert(!sizeof(T*), "Unsupported resource type.");
                }
            }

            template<requirements::basic_graphic_resource T>
            T* try_resource() const
            {
                if constexpr (is_same_v<T, je_gl_shader_t>)
                {
                    if (m_type != kind::SHADER)
                        return nullptr;
                    return m_resource.m_shader;
                }
                else if constexpr (is_same_v<T, je_gl_texture_2d_t>)
                {
                    if (m_type != kind::TEXTURE_2D)
                        return nullptr;
                    return m_resource.m_texture_2d;
                }
                else if constexpr (is_same_v<T, je_gl_texture_3d_t>)
                {
                    if (m_type != kind::TEXTURE_3D)
                        return nullptr;
                    return m_resource.m_texture_3d;
                }
                else if constexpr (is_same_v<T, je_gl_texture_array_t>)
                {
                    if (m_type != kind::TEXTURE_ARRAY)
                        return nullptr;
                    return m_resource.m_texture_array;
                }
                else if constexpr (is_same_v<T, je_gl_texture_cube_t>)
                {
                    if (m_type != kind::TEXTURE_CUBE)
                        return nullptr;
                    return m_resource.m_texture_cube;
                }
                else if constexpr (is_same_v<T, je_gl_texture_msaa_t>)
                {
                    if (m_type != kind::TEXTURE_MSAA)
                        return nullptr;
                    return m_resource.m_texture_msaa;
                }
                else if constexpr (is_same_v<T, je_gl_vertex_t>)
                {
                    if (m_type != kind::VERTEX)
                        return nullptr;
                    return m_resource.m_vertex;
                }
                else if constexpr (is_same_v<T, je_gl_uniform_buffer_t>)
                {
                    if (m_type != kind::UNIFORM_BUFFER)
                        return nullptr;
                    return m_resource.m_uniform_buffer;
                }
                else if constexpr (is_same_v<T, je_gl_frame_buffer_t>)
                {
                    if (m_type != kind::FRAME_BUFFER)
                        return nullptr;
                    return m_resource.m_frame_buffer;
                }
                else if constexpr (is_same_v<T, je_gl_pipeline_t>)
                {
                    if (m_type != kind::PIPELINE)
                        return nullptr;
                    return m_resource.m_pipeline;
                }
                else if constexpr (is_same_v<T, je_gl_sampler_t>)
                {
                    if (m_type != kind::SAMPLER)
                        return nullptr;
                    return m_resource.m_sampler;
                }
                else
                {
                    static_assert(!sizeof(T*), "Unsupported resource type.");
                }
            }

            void drop_resource() const
            {
                switch (m_type)
                {
                case kind::SHADER:
                    je_graphic_shader_close(m_resource.m_shader);
                    break;
                case kind::TEXTURE_2D:
                    je_graphic_texture_2d_close(m_resource.m_texture_2d);
                    break;
                case kind::TEXTURE_3D:
                    je_graphic_texture_3d_close(m_resource.m_texture_3d);
                    break;
                case kind::TEXTURE_ARRAY:
                    je_graphic_texture_array_close(m_resource.m_texture_array);
                    break;
                case kind::TEXTURE_CUBE:
                    je_graphic_texture_cube_close(m_resource.m_texture_cube);
                    break;
                case kind::TEXTURE_MSAA:
                    je_graphic_texture_msaa_close(m_resource.m_texture_msaa);
                    break;
                case kind::VERTEX:
                    je_graphic_vertex_close(m_resource.m_vertex);
                    break;
                case kind::UNIFORM_BUFFER:
                    je_graphic_uniformbuf_close(m_resource.m_uniform_buffer);
                    break;
                case kind::FRAME_BUFFER:
                    je_graphic_framebuf_close(m_resource.m_frame_buffer);
                    break;
                case kind::PIPELINE:
                    je_graphic_pipeline_close(m_resource.m_pipeline);
                    break;
                case kind::SAMPLER:
                    je_graphic_sampler_close(m_resource.m_sampler);
                    break;

                default:
                    abort();
                }
            }
            void free_resource_body() const
            {
                free(m_resource.m_raw_ptr);
            }
            je_gl_resource_handle_t* get_handle() const
            {
                switch (m_type)
                {
                case kind::SHADER:
                    return &m_resource.m_shader->m_handle;
                case kind::TEXTURE_2D:
                case kind::TEXTURE_3D:
                case kind::TEXTURE_ARRAY:
                case kind::TEXTURE_CUBE:
                case kind::TEXTURE_MSAA:
                    // 所有纹理类别的 m_handle 都位于结构体偏移 0 处
                    return static_cast<je_gl_resource_handle_t*>(m_resource.m_raw_ptr);
                case kind::VERTEX:
                    return &m_resource.m_vertex->m_handle;
                case kind::UNIFORM_BUFFER:
                    return &m_resource.m_uniform_buffer->m_handle;
                case kind::FRAME_BUFFER:
                    return &m_resource.m_frame_buffer->m_handle;
                case kind::PIPELINE:
                    return &m_resource.m_pipeline->m_handle;
                case kind::SAMPLER:
                    return &m_resource.m_sampler->m_handle;
                default:
                    abort();
                }
            }
        };
        // Created blobs.
        struct cached_resource_blob
        {
            using kind = created_resource::kind;

            kind m_type;
            je_gl_resource_blob_t m_blob_may_null;
        };
        struct resource_to_destroy
        {
            resource_to_destroy* last;
            created_resource m_resource;

            template<requirements::basic_graphic_resource T>
            resource_to_destroy(T* res)
                : m_resource(res)
            {
            }
        };
    }
}

namespace std
{
    template<>
    struct hash<created_resource>
    {
        size_t operator()(const created_resource& res) const noexcept
        {
            return hash<void*>()(res.m_resource.m_raw_ptr);
        }
    };
}

struct je_gl_context_notifier_t
{
    struct cached_resource_statement
    {
        created_resource m_resource;
        uint8_t m_keeped_count;

        template<requirements::basic_graphic_resource T>
        cached_resource_statement(T* res)
            : m_resource(res)
            , m_keeped_count(0)
        {
        }

        cached_resource_statement(const cached_resource_statement&) = default;
        cached_resource_statement(cached_resource_statement&&) = default;
        cached_resource_statement& operator = (const cached_resource_statement&) = default;
        cached_resource_statement& operator = (cached_resource_statement&&) = default;

        bool try_drop()
        {
            constexpr uint8_t MAX_KEEP_COUNT = 16;

            if (m_resource.get_handle()->m_raw_ref_count->m_binding_count.load() > 1)
            {
                // 此资源依然活跃，为了避免频繁释放，通过 keep 机制延迟释放。
                if (++m_keeped_count > MAX_KEEP_COUNT)
                    m_keeped_count = MAX_KEEP_COUNT;
            }
            else if ((m_keeped_count >>= 1) == 0)
            {
                // 此资源没有其他持有者，并且 keep 计数归零，可以回收。
                m_resource.drop_resource();
                return true;
            }
            return false;
        }
    };

    using cached_resource_map_t =
        unordered_map<string, cached_resource_statement>;
    using cached_resource_blob_t =
        unordered_map<string, cached_resource_blob>;
    using outdated_resource_blob_t =
        unordered_set<string>;
    using inited_resource_set_t =
        unordered_set<created_resource>;
    using closing_resource_lock_free_list_t =
        basic::atomic_list<resource_to_destroy>;

    bool m_graphic_terminated;
    bool m_graphic_reboot;

    mutex m_update_mx;
    condition_variable m_update_waiter;
    bool m_update_request_flag;

    promise<void> m_promise;

    // 已经加载的资源缓存，会有一个清理机制，在必要时释放没有人使用的资源。
    shared_mutex _m_cached_resources_mx;
    cached_resource_map_t _m_cached_resources;

    // 资源的 blob 集合
    mutex _m_outdated_resource_blobs_mx;
    outdated_resource_blob_t _m_outdated_resource_blobs;

    // NOTE: 对以下资源的访问不需要上锁，因为对此的操作都被局限在图形线程内部，不会发生并发访问。
    size_t _m_increased_gpu_memory_size;                // 新增的 GPU 显存占用估算值，用于指示清除缓存时机。
    cached_resource_blob_t _m_created_resource_blobs;   // 已经创建的资源 blob。
    inited_resource_set_t _m_created_resources;         // 已经初始化的资源集合。
    closing_resource_lock_free_list_t _m_closing_resources; // 等待销毁的资源列表。
};

namespace jeecs::graphic
{
    thread_local je_gl_context_t* _current_graphic_thread = nullptr;
    namespace
    {
        vector<je_gl_context_t*> _jegl_alive_glthread_list;
        shared_mutex _jegl_alive_glthread_list_mx;
        je_graphic_sync_callback_func_t _jegl_sync_callback_func = nullptr;
        void* _jegl_sync_callback_arg = nullptr;
        je_gl_graphic_api_entry_t _jegl_host_graphic_api = nullptr;

        enum class jegl_resouce_state
        {
            READY,
            NEED_INIT,
            NEED_UPDATE,
            INVALID_CONTEXT,
        };

        void je_default_graphic_interface_sync_func(je_gl_context_t* gthread, void*)
        {
            thread([=]()
                {
                    je_gl_sync_state_t state = je_gl_sync_state_t::JE_GL_SYNC_SHUTDOWN;
                    for (;;)
                    {
                        je_graphic_sync_init(gthread, state == je_gl_sync_state_t::JE_GL_SYNC_REBOOT);
                        do
                        {
                            state = je_graphic_sync_update(gthread);
                        } while (state == je_gl_sync_state_t::JE_GL_SYNC_COMPLETE);

                        if (je_graphic_sync_shutdown(gthread, state == je_gl_sync_state_t::JE_GL_SYNC_REBOOT))
                            break;
                    }
                })
                .detach();
        }

        void _jegl_close_resouce_instance_by_api(
            je_gl_context_t* thread, const created_resource* res)
        {
            switch (res->m_type)
            {
            case created_resource::kind::SHADER:
                thread->m_apis->shader_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_shader_t>());
                break;
            case created_resource::kind::TEXTURE_2D:
                thread->m_apis->texture_2d_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_texture_2d_t>());
                break;
            case created_resource::kind::TEXTURE_3D:
                thread->m_apis->texture_3d_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_texture_3d_t>());
                break;
            case created_resource::kind::TEXTURE_ARRAY:
                thread->m_apis->texture_array_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_texture_array_t>());
                break;
            case created_resource::kind::TEXTURE_CUBE:
                thread->m_apis->texture_cube_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_texture_cube_t>());
                break;
            case created_resource::kind::TEXTURE_MSAA:
                thread->m_apis->texture_msaa_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_texture_msaa_t>());
                break;
            case created_resource::kind::VERTEX:
                thread->m_apis->vertex_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_vertex_t>());
                break;
            case created_resource::kind::UNIFORM_BUFFER:
                thread->m_apis->ubuffer_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_uniform_buffer_t>());
                break;
            case created_resource::kind::FRAME_BUFFER:
                thread->m_apis->framebuffer_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_frame_buffer_t>());
                break;
            case created_resource::kind::PIPELINE:
                thread->m_apis->pipeline_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_pipeline_t>());
                break;
            case created_resource::kind::SAMPLER:
                thread->m_apis->sampler_close(
                    thread->m_graphic_impl_context,
                    res->resource<je_gl_sampler_t>());
                break;
            default:
                debug::logfatal(
                    "Unknown graphic resource `%d` type when destroying resource in graphic thread.",
                    (int)res->m_type);
            }
        }

        void _jegl_close_resource_blob_by_api(
            je_gl_context_t* thread, const cached_resource_blob* blob)
        {
            if (blob->m_blob_may_null == nullptr)
                // No need to free.
                return;

            switch (blob->m_type)
            {
            case created_resource::kind::SHADER:
                thread->m_apis->shader_close_blob(
                    thread->m_graphic_impl_context,
                    blob->m_blob_may_null);
                break;
            case created_resource::kind::TEXTURE_2D:
            case created_resource::kind::TEXTURE_3D:
            case created_resource::kind::TEXTURE_ARRAY:
            case created_resource::kind::TEXTURE_CUBE:
            case created_resource::kind::TEXTURE_MSAA:
                thread->m_apis->texture_close_blob(
                    thread->m_graphic_impl_context,
                    blob->m_blob_may_null);
                break;
            case created_resource::kind::VERTEX:
                thread->m_apis->vertex_close_blob(
                    thread->m_graphic_impl_context,
                    blob->m_blob_may_null);
                break;
            default:
                debug::logfatal(
                    "Unknown graphic resource blob `%d` type when destroying resource blob in graphic thread.",
                    (int)blob->m_type);
            }
        }

        cached_resource_blob::kind _jegl_try_get_resource_blob(
            je_gl_resource_handle_t* resource_handle,
            je_gl_resource_blob_t* out_blob)
        {
            cached_resource_blob::kind found_blob_type =
                cached_resource_blob::kind::UNKNOWN;

            je_gl_resource_blob_t blob = nullptr;

            if (resource_handle->m_path_may_null_if_builtin != nullptr)
            {
                auto* graphic_thread_context =
                    _current_graphic_thread->_m_thread_notifier;

                auto& cached_resource_blob_list =
                    graphic_thread_context->_m_created_resource_blobs;

                auto blob_fnd = cached_resource_blob_list.find(resource_handle->m_path_may_null_if_builtin);
                if (blob_fnd != cached_resource_blob_list.end())
                {
                    // Check if outdated?
                    lock_guard g1(graphic_thread_context->_m_outdated_resource_blobs_mx);
                    auto fnd = graphic_thread_context->_m_outdated_resource_blobs.find(
                        resource_handle->m_path_may_null_if_builtin);

                    if (fnd != graphic_thread_context->_m_outdated_resource_blobs.end())
                    {
                        // Already outdated.
                        _jegl_close_resource_blob_by_api(
                            _current_graphic_thread, &blob_fnd->second);

                        graphic_thread_context->_m_outdated_resource_blobs.erase(fnd);
                        cached_resource_blob_list.erase(blob_fnd);
                    }
                    else
                    {
                        found_blob_type = blob_fnd->second.m_type;
                        blob = blob_fnd->second.m_blob_may_null;
                    }
                }
            }

            *out_blob = blob;
            return found_blob_type;
        }

        bool _jegl_try_update_resource_blob(
            je_gl_resource_handle_t* resource_handle,
            cached_resource_blob::kind blob_type,
            je_gl_resource_blob_t blob_may_null)
        {
            if (resource_handle->m_path_may_null_if_builtin == nullptr)
            {
                if (blob_may_null != nullptr)
                {
                    debug::logwarn(
                        "Cannot cache resource blob for anonymous resource.");

                    return false;
                }

                // If blob is null, return true to avoid useless blob close operation.
                return true;
            }

            assert(resource_handle->m_path_may_null_if_builtin != nullptr);

            auto result = _current_graphic_thread->_m_thread_notifier
                ->_m_created_resource_blobs.emplace(
                    resource_handle->m_path_may_null_if_builtin,
                    cached_resource_blob
                    {
                        blob_type,
                        blob_may_null,
                    });

            if (blob_may_null != nullptr)
                return result.second;

            // If blob is null, return true to avoid useless blob close operation.
            return true;
        }

        template<requirements::basic_graphic_resource T>
        T* /* MAY NULL */ _jegl_try_load_shared_resource(je_gl_context_t* context, const char* path)
        {
            if (context != nullptr)
            {
                shared_lock sg1(context->_m_thread_notifier->_m_cached_resources_mx);

                auto fnd = context->_m_thread_notifier->_m_cached_resources.find(path);
                if (fnd != context->_m_thread_notifier->_m_cached_resources.end())
                {
                    auto* res = fnd->second.m_resource.try_resource<T>();
                    if (res != nullptr)
                    {
                        jegl_share_resource(res);
                        return res;
                    }
                }
            }
            return nullptr;
        }
        template<requirements::basic_graphic_resource T>
        T* _jegl_try_update_shared_resource(je_gl_context_t* context, T* resource)
        {
            if (context != nullptr)
            {
                const char* resource_path = resource->m_handle.m_path_may_null_if_builtin;
                assert(resource != nullptr && resource_path != nullptr);

                lock_guard g1(context->_m_thread_notifier->_m_cached_resources_mx);

                je_gl_context_notifier_t::cached_resource_statement caching_statement(resource);
                auto&& [cached_resource_pair, insert_succ] =
                    context->_m_thread_notifier->_m_cached_resources.emplace(
                        resource_path, caching_statement);

                je_gl_context_notifier_t::cached_resource_statement& cached_resource =
                    cached_resource_pair->second;

                if (insert_succ)
                {
                    je_graphic_resource_handle_share(caching_statement.m_resource.get_handle());
                    return resource;
                }
                else if (cached_resource.m_resource.m_type == caching_statement.m_resource.m_type)
                {
                    // 就在刚刚创建的瞬间，已经有一个相同路径的资源存在了，放弃新创建的
                    caching_statement.m_resource.drop_resource();

                    je_graphic_resource_handle_share(cached_resource.m_resource.get_handle());
                    return cached_resource.m_resource.resource<T>();
                }
                else
                {
                    // WTF?
                    debug::logwarn(
                        "Different type of resource with same path detected in shared resource cache: `%s`.",
                        resource_path);
                    return resource;
                }
            }
            return resource;
        }
        template<requirements::basic_graphic_resource T>
        jegl_resouce_state _jegl_check_resource_state(
            T* resource)
        {
            auto* resource_handle = &resource->m_handle;

            if (!_current_graphic_thread)
            {
                debug::logerr("Graphic resource only usable in graphic thread.");
                return jegl_resouce_state::INVALID_CONTEXT;
            }

            if (resource_handle->m_graphic_thread != nullptr
                && _current_graphic_thread != resource_handle->m_graphic_thread)
            {
                debug::logerr("This resource has been used in graphic thread: %p.",
                    resource_handle->m_graphic_thread);
                return jegl_resouce_state::INVALID_CONTEXT;
            }
            else if (nullptr == resource_handle->m_graphic_thread
                || resource_handle->m_graphic_thread_version != _current_graphic_thread->m_version)
            {
                resource_handle->m_modified = false;
                resource_handle->m_graphic_thread = _current_graphic_thread;
                resource_handle->m_graphic_thread_version = _current_graphic_thread->m_version;

                auto result =
                    resource_handle->m_graphic_thread->_m_thread_notifier->_m_created_resources.emplace(
                        resource);

                (void)result;
                assert(result.second); // New resource must be inserted.

                return jegl_resouce_state::NEED_INIT;
            }
            else if (resource_handle->m_modified)
            {
                resource_handle->m_modified = false;
                return jegl_resouce_state::NEED_UPDATE;
            }

            return jegl_resouce_state::READY;
        }
        template<typename TexT>
        static bool prepare_texture_resource(TexT* texture)
        {
            assert(texture != nullptr);

            auto* thread_handle = _current_graphic_thread;
            if (thread_handle == nullptr)
                return false;

            const auto* gapi = thread_handle->m_apis;

            je_gl_resource_blob_t(*create_blob_fn)(je_graphic_impl_context_t, TexT*);
            void (*init_fn)(je_graphic_impl_context_t, je_gl_resource_blob_t, TexT*);

            if constexpr (is_same_v<TexT, je_gl_texture_2d_t>)
            {
                create_blob_fn = gapi->texture_2d_create_blob;
                init_fn = gapi->texture_2d_init;
            }
            else if constexpr (is_same_v<TexT, je_gl_texture_3d_t>)
            {
                create_blob_fn = gapi->texture_3d_create_blob;
                init_fn = gapi->texture_3d_init;
            }
            else if constexpr (is_same_v<TexT, je_gl_texture_array_t>)
            {
                create_blob_fn = gapi->texture_array_create_blob;
                init_fn = gapi->texture_array_init;
            }
            else if constexpr (is_same_v<TexT, je_gl_texture_cube_t>)
            {
                create_blob_fn = gapi->texture_cube_create_blob;
                init_fn = gapi->texture_cube_init;
            }
            else if constexpr (is_same_v<TexT, je_gl_texture_msaa_t>)
            {
                create_blob_fn = gapi->texture_msaa_create_blob;
                init_fn = gapi->texture_msaa_init;
            }
            else
            {
                static_assert(sizeof(TexT*) == 0, "Unsupported texture type.");
            }

            switch (_jegl_check_resource_state(texture))
            {
            case jegl_resouce_state::READY:
                return true;
            case jegl_resouce_state::NEED_INIT:
            {
                je_gl_resource_blob_t texture_blob;
                auto found_blob_kind = _jegl_try_get_resource_blob(&texture->m_handle, &texture_blob);

                if (created_resource::is_texture_kind(found_blob_kind))
                {
                    init_fn(
                        thread_handle->m_graphic_impl_context,
                        texture_blob,
                        texture);
                }
                else
                {
                    texture_blob = create_blob_fn(
                        thread_handle->m_graphic_impl_context,
                        texture);

                    init_fn(
                        thread_handle->m_graphic_impl_context,
                        texture_blob,
                        texture);

                    size_t slice_count = 1;
                    if constexpr (is_same_v<TexT, je_gl_texture_3d_t>)
                        slice_count = texture->m_depth;
                    else if constexpr (is_same_v<TexT, je_gl_texture_array_t>)
                        slice_count = texture->m_layers;
                    else if constexpr (is_same_v<TexT, je_gl_texture_cube_t>)
                        slice_count = 6;
                    else if constexpr (is_same_v<TexT, je_gl_texture_msaa_t>)
                        slice_count = texture->m_samples;

                    thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size
                        += texture_format_pixel_data_size(
                            texture->m_format,
                            texture->m_width,
                            texture->m_height)
                        * slice_count;

                    created_resource::kind blob_kind;
                    if constexpr (is_same_v<TexT, je_gl_texture_2d_t>)
                        blob_kind = created_resource::kind::TEXTURE_2D;
                    else if constexpr (is_same_v<TexT, je_gl_texture_3d_t>)
                        blob_kind = created_resource::kind::TEXTURE_3D;
                    else if constexpr (is_same_v<TexT, je_gl_texture_array_t>)
                        blob_kind = created_resource::kind::TEXTURE_ARRAY;
                    else if constexpr (is_same_v<TexT, je_gl_texture_cube_t>)
                        blob_kind = created_resource::kind::TEXTURE_CUBE;
                    else
                        blob_kind = created_resource::kind::TEXTURE_MSAA;

                    if (found_blob_kind != cached_resource_blob::kind::UNKNOWN
                        || !_jegl_try_update_resource_blob(
                            &texture->m_handle,
                            blob_kind,
                            texture_blob))
                    {
                        gapi->texture_close_blob(
                            thread_handle->m_graphic_impl_context,
                            texture_blob);
                    }
                }
                return true;
            }
            case jegl_resouce_state::INVALID_CONTEXT:
            default:
                return false;
            }
        }

        void _jegl_free_resource_instance(
            resource_to_destroy* /* created by `new` */ del_res)
        {
            assert(del_res != nullptr);

            auto* resource_handle = del_res->m_resource.get_handle();
            assert(resource_handle->m_raw_ref_count == nullptr);

            // Send this resource to destroing list;

            shared_lock sg1(_jegl_alive_glthread_list_mx);
            auto fnd = find(
                _jegl_alive_glthread_list.begin(),
                _jegl_alive_glthread_list.end(),
                resource_handle->m_graphic_thread);

            if (fnd != _jegl_alive_glthread_list.end())
                (*fnd)->_m_thread_notifier->_m_closing_resources.add_one(del_res);
            else
            {
                // 既然这个资源已经没有管理线程了，直接就地杀了埋了
                assert(resource_handle->m_graphic_thread == nullptr);

#ifndef NDEBUG
                if (resource_handle->m_ptr != nullptr)
                    debug::logwarn("Resource %p cannot free by correct graphic context, maybe it is out-dated? Free it!",
                        resource_handle);
#endif

                del_res->m_resource.free_resource_body();
                delete del_res;
            }
        }
        void _jegl_free_vertex_bone_data(const je_gl_vertex_bone_data_t* b)
        {
            je_mem_free(const_cast<char*>(b->m_name));
            free(const_cast<je_gl_vertex_bone_data_t*>(b));
        }
        bool _jegl_close_resource_handle(je_gl_resource_handle_t* resource_handle)
        {
            if (0 == --resource_handle->m_raw_ref_count->m_binding_count)
            {
                delete resource_handle->m_raw_ref_count;
                resource_handle->m_raw_ref_count = nullptr;

                if (resource_handle->m_path_may_null_if_builtin != nullptr)
                    je_mem_free(const_cast<char*>(resource_handle->m_path_may_null_if_builtin));

                return true;
            }
            return false;
        }
        template<typename TexT>
        static void _jegl_close_texture_instance(TexT* texture)
        {
            if (_jegl_close_resource_handle(&texture->m_handle))
            {
                // close resource's raw data, then send this resource to closing-queue
                free(texture->m_initial_pixels);
                _jegl_free_resource_instance(new resource_to_destroy(texture));
            }
        }

        je_gl_shader_t* _jegl_load_shader_source_impl(
            const char* path, const char* src, bool is_virtual_file)
        {
            if (is_virtual_file)
            {
                if (je_file_t* shader_cache = je_cache_file_load(path, SHADER_CACHE_VERSION, wo_crc64_str(src)))
                    return shader_wrapper::_jegl_load_shader_cache(shader_cache, path);
            }

#if JE4_ENABLE_SHADER_WRAP_GENERATOR
            je_gl_shader_t* instance = nullptr;

            wo_CompileErrors* cerror;
            woort_CodeEnv* const cenv = wo_load_source(path, src, &cerror);
            if (cenv == nullptr)
            {
                // Compile error
                debug::logerr("Fail to load shader: %s.\n%s", path, wo_get_compile_error(cerror, WO_PLAIM));
                wo_compile_errors_free(cerror);
            }
            else
            {
                woort_vm* const vmm = woort_vm_create();
                if (vmm == nullptr)
                    debug::logerr("Fail to load shader: %s.\nFailed to create vm.");
                else
                {
                    woort_vm* const last = woort_vm_swap(vmm);

                    woort_value result;
                    if (!woort_push_reserve(1, &result))
                    {
                        debug::logerr("Fail to load shader: %s.\nStack overflow.");
                    }
                    else if (WOORT_VM_CALL_STATUS_NORMAL != woort_bootup(result, cenv, false))
                    {
                        debug::logerr("Fail to load shader: %s.\n%s.",
                            woort_vm_get_runtime_error(vmm));
                    }
                    else if (!woort_load_extern_const(result, cenv, "je::shader::generate_shader"))
                    {
                        debug::logerr("Fail to load shader: %s. need to import je::shader.",
                            path);
                    }
                    else if (WOORT_VM_CALL_STATUS_NORMAL != woort_invoke(result, result))
                    {
                        debug::logerr("Fail to load shader: %s: %s.", path,
                            woort_vm_get_runtime_error(vmm));
                    }
                    else
                    {
                        shader_wrapper::ShaderWrapper* const shader_graph =
                            static_cast<shader_wrapper::ShaderWrapper*>(woort_gcpointer(result));

                        instance = shader_wrapper::_jegl_create_shader_instance_and_init();

                        shader_wrapper::jegl_shader_generate_shader_source(shader_graph, instance);

                        init_resource_handle(&instance->m_handle, path);
                        shader_wrapper::_jegl_create_shader_cache(instance, is_virtual_file ? wo_crc64_str(src) : 0);
                    }

                    (void)woort_vm_swap(last);
                    woort_vm_close(vmm);
                }
                woort_codeenv_drop(cenv);
            }

            return instance;
#else
            debug::logerr("Shader generator has been disabled.");
            return nullptr;
#endif
        }
    }

    void bootup(int argc, char** argv)
    {
        _jegl_host_graphic_api = nullptr;

        // Update default graphic sync funciton
        je_graphic_sync_thread_callback_register(
            je_default_graphic_interface_sync_func, nullptr);

        for (int i = 1; i < argc - 1; ++i)
        {
            string config = argv[i];
            string value = argv[i + 1];
            if (config == "-gapi")
            {
                if (false)
                    ;
#if defined(JE_ENABLE_DX11_GAPI)
                else if (value == "dx11")
                    je_graphic_host_api_set(je_graphic_using_dx11_apis);
#endif
#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
                else if (value == "gl3")
                    je_graphic_host_api_set(je_graphic_using_opengl3_apis);
#endif
#if defined(JE_ENABLE_VK120_GAPI)
                else if (value == "vk120")
                    je_graphic_host_api_set(je_graphic_using_vk120_apis);
#endif
#if defined(JE_ENABLE_METAL_GAPI)
                else if (value == "metal")
                    je_graphic_host_api_set(je_graphic_using_metal_apis);
#endif
                else if (value == "none")
                    je_graphic_host_api_set(je_graphic_using_none_apis);
                else
                    debug::logwarn("Unknown graphic api named: '%s'.", value.c_str());
            }
        }
    }

    void shutdown()
    {
        vector<je_gl_context_t*> shutdown_glthreads;
        do
        {
            lock_guard g1(_jegl_alive_glthread_list_mx);
            shutdown_glthreads = _jegl_alive_glthread_list;
        } while (0);

        for (auto alive_glthread : shutdown_glthreads)
        {
            je_graphic_thread_terminate(alive_glthread);
        }
    }

    /////////////////////////////////////////////////////////////////////////////
   // Resource preparation helpers for the render API (je_gl_command_buffer_t).
   // These perform lazy init/update on the graphic thread, but never bind or
   // draw. They intentionally mirror the legacy immediate-API code paths so
   // both paths share the same resource state machine.
   /////////////////////////////////////////////////////////////////////////////
    ResourcePrepareResult prepare_shader_resource(je_gl_shader_t* shader)
    {
        assert(shader != nullptr);

        auto* thread_handle = _current_graphic_thread;
        if (thread_handle == nullptr)
            return ResourcePrepareResult::INVALID;

        const auto* gapi = thread_handle->m_apis;
        switch (_jegl_check_resource_state(shader))
        {
        case jegl_resouce_state::READY:
            return ResourcePrepareResult::READY;
        case jegl_resouce_state::NEED_UPDATE:
            return ResourcePrepareResult::UPDATED;
        case jegl_resouce_state::NEED_INIT:
        {
            je_gl_resource_blob_t shader_blob;
            auto found_blob_kind = _jegl_try_get_resource_blob(&shader->m_handle, &shader_blob);

            if (found_blob_kind == cached_resource_blob::kind::SHADER)
            {
                gapi->shader_init(
                    thread_handle->m_graphic_impl_context,
                    shader_blob,
                    shader);
            }
            else
            {
                shader_blob = gapi->shader_create_blob(
                    thread_handle->m_graphic_impl_context,
                    shader);

                gapi->shader_init(
                    thread_handle->m_graphic_impl_context,
                    shader_blob,
                    shader);

                if (found_blob_kind != cached_resource_blob::kind::UNKNOWN
                    || !_jegl_try_update_resource_blob(
                        &shader->m_handle,
                        cached_resource_blob::kind::SHADER,
                        shader_blob))
                {
                    gapi->shader_close_blob(
                        thread_handle->m_graphic_impl_context,
                        shader_blob);
                }
            }
            return ResourcePrepareResult::INITIALIZED;
        }
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return ResourcePrepareResult::INVALID;
        }
    }
    ResourcePrepareResult prepare_pipeline_resource(je_gl_pipeline_t* pipeline)
    {
        assert(pipeline != nullptr);
        if (_current_graphic_thread == nullptr)
            return ResourcePrepareResult::INVALID;

        switch (_jegl_check_resource_state(pipeline))
        {
        case jegl_resouce_state::READY:
            return ResourcePrepareResult::READY;
        case jegl_resouce_state::NEED_UPDATE:
            return ResourcePrepareResult::UPDATED;
        case jegl_resouce_state::NEED_INIT:
            return ResourcePrepareResult::INITIALIZED;
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return ResourcePrepareResult::INVALID;
        }
    }
    ResourcePrepareResult prepare_sampler_resource(je_gl_sampler_t* sampler)
    {
        assert(sampler != nullptr);
        if (_current_graphic_thread == nullptr)
            return ResourcePrepareResult::INVALID;

        switch (_jegl_check_resource_state(sampler))
        {
        case jegl_resouce_state::READY:
            return ResourcePrepareResult::READY;
        case jegl_resouce_state::NEED_UPDATE:
            return ResourcePrepareResult::UPDATED;
        case jegl_resouce_state::NEED_INIT:
            return ResourcePrepareResult::INITIALIZED;
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return ResourcePrepareResult::INVALID;
        }
    }

    bool prepare_vertex_resource(je_gl_vertex_t* vert)
    {
        assert(vert != nullptr);

        auto* thread_handle = _current_graphic_thread;
        if (thread_handle == nullptr)
            return false;

        const auto* gapi = thread_handle->m_apis;
        switch (_jegl_check_resource_state(vert))
        {
        case jegl_resouce_state::READY:
            return true;
        case jegl_resouce_state::NEED_INIT:
        {
            je_gl_resource_blob_t vertex_blob;
            auto found_blob_kind = _jegl_try_get_resource_blob(&vert->m_handle, &vertex_blob);

            if (found_blob_kind == cached_resource_blob::kind::VERTEX)
            {
                gapi->vertex_init(
                    thread_handle->m_graphic_impl_context,
                    vertex_blob,
                    vert);
            }
            else
            {
                vertex_blob = gapi->vertex_create_blob(
                    thread_handle->m_graphic_impl_context,
                    vert);

                gapi->vertex_init(
                    thread_handle->m_graphic_impl_context,
                    vertex_blob,
                    vert);

                thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size
                    += vert->m_vertex_length * 4 /* FLOAT32 OR INT32 */
                    + vert->m_index_count * sizeof(uint32_t);

                if (found_blob_kind != cached_resource_blob::kind::UNKNOWN
                    || !_jegl_try_update_resource_blob(
                        &vert->m_handle,
                        cached_resource_blob::kind::VERTEX,
                        vertex_blob))
                {
                    gapi->vertex_close_blob(
                        thread_handle->m_graphic_impl_context,
                        vertex_blob);
                }
            }
            return true;
        }
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return false;
        }
    }
    bool prepare_uniform_buffer_resource(je_gl_uniform_buffer_t* uniformbuf)
    {
        assert(uniformbuf != nullptr);

        auto* thread_handle = _current_graphic_thread;
        if (thread_handle == nullptr)
            return false;

        const auto* gapi = thread_handle->m_apis;
        switch (_jegl_check_resource_state(uniformbuf))
        {
        case jegl_resouce_state::READY:
            return true;
        case jegl_resouce_state::NEED_INIT:
            gapi->ubuffer_init(
                thread_handle->m_graphic_impl_context,
                uniformbuf);

            thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size
                += uniformbuf->m_buffer_size;
            return true;
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return false;
        }
    }
    bool prepare_framebuffer_resource(je_gl_frame_buffer_t* framebuffer)
    {
        assert(framebuffer != nullptr);

        auto* thread_handle = _current_graphic_thread;
        if (thread_handle == nullptr)
            return false;

        const auto* gapi = thread_handle->m_apis;
        switch (_jegl_check_resource_state(framebuffer))
        {
        case jegl_resouce_state::READY:
            return true;
        case jegl_resouce_state::NEED_UPDATE:
            gapi->framebuffer_update(
                thread_handle->m_graphic_impl_context,
                framebuffer);
            return true;
        case jegl_resouce_state::NEED_INIT:
            gapi->framebuffer_init(
                thread_handle->m_graphic_impl_context,
                framebuffer);
            return true;
        case jegl_resouce_state::INVALID_CONTEXT:
        default:
            return false;
        }
    }
    bool prepare_texture_2d_resource(je_gl_texture_2d_t* texture)
    {
        return prepare_texture_resource(texture);
    }
    bool prepare_texture_3d_resource(je_gl_texture_3d_t* texture)
    {
        return prepare_texture_resource(texture);
    }
    bool prepare_texture_array_resource(je_gl_texture_array_t* texture)
    {
        return prepare_texture_resource(texture);
    }
    bool prepare_texture_cube_resource(je_gl_texture_cube_t* texture)
    {
        return prepare_texture_resource(texture);
    }
    bool prepare_texture_msaa_resource(je_gl_texture_msaa_t* texture)
    {
        return prepare_texture_resource(texture);
    }

    void init_resource_handle(
        je_gl_resource_handle_t* resource_handle,
        const char* path_may_null_if_builtin)
    {
        if (path_may_null_if_builtin != nullptr)
        {
            resource_handle->m_path_may_null_if_builtin =
                basic::make_new_string(path_may_null_if_builtin);
        }
        else
            resource_handle->m_path_may_null_if_builtin = nullptr;

        resource_handle->m_raw_ref_count = new je_gl_resource_bind_counter_t(1);

        resource_handle->m_graphic_thread = nullptr;
        resource_handle->m_graphic_thread_version = 0;
        resource_handle->m_modified = false;
        resource_handle->m_ptr = nullptr;
    }
}

//////////////////////////////////// API /////////////////////////////////////////
void je_graphic_host_api_set(/* OPTIONAL: nullptr to use platform default */ je_gl_graphic_api_entry_t api)
{
    _jegl_host_graphic_api = api;
}

je_gl_graphic_api_entry_t je_graphic_host_api_get(void)
{
    if (_jegl_host_graphic_api == nullptr)
    {
#if defined(JE_ENABLE_DX11_GAPI)
        return je_graphic_using_dx11_apis;
#elif defined(JE_ENABLE_METAL_GAPI)
        return je_graphic_using_metal_apis;
#elif defined(JE_ENABLE_VK120_GAPI)
        return je_graphic_using_vk120_apis;
#elif defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
        return je_graphic_using_opengl3_apis;
#else
        return je_graphic_using_none_apis;
#endif
    }
    return _jegl_host_graphic_api;
}


void je_graphic_sync_init(je_gl_context_t* thread, bool isreboot)
{
    if (_current_graphic_thread == nullptr)
        _current_graphic_thread = thread;

    assert(_current_graphic_thread == thread);

    if (thread->m_universe_instance != nullptr)
    {
        if (thread->m_config.m_fps == 0)
            je_ecs_universe_frame_deltatime_set(
                thread->m_universe_instance,
                0.0);
        else
            je_ecs_universe_frame_deltatime_set(
                thread->m_universe_instance,
                1.0 / (double)thread->m_config.m_fps);
    }

    thread->m_graphic_impl_context = thread->m_apis->interface_startup(
        thread, &thread->m_config, isreboot);

    ++thread->m_version;
}

je_gl_sync_state_t je_graphic_sync_update(je_gl_context_t* thread)
{
    unordered_map<
        resource_to_destroy*,
        bool /* Need close graphic resource */>
        _waiting_to_free_resource;

    auto* notifier = thread->_m_thread_notifier;

    if (!notifier->m_graphic_terminated)
    {
        do
        {
            unique_lock uq1(notifier->m_update_mx);
            notifier->m_update_waiter.wait(
                uq1,
                [notifier]() -> bool
                {
                    return notifier->m_update_request_flag;
                });
        } while (0);

        // Ready for rend..
        if (notifier->m_graphic_terminated || notifier->m_graphic_reboot)
            goto is_reboot_or_shutdown;

        const auto frame_update_state =
            thread->m_apis->update_frame_ready(thread->m_graphic_impl_context);

        switch (frame_update_state)
        {
        case je_gl_update_action_t::JE_GL_UPDATE_STOP:
            // graphic thread want to exit. mark stop update.
            notifier->m_graphic_terminated = true;
            break;
        case je_gl_update_action_t::JE_GL_UPDATE_SKIP:
            if (thread->m_config.m_fps == 0)
                je_clock_sleep_for(1.0 / (double)60.f);
            else
                je_clock_sleep_for(1.0 / (double)thread->m_config.m_fps);
            break;
        case je_gl_update_action_t::JE_GL_UPDATE_CONTINUE:
            // do nothing.
            break;
        }

        thread->_m_frame_rend_work(
            thread,
            thread->_m_frame_rend_work_arg,
            frame_update_state);

        if (je_gl_update_action_t::JE_GL_UPDATE_STOP ==
            thread->m_apis->update_draw_commit(
                thread->m_graphic_impl_context, frame_update_state))
        {
            notifier->m_graphic_terminated = true;
        }

        // HINT: 这个数值约等于 2048 * 2048 RGBA8 贴图的大小。
        constexpr size_t GPU_MEMORY_CLEANUP_THRESHOLD = 16 * 1024 * 1024;

        if (// 当资源新增的估计占用值达到 GPU_MEMORY_CLEANUP_THRESHOLD 时：
            notifier->_m_increased_gpu_memory_size >= GPU_MEMORY_CLEANUP_THRESHOLD)
        {
            // 检查一遍是否有可以释放的缓存资源。
            notifier->_m_increased_gpu_memory_size -= GPU_MEMORY_CLEANUP_THRESHOLD;

            lock_guard g1(notifier->_m_cached_resources_mx);
            forward_list<const char*> resources_to_erase;

            for (auto& [res_key, cached_res] : notifier->_m_cached_resources)
            {
                if (cached_res.try_drop())
                    // 缓存的资源被释放，标记为待移除
                    resources_to_erase.push_front(res_key.c_str());
            }
            for (const char* res_key : resources_to_erase)
            {
                auto erase_result = notifier->_m_cached_resources.erase(res_key);

                (void)erase_result;
                assert(erase_result == 1);
            }
        }

        auto* del_res = notifier->_m_closing_resources.pick_all();
        while (del_res)
        {
            auto* cur_del_res = del_res;
            del_res = del_res->last;

            auto closing_resource_handle = cur_del_res->m_resource.get_handle();
            assert(closing_resource_handle->m_graphic_thread == thread);

            if (closing_resource_handle->m_graphic_thread_version == thread->m_version)
                _waiting_to_free_resource[cur_del_res] = true;
            else
                // Free this only.
                _waiting_to_free_resource[cur_del_res] = false;
        }

        do
        {
            lock_guard g1(notifier->m_update_mx);
            notifier->m_update_request_flag = false;
            notifier->m_update_waiter.notify_all();
        } while (0);

        for (auto [deleting_resource, need_close] : _waiting_to_free_resource)
        {
            if (need_close)
            {
                _jegl_close_resouce_instance_by_api(thread, &deleting_resource->m_resource);
                auto result = notifier->_m_created_resources.erase(
                    deleting_resource->m_resource);

                (void)result;
                assert(result == 1);
            }

            deleting_resource->m_resource.free_resource_body();
            delete deleting_resource;
        }
    }
    else
    {
    is_reboot_or_shutdown:
        if (notifier->m_graphic_reboot)
        {
            notifier->m_graphic_reboot = false;
            return je_gl_sync_state_t::JE_GL_SYNC_REBOOT;
        }
        return je_gl_sync_state_t::JE_GL_SYNC_SHUTDOWN;
    }

    return je_gl_sync_state_t::JE_GL_SYNC_COMPLETE;
}

bool je_graphic_sync_shutdown(je_gl_context_t* thread, bool isreboot)
{
    thread->m_apis->interface_shutdown_before_resource_release(
        thread,
        thread->m_graphic_impl_context,
        isreboot);

    for (auto& [_, resource_blob] : thread->_m_thread_notifier->_m_created_resource_blobs)
        _jegl_close_resource_blob_by_api(thread, &resource_blob);

    thread->_m_thread_notifier->_m_created_resource_blobs.clear();

    for (auto& resource : thread->_m_thread_notifier->_m_created_resources)
    {
        _jegl_close_resouce_instance_by_api(thread, &resource);

        auto* resouce_handle = resource.get_handle();

        resouce_handle->m_graphic_thread = nullptr;
        resouce_handle->m_graphic_thread_version = 0;
    }
    thread->_m_thread_notifier->_m_created_resources.clear();

    thread->m_apis->interface_shutdown(
        thread,
        thread->m_graphic_impl_context,
        isreboot);

    thread->m_graphic_impl_context = nullptr;

    if (!isreboot)
    {
        // Really shutdown, give promise!
        thread->_m_thread_notifier->m_promise.set_value();
        _current_graphic_thread = nullptr;
        return true;
    }
    return false;
}

void je_graphic_sync_thread_callback_register(
    je_graphic_sync_callback_func_t callback, /* OPTIONAL */ void* arg)
{
    assert(callback != nullptr);

    _jegl_sync_callback_func = callback;
    _jegl_sync_callback_arg = arg;
}

/* OPTIONAL: If failed to start up */ je_gl_context_t* je_graphic_thread_start(
    je_gl_interface_config_t config,
    /* OPTIONAL: If no universe attached */ je_game_universe_t* universe_instance,
    je_graphic_api_register_func_t register_func,
    je_gl_frame_job_func_t frame_rend_work,
    /* OPTIONAL */ void* arg)
{
    je_gl_context_t* thread_handle = nullptr;

    if (config.m_title == nullptr)
    {
        debug::logerr("Cannot startup graphic thread without valid title.");
    }
    else if (register_func != nullptr)
    {
        thread_handle = new je_gl_context_t();

        thread_handle->m_version = 0;
        thread_handle->_m_thread_notifier = new je_gl_context_notifier_t();
        thread_handle->m_apis = new je_gl_graphic_api_t();

        memset(thread_handle->m_apis, 0, sizeof(je_gl_graphic_api_t));
        register_func(thread_handle->m_apis);

        size_t err_api_no = 0;
        // Every entry in the table must be provided, including the render
        // entry points (pipeline/sampler/pass/bind/draw/commit).
        const size_t api_count =
            sizeof(je_gl_graphic_api_t) / sizeof(void*);
        for (size_t i = 0; i < api_count; ++i)
        {
            if (!((void**)thread_handle->m_apis)[i])
            {
                err_api_no++;
                debug::logerr("GraphicAPI function: %zu is invalid.", i);
            }
        }

        if (err_api_no)
        {
            delete thread_handle->_m_thread_notifier;
            delete thread_handle->m_apis;
            delete thread_handle;

            thread_handle = nullptr;
        }
    }
    if (thread_handle != nullptr)
    {
        // Register finish functions
        do
        {
            lock_guard g1(_jegl_alive_glthread_list_mx);

            if (_jegl_alive_glthread_list.end() ==
                find(
                    _jegl_alive_glthread_list.begin(),
                    _jegl_alive_glthread_list.end(),
                    thread_handle))
            {
                _jegl_alive_glthread_list.push_back(thread_handle);
            }
        } while (0);

        // Take place.
        thread_handle->m_config = config;
        thread_handle->_m_thread_notifier->m_graphic_terminated = false;
        thread_handle->_m_thread_notifier->m_update_request_flag = false;
        thread_handle->_m_thread_notifier->m_graphic_reboot = false;
        thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size = 0;
        thread_handle->m_universe_instance = universe_instance;
        thread_handle->_m_frame_rend_work = frame_rend_work;
        thread_handle->_m_frame_rend_work_arg = arg;

        assert(_jegl_sync_callback_func != nullptr);
        thread_handle->_m_sync_callback_arg = _jegl_sync_callback_arg;
        _jegl_sync_callback_func(thread_handle, _jegl_sync_callback_arg);
    }
    else
        debug::logfatal("Fail to start up graphic thread, abort and return nullptr.");

    return thread_handle;
}

void je_graphic_thread_terminate(je_gl_context_t* thread)
{
    thread->_m_thread_notifier->m_graphic_terminated = true;

    do
    {
        lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_request_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    // Sync thread and wait for shutting-down
    thread->_m_thread_notifier->m_promise.get_future().get();

    // Close shared resources in contexrt.
    do
    {
        shared_lock sg1(thread->_m_thread_notifier->_m_cached_resources_mx);
        for (auto& [_path, cached_resource] : thread->_m_thread_notifier->_m_cached_resources)
        {
            (void)_path;
            cached_resource.m_resource.drop_resource();
        }
    } while (0);

    do
    {
        lock_guard g1(_jegl_alive_glthread_list_mx);
        auto fnd = find(
            _jegl_alive_glthread_list.begin(),
            _jegl_alive_glthread_list.end(),
            thread);
        if (fnd == _jegl_alive_glthread_list.end())
            return;
        else
        {
            _jegl_alive_glthread_list.erase(fnd);
        }

    } while (0);

    auto* closing_resource = thread->_m_thread_notifier->_m_closing_resources.pick_all();
    while (closing_resource)
    {
        auto* cur_closing_resource = closing_resource;
        closing_resource = closing_resource->last;

        cur_closing_resource->m_resource.free_resource_body();
        delete cur_closing_resource;
    }

    // delete launder(reinterpret_cast<atomic_bool*>(thread->m_stop_update));
    delete thread->_m_thread_notifier;
    delete thread->m_apis;
    delete thread;
}

bool je_graphic_update(
    je_gl_context_t* thread,
    je_gl_update_sync_mode_t mode,
    /* OPTIONAL */ je_gl_update_sync_callback_t callback_after_wait_may_null,
    /* OPTIONAL */ void* callback_param)
{
    if (thread->_m_thread_notifier->m_graphic_terminated)
        return false;

    unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
    if (mode == je_gl_update_sync_mode_t::JE_GL_WAIT_LAST_FRAME_END)
    {
        // Wait until `last` frame draw end.
        thread->_m_thread_notifier->m_update_waiter.wait(
            uq1,
            [thread]() -> bool
            {
                return !thread->_m_thread_notifier->m_update_request_flag;
            });

        if (nullptr != callback_after_wait_may_null)
            callback_after_wait_may_null(callback_param);
    }

    thread->_m_thread_notifier->m_update_request_flag = true;
    thread->_m_thread_notifier->m_update_waiter.notify_all();

    if (mode == je_gl_update_sync_mode_t::JE_GL_WAIT_THIS_FRAME_END)
    {
        // Wait until `this` frame draw end.
        thread->_m_thread_notifier->m_update_waiter.wait(
            uq1,
            [thread]() -> bool
            {
                return !thread->_m_thread_notifier->m_update_request_flag;
            });

        if (nullptr != callback_after_wait_may_null)
            callback_after_wait_may_null(callback_param);
    }

    return true;
}

void je_graphic_thread_reboot(je_gl_context_t* thread_handle, /* OPTIONAL: nullptr to keep current config */ const je_gl_interface_config_t* config_may_null)
{
    if (config_may_null != nullptr)
        thread_handle->m_config = *config_may_null;

    thread_handle->_m_thread_notifier->m_graphic_reboot = true;
}

bool je_graphic_shared_resources_mark_outdated(
    je_gl_context_t* context, const char* path)
{
    lock_guard g1(context->_m_thread_notifier->_m_outdated_resource_blobs_mx);
    if (context->_m_thread_notifier->_m_outdated_resource_blobs.emplace(path).second)
    {
        lock_guard g2(context->_m_thread_notifier->_m_cached_resources_mx);

        auto fnd = context->_m_thread_notifier->_m_cached_resources.find(path);
        if (fnd != context->_m_thread_notifier->_m_cached_resources.end())
        {
            fnd->second.m_resource.drop_resource();
            context->_m_thread_notifier->_m_cached_resources.erase(fnd);
        }

        return true;
    }
    return false;
}

void je_graphic_shader_close(je_gl_shader_t* shader)
{
    if (_jegl_close_resource_handle(&shader->m_handle))
    {
        shader_wrapper::jegl_shader_free_generated_shader_source(shader);
        _jegl_free_resource_instance(new resource_to_destroy(shader));
    }
}

void je_graphic_texture_2d_close(je_gl_texture_2d_t* texture)
{
    _jegl_close_texture_instance(texture);
}
void je_graphic_texture_3d_close(je_gl_texture_3d_t* texture)
{
    _jegl_close_texture_instance(texture);
}
void je_graphic_texture_array_close(je_gl_texture_array_t* texture)
{
    _jegl_close_texture_instance(texture);
}
void je_graphic_texture_cube_close(je_gl_texture_cube_t* texture)
{
    _jegl_close_texture_instance(texture);
}
void je_graphic_texture_msaa_close(je_gl_texture_msaa_t* texture)
{
    _jegl_close_texture_instance(texture);
}

void je_graphic_vertex_close(je_gl_vertex_t* vertex)
{
    if (_jegl_close_resource_handle(&vertex->m_handle))
    {
        // close resource's raw data, then send this resource to closing-queue
        free(const_cast<void*>(vertex->m_initial_vertexs));
        free(const_cast<uint32_t*>(vertex->m_initial_indices));
        free(const_cast<je_gl_vertex_data_layout_t*>(vertex->m_formats));

        for (size_t bone_idx = 0; bone_idx < vertex->m_bone_count; ++bone_idx)
            _jegl_free_vertex_bone_data(vertex->m_bones[bone_idx]);

        free(const_cast<je_gl_vertex_bone_data_t**>(vertex->m_bones));
        _jegl_free_resource_instance(new resource_to_destroy(vertex));
    }
}

void je_graphic_framebuf_close(je_gl_frame_buffer_t* framebuf)
{
    if (_jegl_close_resource_handle(&framebuf->m_handle))
    {
        // Close all frame rend out texture
        assert(framebuf->m_attachment_count > 0);
        basic::resource<texture2d>* attachments =
            reinterpret_cast<basic::resource<texture2d> *>(
                framebuf->m_output_attachments);

        for (size_t i = 0; i < framebuf->m_attachment_count; ++i)
            attachments[i].~shared_pointer();

        free(attachments);
        _jegl_free_resource_instance(new resource_to_destroy(framebuf));
    }
}

void je_graphic_uniformbuf_close(je_gl_uniform_buffer_t* ubuffer)
{
    if (_jegl_close_resource_handle(&ubuffer->m_handle))
    {
        free(ubuffer->m_initial_buffer);
        _jegl_free_resource_instance(new resource_to_destroy(ubuffer));
    }
}

/////////////////////////////////////////////////////////////////////////////
// Render API (v2) resource lifetime.
/////////////////////////////////////////////////////////////////////////////

static je_gl_uniform_slot_t _jegl_find_shader_uniform_slot(
    je_gl_shader_t* shader, const char* name)
{
    assert(shader != nullptr);
    assert(name != nullptr);

    for (auto* uniform = shader->m_custom_uniforms;
        uniform != nullptr;
        uniform = uniform->m_next)
    {
        if (uniform->m_uniform_type != JE_GL_SHADER_UNIFORM_TYPE_TEXTURE
            && strcmp(uniform->m_name, name) == 0)
            return je_gl_uniform_slot_t{ uniform->m_offset, uniform->m_size };
    }
    return je_gl_uniform_slot_t{ JE_GL_UNIFORM_SLOT_NONE, 0 };
}

je_gl_pipeline_t* je_graphic_pipeline_create(
    je_gl_context_t* context, const je_gl_pipeline_desc_t* desc)
{
    assert(context != nullptr);
    assert(desc != nullptr);
    assert(desc->m_shader != nullptr);

    auto* pipeline = static_cast<je_gl_pipeline_t*>(malloc(sizeof(je_gl_pipeline_t)));
    assert(pipeline != nullptr);
    memset(pipeline, 0, sizeof(je_gl_pipeline_t));

    init_resource_handle(&pipeline->m_handle, nullptr);
    pipeline->m_context = context;
    pipeline->m_shader = desc->m_shader;
    jegl_share_resource(desc->m_shader);

    if (desc->m_state != nullptr)
        pipeline->m_state = *desc->m_state;
    else
        pipeline->m_state = je_graphic_pipeline_state_from_shader(desc->m_shader);

    assert(pipeline->m_state.m_topology <= je_gl_topology_t::TRIANGLE_STRIP);
    assert(pipeline->m_state.m_front_face <= je_gl_front_face_t::CLOCKWISE);
    assert(pipeline->m_state.m_color_write_mask <= 0x0F);
    assert(pipeline->m_state.m_cull != je_gl_shader_cull_mode_t::INVALID);

    if (pipeline->m_state.m_depth_test)
    {
        assert(pipeline->m_state.m_depth_func != je_gl_shader_depth_test_method_t::INVALID);
    }
    if (pipeline->m_state.m_blend)
    {
        assert(pipeline->m_state.m_blend_equation != je_gl_shader_blend_equation_t::INVALID);
        assert(pipeline->m_state.m_blend_src_color != je_gl_shader_blend_method_t::INVALID);
        assert(pipeline->m_state.m_blend_dst_color != je_gl_shader_blend_method_t::INVALID);
        assert(pipeline->m_state.m_blend_src_alpha != je_gl_shader_blend_method_t::INVALID);
        assert(pipeline->m_state.m_blend_dst_alpha != je_gl_shader_blend_method_t::INVALID);
    }

    pipeline->m_builtin_slots.m_model =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_M");
    pipeline->m_builtin_slots.m_model_view =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_MV");
    pipeline->m_builtin_slots.m_model_view_projection =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_MVP");
    pipeline->m_builtin_slots.m_local_scale =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_LOCAL_SCALE");
    pipeline->m_builtin_slots.m_uv_tiling =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_UV_TILING");
    pipeline->m_builtin_slots.m_uv_offset =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_UV_OFFSET");
    pipeline->m_builtin_slots.m_color =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_COLOR");
    pipeline->m_builtin_slots.m_light2d_resolution =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_LIGHT2D_RESOLUTION");
    pipeline->m_builtin_slots.m_light2d_decay =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_LIGHT2D_DECAY");
    pipeline->m_builtin_slots.m_shadow_ellipse =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_SHADOW_ELLIPSE");
    pipeline->m_builtin_slots.m_shadow_key =
        _jegl_find_shader_uniform_slot(desc->m_shader, "JE_SHADOW_KEY");

    return pipeline;
}

void je_graphic_pipeline_close(je_gl_pipeline_t* pipeline)
{
    assert(pipeline != nullptr);

    if (_jegl_close_resource_handle(&pipeline->m_handle))
    {
        je_graphic_shader_close(pipeline->m_shader);
        _jegl_free_resource_instance(new resource_to_destroy(pipeline));
    }
}

je_gl_sampler_t* je_graphic_sampler_create(
    je_gl_context_t* context, const je_gl_sampler_desc_t* desc)
{
    assert(context != nullptr);
    assert(desc != nullptr);

    assert(desc->m_min_filter <= je_gl_filter_t::LINEAR);
    assert(desc->m_mag_filter <= je_gl_filter_t::LINEAR);
    assert(desc->m_mip_filter <= je_gl_mip_filter_t::LINEAR);
    assert(desc->m_u_wrap <= je_gl_wrap_mode_t::MIRRORED_REPEAT);
    assert(desc->m_v_wrap <= je_gl_wrap_mode_t::MIRRORED_REPEAT);
    assert(desc->m_w_wrap <= je_gl_wrap_mode_t::MIRRORED_REPEAT);

    auto* sampler = static_cast<je_gl_sampler_t*>(malloc(sizeof(je_gl_sampler_t)));
    assert(sampler != nullptr);
    memset(sampler, 0, sizeof(je_gl_sampler_t));

    init_resource_handle(&sampler->m_handle, nullptr);
    sampler->m_context = context;
    sampler->m_desc = *desc;

    return sampler;
}

void je_graphic_sampler_close(je_gl_sampler_t* sampler)
{
    assert(sampler != nullptr);

    if (_jegl_close_resource_handle(&sampler->m_handle))
        _jegl_free_resource_instance(new resource_to_destroy(sampler));
}

static /* OPTIONAL */ je_gl_texture_pixel_data_t* _jegl_copy_texture_initial_pixels(
    je_gl_texture_format_t format,
    /* OPTIONAL */ const je_gl_texture_pixel_data_t* initial_pixels,
    size_t width, size_t height, size_t slice_count)
{
    if (initial_pixels == nullptr)
        return nullptr;

    assert(0 == (format & JE_GL_TEXTURE_FORMAT_NON_PIXEL_FORMAT_MASK));

    const size_t pixel_data_size =
        texture_format_pixel_data_size(format, width, height) * slice_count;
    assert(pixel_data_size != 0);

    auto* pixels = (je_gl_texture_pixel_data_t*)malloc(pixel_data_size);
    assert(pixels != nullptr);

    memcpy(pixels, initial_pixels, pixel_data_size);
    return pixels;
}

je_gl_texture_2d_t* je_graphic_texture_2d_create(
    size_t width, size_t height, je_gl_texture_format_t format,
    /* OPTIONAL */ const je_gl_texture_pixel_data_t* initial_pixels)
{
    je_gl_texture_2d_t* texture_instance =
        static_cast<je_gl_texture_2d_t*>(malloc(sizeof(je_gl_texture_2d_t)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;

    texture_instance->m_initial_pixels = _jegl_copy_texture_initial_pixels(
        format, initial_pixels, width, height, 1);

    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_2D;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_format = format;

    init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

je_gl_texture_3d_t* je_graphic_texture_3d_create(
    size_t width, size_t height, size_t depth, je_gl_texture_format_t format,
    /* OPTIONAL */ const je_gl_texture_pixel_data_t* initial_pixels)
{
    je_gl_texture_3d_t* texture_instance =
        static_cast<je_gl_texture_3d_t*>(malloc(sizeof(je_gl_texture_3d_t)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    if (depth == 0)
        depth = 1;

    // 块压缩格式只用于图片加载得到的 2D 纹理。
    assert(0 == (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK));

    texture_instance->m_initial_pixels = _jegl_copy_texture_initial_pixels(
        format, initial_pixels, width, height, depth);

    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_3D;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_depth = depth;
    texture_instance->m_format = format;

    init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

je_gl_texture_array_t* je_graphic_texture_array_create(
    size_t width, size_t height, size_t layers, je_gl_texture_format_t format,
    /* OPTIONAL */ const je_gl_texture_pixel_data_t* initial_pixels)
{
    je_gl_texture_array_t* texture_instance =
        static_cast<je_gl_texture_array_t*>(malloc(sizeof(je_gl_texture_array_t)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    if (layers == 0)
        layers = 1;

    // 块压缩格式只用于图片加载得到的 2D 纹理。
    assert(0 == (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK));

    texture_instance->m_initial_pixels = _jegl_copy_texture_initial_pixels(
        format, initial_pixels, width, height, layers);

    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_ARRAY;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_layers = layers;
    texture_instance->m_format = format;

    init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

je_gl_texture_cube_t* je_graphic_texture_cube_create(
    size_t width, size_t height, je_gl_texture_format_t format,
    /* OPTIONAL */ const je_gl_texture_pixel_data_t* initial_pixels)
{
    je_gl_texture_cube_t* texture_instance =
        static_cast<je_gl_texture_cube_t*>(malloc(sizeof(je_gl_texture_cube_t)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;

    constexpr size_t CUBE_FACE_COUNT = 6;

    // 块压缩格式只用于图片加载得到的 2D 纹理。
    assert(0 == (format & JE_GL_TEXTURE_FORMAT_COMPRESSED_MASK));

    texture_instance->m_initial_pixels = _jegl_copy_texture_initial_pixels(
        format, initial_pixels, width, height, CUBE_FACE_COUNT);

    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_CUBE;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_format = format;

    init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

je_gl_texture_msaa_t* je_graphic_texture_msaa_create(
    size_t width, size_t height, size_t samples, je_gl_texture_format_t format)
{
    je_gl_texture_msaa_t* texture_instance =
        static_cast<je_gl_texture_msaa_t*>(malloc(sizeof(je_gl_texture_msaa_t)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;
    if (samples == 0)
        samples = 1;

    // MSAA 纹理只存在于 GPU 侧，不持有 CPU 像素数据
    texture_instance->m_initial_pixels = nullptr;

    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_MSAA;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_samples = samples;
    texture_instance->m_format =
        (je_gl_texture_format_t)(format | JE_GL_TEXTURE_FORMAT_FRAMEBUF);

    init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

/* OPTIONAL */ je_gl_shader_t* je_graphic_shader_source_load(
    /* OPTIONAL */ je_gl_context_t* context, const char* path, const char* src, bool is_virtual_file)
{
    if (is_virtual_file)
    {
        auto* shared_shader =
            _jegl_try_load_shared_resource<je_gl_shader_t>(
                context, path);

        if (shared_shader != nullptr)
            return shared_shader;
    }

    auto* shader_instance = _jegl_load_shader_source_impl(path, src, is_virtual_file);

    if (shader_instance != nullptr && shader_instance->m_enable_to_shared)
        return _jegl_try_update_shared_resource(
            context, shader_instance);

    return shader_instance;
}

/* OPTIONAL */ je_gl_shader_t* je_graphic_shader_load(/* OPTIONAL */ je_gl_context_t* context, const char* path)
{
    auto* shared_shader =
        _jegl_try_load_shared_resource<je_gl_shader_t>(
            context, path);

    if (shared_shader != nullptr)
        return shared_shader;

    if (je_file_t* shader_cache = je_cache_file_load(path, SHADER_CACHE_VERSION, 0))
    {
        auto* cached_shader_instance = shader_wrapper::_jegl_load_shader_cache(shader_cache, path);
        assert(cached_shader_instance != nullptr);

        if (cached_shader_instance->m_enable_to_shared)
            return _jegl_try_update_shared_resource(
                context, cached_shader_instance);

        return cached_shader_instance;
    }
    if (je_file_t* texfile = je_file_open(path))
    {
        const size_t shader_source_size = je_file_size(texfile);
        char* src = (char*)malloc(shader_source_size + 1);
        je_file_read(src, sizeof(char), shader_source_size, texfile);
        src[shader_source_size] = 0;

        je_file_close(texfile);

        auto* shader_resource = je_graphic_shader_source_load(context, path, src, false);
        free(src);

        return shader_resource;
    }
    debug::logerr("Fail to open file: '%s'", path);
    return nullptr;
}

void je_graphic_resource_handle_share(je_gl_resource_handle_t* resource_handle)
{
    ++resource_handle->m_raw_ref_count->m_binding_count;
}

// Opens the texture cache <path>.je4cache of an image. The cache is
// (re)generated only when the image itself exists; without it an existing
// cache is read as is: exported runtimes carry only the cache, and probing an
// optional image (e.g. @/icon.png) must not trigger generator failure logs.
static /* OPTIONAL */ je_file_t* _jegl_open_image_texture_cache(const char* path)
{
    if (je_file_t* image = je_file_open(path))
    {
        je_file_close(image);
        return je_image_texture_cache_open_or_create(path);
    }
    return je_cache_file_load(path, IMAGE_TEXTURE_CACHE_VERSION, 0);
}

// Reads the document written by builtin/script/internal/cache/texture.wo:
//   DOCUMENT
//     "width"       -> INT32
//     "height"      -> INT32
//     "color_space" -> INT32（0 = sRGB，1 = Linear）
//     "format"      -> INT32（0 = RGBA8，1 = BC1，2 = BC3）
//     "pixels"      -> BUFFER（对应 format 的像素/块数据，行序自底向上）
// Outputs are written only on success; *out_pixels is malloc'ed like every
// texture pixel buffer.
static bool _jegl_read_image_texture_cache(
    je_file_t* cache,
    size_t* out_width,
    size_t* out_height,
    je_image_texture_color_space_t* out_color_space,
    je_gl_texture_format_t* out_format,
    je_gl_texture_pixel_data_t** out_pixels)
{
    je_cache_binary_document_t root;
    int32_t width, height, color_space, cache_format;
    je_cache_binary_buffer_t pixels;

    if (!je_cache_binary_document_open(cache, &root)
        || !je_cache_binary_document_index(
            &root, "width", JE_CACHE_BINARY_VALUE_INT32, &width)
        || !je_cache_binary_document_index(
            &root, "height", JE_CACHE_BINARY_VALUE_INT32, &height)
        || !je_cache_binary_document_index(
            &root, "color_space", JE_CACHE_BINARY_VALUE_INT32, &color_space)
        || !je_cache_binary_document_index(
            &root, "format", JE_CACHE_BINARY_VALUE_INT32, &cache_format)
        || !je_cache_binary_document_index(
            &root, "pixels", JE_CACHE_BINARY_VALUE_BUFFER, &pixels))
        return false;

    if (width <= 0 || height <= 0)
        return false;

    je_gl_texture_format_t format = JE_GL_TEXTURE_FORMAT_RGBA;
    switch (cache_format)
    {
    case 0:
        format = JE_GL_TEXTURE_FORMAT_RGBA;
        break;
    case 1:
        format = JE_GL_TEXTURE_FORMAT_BC1;
        break;
    case 2:
        format = JE_GL_TEXTURE_FORMAT_BC3;
        break;
    default:
        return false;
    }

    const size_t pixels_size = texture_format_pixel_data_size(
        format, (size_t)width, (size_t)height);

    size_t buffer_size = 0;
    if (!je_cache_binary_buffer_read(nullptr, &buffer_size, &pixels)
        || buffer_size != pixels_size)
        return false;

    auto* const pixel_data =
        static_cast<je_gl_texture_pixel_data_t*>(malloc(pixels_size));
    if (!je_cache_binary_buffer_read(pixel_data, &buffer_size, &pixels))
    {
        free(pixel_data);
        return false;
    }

    *out_width = (size_t)width;
    *out_height = (size_t)height;
    *out_color_space = color_space == (int32_t)JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR
        ? JE_IMAGE_TEXTURE_COLOR_SPACE_LINEAR
        : JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
    *out_format = format;
    *out_pixels = pixel_data;
    return true;
}

/* OPTIONAL */ je_gl_texture_2d_t* je_graphic_texture_2d_load(/* OPTIONAL */ je_gl_context_t* context, const char* path)
{
    auto* shared_texture =
        _jegl_try_load_shared_resource<je_gl_texture_2d_t>(
            context, path);

    if (shared_texture != nullptr)
        return shared_texture;

    size_t width = 0;
    size_t height = 0;
    je_image_texture_color_space_t color_space = JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB;
    // Filled by the reader below; RGBA is only a placeholder for the
    // "no usable cache" path, where pixels stay null.
    je_gl_texture_format_t format = JE_GL_TEXTURE_FORMAT_RGBA;
    je_gl_texture_pixel_data_t* pixels = nullptr;

    if (je_file_t* cache = _jegl_open_image_texture_cache(path))
    {
        if (!_jegl_read_image_texture_cache(
            cache, &width, &height, &color_space, &format, &pixels))
        {
            // A cache that exists but cannot be parsed is corruption, not a
            // miss; report it instead of silently treating it as loadable.
            debug::logerr("Invalid image texture cache for '%s'.", path);
        }
        je_file_close(cache);
    }

    if (pixels == nullptr)
    {
        // No cache and no generator (exported runtimes ship only caches and
        // no generator scripts): there is nothing left to load from. Stay
        // quiet about plain misses: optional probes like @/icon.png end up
        // here on every start.
        debug::loginfo("Cannot load texture: no usable cache for '%s'.", path);
        return nullptr;
    }

    je_gl_texture_2d_t* texture_instance =
        static_cast<je_gl_texture_2d_t*>(malloc(sizeof(je_gl_texture_2d_t)));

    texture_instance->m_initial_pixels = pixels;
    texture_instance->m_kind = je_gl_texture_kind_t::TEXTURE_2D;
    texture_instance->m_width = width;
    texture_instance->m_height = height;
    // BC1/BC3 block data is uploaded as is by backends that support block
    // compression; the others decode it before creating the GPU texture.
    texture_instance->m_format = format;

    // Image textures default to sRGB; the <path>.je4texture property
    // source (edited from the texture properties window) can mark
    // non-color data (normal maps, masks, ...) linear.
    if (color_space == JE_IMAGE_TEXTURE_COLOR_SPACE_SRGB)
    {
        texture_instance->m_format = (je_gl_texture_format_t)(
            texture_instance->m_format | JE_GL_TEXTURE_FORMAT_SRGB);
    }

    init_resource_handle(&texture_instance->m_handle, path);

    return _jegl_try_update_shared_resource(
        context, texture_instance);
}

je_gl_vertex_t* _jegl_create_vertex_impl(
    je_gl_vertex_type_t type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const je_gl_vertex_data_layout_t* format,
    size_t format_count)
{
    size_t data_size_per_point = 0;
    for (size_t i = 0; i < format_count; ++i)
        data_size_per_point += format[i].m_count * 4; /* Both int32 & float32 is 4 byte*/

    // 先检查每顶点字节数是否为 0，再做除法，避免空 formats 时除零
    if (data_size_per_point == 0)
    {
        debug::logerr("Bad format, point donot contain any data.");
        return nullptr;
    }

    auto data_group_count = data_length / data_size_per_point;

    if (data_group_count == 0)
    {
        debug::logerr("Vertex data donot contain any completed point.");
        return nullptr;
    }

    if (data_length % data_size_per_point)
        debug::logwarn("Vertex data & format not matched, please check.");

    assert(format_count > 0);

    je_gl_vertex_t* vertex = static_cast<je_gl_vertex_t*>(malloc(sizeof(je_gl_vertex_t)));

    vertex->m_type = type;
    vertex->m_data_size_per_point = data_size_per_point;
    vertex->m_bones = nullptr;
    vertex->m_bone_count = 0;

    void* data_buffer = (void*)malloc(data_length);
    memcpy(data_buffer, datas, data_length);
    vertex->m_initial_vertexs = data_buffer;
    vertex->m_vertex_length = data_length;

    uint32_t* index_buffer = (uint32_t*)malloc(index_count * sizeof(uint32_t));
    memcpy(index_buffer, indices, index_count * sizeof(uint32_t));
    vertex->m_initial_indices = index_buffer;
    vertex->m_index_count = index_count;
    vertex->m_index_capacity = index_count;

    je_gl_vertex_data_layout_t* formats =
        (je_gl_vertex_data_layout_t*)malloc(format_count * sizeof(je_gl_vertex_data_layout_t));
    memcpy(formats, format,
        format_count * sizeof(je_gl_vertex_data_layout_t));
    vertex->m_formats = formats;
    vertex->m_format_count = format_count;

    // Calc size by default:
    float
        x_min = 0.f,
        x_max = 0.f,
        y_min = 0.f, y_max = 0.f,
        z_min = 0.f, z_max = 0.f;

    if (format[0].m_count == 3 && format[0].m_type == JE_GL_VERTEX_DATA_TYPE_FLOAT32)
    {
        // First data group is position(by default).
        x_min = x_max = static_cast<const float*>(datas)[0];
        y_min = y_max = static_cast<const float*>(datas)[1];
        z_min = z_max = static_cast<const float*>(datas)[2];

        // First data group is position(by default).
        for (size_t i = 1; i < data_group_count; ++i)
        {
            const float* fdata = reinterpret_cast<const float*>(
                static_cast<const char*>(datas) + i * data_size_per_point);

            float x = fdata[0];
            float y = fdata[1];
            float z = fdata[2];

            x_min = min(x, x_min);
            x_max = max(x, x_max);
            y_min = min(y, y_min);
            y_max = max(y, y_max);
            z_min = min(z, z_min);
            z_max = max(z, z_max);
        }
    }
    else
        debug::logwarn(
            "Position data of vertex(%p) mismatch, first data should be position and with length '3'",
            vertex);

    vertex->m_x_min = x_min;
    vertex->m_x_max = x_max;
    vertex->m_y_min = y_min;
    vertex->m_y_max = y_max;
    vertex->m_z_min = z_min;
    vertex->m_z_max = z_max;

    return vertex;
}

/* OPTIONAL */ je_gl_vertex_t* je_graphic_vertex_load(/* OPTIONAL */ je_gl_context_t* context, const char* path)
{
    const size_t MAX_BONE_COUNT = 256;

    auto* shared_vertex =
        _jegl_try_load_shared_resource<je_gl_vertex_t>(
            context, path);

    if (shared_vertex != nullptr)
        return shared_vertex;

    Assimp::Importer importer;
    importer.SetIOHandler(new Assimp::je_file_io_system());

    auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        return nullptr;
    }

    // 节点变换随遍历累计：所有子网格合并进同一个顶点缓冲时，
    // 必须把各自节点的变换烘焙进顶点数据，否则子网格位置错误。
    // Assimp 采用行向量约定（v' = v * M），子节点累计矩阵 = 自身变换 * 父累计矩阵。
    stack<pair<aiNode*, aiMatrix4x4>> m_node_stack;
    m_node_stack.push({ scene->mRootNode, scene->mRootNode->mTransformation });

    struct jegl_standard_model_vertex_t
    {
        math::vec3 m_vertex;
        math::vec2 m_texcoord;
        math::vec3 m_normal;
        math::vec3 m_tangent;

        int32_t m_bone_id[4];
        float m_bone_weight[4];
    };
    static_assert(sizeof(jegl_standard_model_vertex_t) == 76);

    const je_gl_vertex_data_layout_t jegl_standard_model_vertex_format[] = {
        {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
        {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2},
        {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
        {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3},
        {JE_GL_VERTEX_DATA_TYPE_INT32, 4},
        {JE_GL_VERTEX_DATA_TYPE_FLOAT32, 4},
    };

    vector<jegl_standard_model_vertex_t> vertex_datas;
    vector<uint32_t> index_datas;
    vector<const je_gl_vertex_bone_data_t*> bones;

    while (!m_node_stack.empty())
    {
        auto node_info = m_node_stack.top();
        m_node_stack.pop();

        auto* current_node = node_info.first;
        const aiMatrix4x4& node_transform = node_info.second;

        for (size_t i = 0; i < current_node->mNumChildren; ++i)
            m_node_stack.push({
                current_node->mChildren[i],
                current_node->mChildren[i]->mTransformation * node_transform });

        size_t bone_counter = 0;

        for (size_t i = 0; i < current_node->mNumMeshes; ++i)
        {
            auto* mesh = scene->mMeshes[current_node->mMeshes[i]];
            const size_t current_model_vertex_offset = vertex_datas.size();

            const bool has_texcrood_0 = mesh->HasTextureCoords(0);
            const bool has_normals = mesh->HasNormals();

            // 法线只应用线性部分（不含平移）；随后单位化以应对非等比缩放
            const aiMatrix3x3 normal_transform(node_transform);

            for (size_t index = 0; index < mesh->mNumVertices; ++index)
            {
                const aiVector3D vertex = node_transform * mesh->mVertices[index];

                jegl_standard_model_vertex_t model_vertex = {};
                model_vertex.m_vertex = math::vec3(
                    vertex.x,
                    vertex.y,
                    vertex.z);

                if (has_texcrood_0)
                {
                    model_vertex.m_texcoord = math::vec2(
                        mesh->mTextureCoords[0][index].x,
                        mesh->mTextureCoords[0][index].y);
                }
                if (has_normals)
                {
                    const aiVector3D normal = normal_transform * mesh->mNormals[index];
                    model_vertex.m_normal = math::vec3(
                        normal.x,
                        normal.y,
                        normal.z).unit();
                }

                model_vertex.m_bone_id[0] = -1;
                model_vertex.m_bone_id[1] = -1;
                model_vertex.m_bone_id[2] = -1;
                model_vertex.m_bone_id[3] = -1;

                vertex_datas.push_back(model_vertex);
            }
            for (size_t f = 0; f < mesh->mNumFaces; ++f)
            {
                auto& face = mesh->mFaces[f];
                assert(face.mNumIndices == 3);

                auto p0idx = face.mIndices[0] + current_model_vertex_offset;
                auto p1idx = face.mIndices[1] + current_model_vertex_offset;
                auto p2idx = face.mIndices[2] + current_model_vertex_offset;

                index_datas.push_back(p0idx);
                index_datas.push_back(p1idx);
                index_datas.push_back(p2idx);

                // NOTE: 在此计算切线向量，对于多个面共用的顶点，将计算的切线全部累加，最后取单位向量
                //  不使用模型自带的切线。

                auto& p0 = vertex_datas[p0idx];
                auto& p1 = vertex_datas[p1idx];
                auto& p2 = vertex_datas[p2idx];

                const auto edge10 = p1.m_vertex - p0.m_vertex;
                const auto edge20 = p2.m_vertex - p0.m_vertex;

                const auto duv10 = p1.m_texcoord - p0.m_texcoord;
                const auto duv20 = p2.m_texcoord - p0.m_texcoord;

                const float normalize_factor = 1.0f / (duv10.x * duv20.y - duv20.x * duv10.y);
                const math::vec3 tangent =
                    (math::vec3(
                        duv20.y * edge10.x - duv10.y * edge20.x,
                        duv20.y * edge10.y - duv10.y * edge20.y,
                        duv20.y * edge10.z - duv10.y * edge20.z) * normalize_factor).unit();

                vertex_datas[p0idx].m_tangent += tangent;
                vertex_datas[p1idx].m_tangent += tangent;
                vertex_datas[p2idx].m_tangent += tangent;
            }
            for (size_t bone_idx = 0; bone_idx < mesh->mNumBones; ++bone_idx)
            {
                je_gl_vertex_bone_data_t* bone_data =
                    (je_gl_vertex_bone_data_t*)malloc(sizeof(je_gl_vertex_bone_data_t));

                assert(bone_data != nullptr);
                bone_data->m_name = basic::make_new_string(mesh->mBones[bone_idx]->mName.C_Str());
                bone_data->m_index = bone_counter++;
                assert(bone_data->m_name != nullptr);

                const auto& offset_matrix = mesh->mBones[bone_idx]->mOffsetMatrix;
                for (size_t ix = 0; ix < 4; ++ix)
                {
                    for (size_t iy = 0; iy < 4; ++iy)
                    {
                        bone_data->m_m2b_trans[ix][iy] = offset_matrix[iy][ix];
                    }
                }

                bones.push_back(bone_data);
                const size_t bone_id = bone_data->m_index;

                if (bone_id >= MAX_BONE_COUNT)
                    // Too many bones, skip to avoid overflow.
                    continue;

                auto* weights = mesh->mBones[bone_idx]->mWeights;
                size_t numWeights = (size_t)mesh->mBones[bone_idx]->mNumWeights;

                for (size_t weight_index = 0; weight_index < numWeights; ++weight_index)
                {
                    auto& weight = weights[weight_index];
                    auto& vertex = vertex_datas[weight.mVertexId + current_model_vertex_offset];

                    size_t min_weight_index = 0;
                    for (size_t i = 1; i < 4; ++i)
                    {
                        if (vertex.m_bone_id[i] == -1
                            || abs(vertex.m_bone_weight[i]) < abs(vertex.m_bone_weight[min_weight_index]))
                            min_weight_index = i;
                    }

                    if (vertex.m_bone_weight[min_weight_index] < weight.mWeight)
                    {
                        vertex.m_bone_id[min_weight_index] = (int32_t)bone_id;
                        vertex.m_bone_weight[min_weight_index] = weight.mWeight;
                    }
                }
            }
        }
    }

    for (auto& vdata : vertex_datas)
    {
        // Normalize tangent vector
        vdata.m_tangent = vdata.m_tangent.unit();

        for (size_t i = 0; i < 4; ++i)
        {
            if (vdata.m_bone_id[i] == -1)
            {
                vdata.m_bone_id[i] = 0;
                vdata.m_bone_weight[i] = 0.f;
            }
        }
    }

    auto* vertex = _jegl_create_vertex_impl(
        JE_GL_VERTEX_TYPE_TRIANGLES,
        vertex_datas.data(),
        vertex_datas.size() * sizeof(jegl_standard_model_vertex_t),
        index_datas.data(),
        index_datas.size(),
        jegl_standard_model_vertex_format,
        sizeof(jegl_standard_model_vertex_format) / sizeof(je_gl_vertex_data_layout_t));

    if (vertex != nullptr)
    {
        const je_gl_vertex_bone_data_t** cbones =
            (const je_gl_vertex_bone_data_t**)malloc(
                bones.size() * sizeof(const je_gl_vertex_bone_data_t*));

        memcpy(
            cbones,
            bones.data(),
            bones.size() * sizeof(const je_gl_vertex_bone_data_t*));

        vertex->m_bones = cbones;
        vertex->m_bone_count = bones.size();

        init_resource_handle(&vertex->m_handle, path);

        return _jegl_try_update_shared_resource(
            context, vertex);
    }
    else
    {
        for (auto* bonedata : bones)
            _jegl_free_vertex_bone_data(bonedata);
    }
    return nullptr;
}


/* OPTIONAL: If bad format or no completed vertex point */ je_gl_vertex_t* je_graphic_vertex_create(
    je_gl_vertex_type_t type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const je_gl_vertex_data_layout_t* format,
    size_t format_count)
{
    auto* vertex = _jegl_create_vertex_impl(
        type,
        datas,
        data_length,
        indices,
        index_count,
        format,
        format_count);

    if (vertex != nullptr)
        init_resource_handle(&vertex->m_handle, nullptr);

    return vertex;
}

/* OPTIONAL: If size or color_attachment_count is zero */ je_gl_frame_buffer_t* je_graphic_framebuf_create(
    size_t width,
    size_t height,
    const je_gl_texture_format_t* color_attachment_formats,
    size_t color_attachment_count,
    bool contain_depth_attachment)
{
    if (width == 0 || height == 0 || color_attachment_count == 0)
    {
        debug::logerr("Failed to create invalid framebuffer: size is zero or no attachment.");
        return nullptr;
    }

    je_gl_frame_buffer_t* framebuf =
        static_cast<je_gl_frame_buffer_t*>(
            malloc(sizeof(je_gl_frame_buffer_t)));

    framebuf->m_attachment_count =
        color_attachment_count + (contain_depth_attachment ? 1 : 0);
    framebuf->m_width = width;
    framebuf->m_height = height;

    basic::resource<texture2d>* attachments =
        static_cast<basic::resource<texture2d> *>(
            malloc(
                framebuf->m_attachment_count
                * sizeof(basic::resource<texture2d>)));

    for (size_t i = 0; i < color_attachment_count; ++i)
        new (&attachments[i]) basic::resource<texture2d>(
            texture2d::create(
                width,
                height,
                je_gl_texture_format_t(
                    color_attachment_formats[i]
                    | JE_GL_TEXTURE_FORMAT_FRAMEBUF)));

    if (contain_depth_attachment)
        new (&attachments[color_attachment_count]) basic::resource<texture2d>(
            texture2d::create(
                width,
                height,
                je_gl_texture_format_t(
                    JE_GL_TEXTURE_FORMAT_DEPTH
                    | JE_GL_TEXTURE_FORMAT_FRAMEBUF)));

    framebuf->m_output_attachments =
        (je_gl_frame_buffer_attachment_t*)attachments;

    init_resource_handle(&framebuf->m_handle, nullptr);
    return framebuf;
}

je_gl_uniform_buffer_t* je_graphic_uniformbuf_create(
    size_t binding_place,
    size_t length)
{
    je_gl_uniform_buffer_t* uniformbuf =
        static_cast<je_gl_uniform_buffer_t*>(
            malloc(sizeof(je_gl_uniform_buffer_t)));

    // 一致变量缓冲区创建后不保证初始化（接口不承诺内容）；这里清零只是让会读取
    // 该缓冲的后端得到确定的字节。运行时内容由 je_graphic_cmd_update_uniform_buffer
    // 按帧录制
    uniformbuf->m_initial_buffer = (uint8_t*)calloc(length, 1);
    uniformbuf->m_buffer_size = length;
    uniformbuf->m_buffer_binding_place = binding_place;

    init_resource_handle(&uniformbuf->m_handle, nullptr);
    return uniformbuf;
}
