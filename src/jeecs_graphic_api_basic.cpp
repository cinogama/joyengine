#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_cache_version.hpp"

#define JE_IMPL
#include "jeecs.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>

#include <forward_list>

namespace Assimp
{
    class je_file_io_stream : public IOStream
    {
        JECS_DISABLE_MOVE_AND_COPY(je_file_io_stream);

        jeecs_file* m_file;

    public:
        je_file_io_stream(jeecs_file* fres)
            : m_file(fres)
        {
            assert(m_file != nullptr);
        }

        virtual ~je_file_io_stream()
        {
            jeecs_file_close(m_file);
        }

        virtual size_t Read(
            void* pvBuffer,
            size_t pSize,
            size_t pCount) override
        {
            return jeecs_file_read(pvBuffer, pSize, pCount, m_file);
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
            static_assert((int)aiOrigin::aiOrigin_CUR == (int)je_read_file_seek_mode::JE_READ_FILE_SEEK_CURRENT);
            static_assert((int)aiOrigin::aiOrigin_END == (int)je_read_file_seek_mode::JE_READ_FILE_SEEK_END);
            static_assert((int)aiOrigin::aiOrigin_SET == (int)je_read_file_seek_mode::JE_READ_FILE_SEEK_SET);

            jeecs_file_seek(m_file, (int64_t)pOffset, (je_read_file_seek_mode)pOrigin);
            return aiReturn::aiReturn_SUCCESS;
        }

        // -------------------------------------------------------------------
        /** @brief Get the current position of the read/write cursor
         *
         * See ftell() for more details */
        virtual size_t Tell() const override
        {
            return jeecs_file_tell(m_file);
        }

        // -------------------------------------------------------------------
        /** @brief Returns filesize
         *  Returns the filesize. */
        virtual size_t FileSize() const override
        {
            return m_file->m_file_length;
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

        std::string m_current_dir;

    public:
        je_file_io_system()
            : m_current_dir(jeecs_file_get_runtime_path())
        {
        }

        bool Exists(const char* pFile) const override
        {
            auto* f = jeecs_file_open(pFile);
            if (f == nullptr)
                return false;

            jeecs_file_close(f);
            return true;
        }

        char getOsSeparator() const override
        {
            return '/'; // why not? it doesn't care
        }

        IOStream* Open(const char* pFile, const char* pMode = "rb") override
        {
            auto* f = jeecs_file_open(pFile);
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

        bool PushDirectory(const std::string& path) override
        {
            return false;
        }

        const std::string& CurrentDirectory() const override
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

        bool CreateDirectory(const std::string& path) override
        {
            return false;
        }

        bool ChangeDirectory(const std::string& path) override
        {
            return false;
        }

        bool DeleteFile(const std::string& file) override
        {
            return false;
        }
    };
}

#if !JE4_STB_IMAGE_STATIC_IMPLED
#define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>

#include <condition_variable>
#include <list>
#include <future>
#include <stack>
#include <queue>

struct jegl_resource_bind_counter
{
    JECS_DISABLE_MOVE_AND_COPY(jegl_resource_bind_counter);

    std::atomic_int32_t m_binding_count;
    jegl_resource_bind_counter(int32_t init)
        : m_binding_count(init)
    {
    }
};

namespace jeecs::graphic
{
    struct created_resource
    {
        enum class kind
        {
            UNKNOWN,
            SHADER,
            TEXTURE,
            VERTEX,
            UNIFORM_BUFFER,
            FRAME_BUFFER
        };
        union deleting_graphic_resource
        {
            void* m_raw_ptr;

            jegl_shader* m_shader;
            jegl_texture* m_texture;
            jegl_vertex* m_vertex;
            jegl_uniform_buffer* m_uniform_buffer;
            jegl_frame_buffer* m_frame_buffer;
        };

        kind m_type;
        deleting_graphic_resource m_resource;

        created_resource(const created_resource&) = default;
        created_resource(created_resource&&) = default;
        created_resource& operator = (const created_resource&) = default;
        created_resource& operator = (created_resource&&) = default;
        ~created_resource() = default;

        constexpr created_resource(jegl_shader* shader)
            : m_type(kind::SHADER)
        {
            m_resource.m_shader = shader;
        }
        constexpr created_resource(jegl_texture* texture)
            : m_type(kind::TEXTURE)
        {
            m_resource.m_texture = texture;
        }
        constexpr created_resource(jegl_vertex* vertex)
            : m_type(kind::VERTEX)
        {
            m_resource.m_vertex = vertex;
        }
        constexpr created_resource(jegl_uniform_buffer* uniform_buffer)
            : m_type(kind::UNIFORM_BUFFER)
        {
            m_resource.m_uniform_buffer = uniform_buffer;
        }
        constexpr created_resource(jegl_frame_buffer* frame_buffer)
            : m_type(kind::FRAME_BUFFER)
        {
            m_resource.m_frame_buffer = frame_buffer;
        }
        bool operator == (const created_resource& another) const noexcept
        {
            return m_resource.m_raw_ptr == another.m_resource.m_raw_ptr;
        }

        template<requirements::basic_graphic_resource T>
        T* resource() const
        {
            if constexpr (std::is_same_v<T, jegl_shader>)
            {
                assert(m_type == kind::SHADER);
                return m_resource.m_shader;
            }
            else if constexpr (std::is_same_v<T, jegl_texture>)
            {
                assert(m_type == kind::TEXTURE);
                return m_resource.m_texture;
            }
            else if constexpr (std::is_same_v<T, jegl_vertex>)
            {
                assert(m_type == kind::VERTEX);
                return m_resource.m_vertex;
            }
            else if constexpr (std::is_same_v<T, jegl_uniform_buffer>)
            {
                assert(m_type == kind::UNIFORM_BUFFER);
                return m_resource.m_uniform_buffer;
            }
            else if constexpr (std::is_same_v<T, jegl_frame_buffer>)
            {
                assert(m_type == kind::FRAME_BUFFER);
                return m_resource.m_frame_buffer;
            }
            else
            {
                static_assert(!sizeof(T*), "Unsupported resource type.");
            }
        }

        template<requirements::basic_graphic_resource T>
        T* try_resource() const
        {
            if constexpr (std::is_same_v<T, jegl_shader>)
            {
                if (m_type != kind::SHADER)
                    return nullptr;
                return m_resource.m_shader;
            }
            else if constexpr (std::is_same_v<T, jegl_texture>)
            {
                if (m_type != kind::TEXTURE)
                    return nullptr;
                return m_resource.m_texture;
            }
            else if constexpr (std::is_same_v<T, jegl_vertex>)
            {
                if (m_type != kind::VERTEX)
                    return nullptr;
                return m_resource.m_vertex;
            }
            else if constexpr (std::is_same_v<T, jegl_uniform_buffer>)
            {
                if (m_type != kind::UNIFORM_BUFFER)
                    return nullptr;
                return m_resource.m_uniform_buffer;
            }
            else if constexpr (std::is_same_v<T, jegl_frame_buffer>)
            {
                if (m_type != kind::FRAME_BUFFER)
                    return nullptr;
                return m_resource.m_frame_buffer;
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
                jegl_close_shader(m_resource.m_shader);
                break;
            case kind::TEXTURE:
                jegl_close_texture(m_resource.m_texture);
                break;
            case kind::VERTEX:
                jegl_close_vertex(m_resource.m_vertex);
                break;
            case kind::UNIFORM_BUFFER:
                jegl_close_uniformbuf(m_resource.m_uniform_buffer);
                break;
            case kind::FRAME_BUFFER:
                jegl_close_framebuf(m_resource.m_frame_buffer);
                break;

            default:
                abort();
            }
        }
        void free_resouce_body() const
        {
            free(m_resource.m_raw_ptr);
        }
        jegl_resource_handle* get_handle() const
        {
            switch (m_type)
            {
            case kind::SHADER:
                return &m_resource.m_shader->m_handle;
            case kind::TEXTURE:
                return &m_resource.m_texture->m_handle;
            case kind::VERTEX:
                return &m_resource.m_vertex->m_handle;
            case kind::UNIFORM_BUFFER:
                return &m_resource.m_uniform_buffer->m_handle;
            case kind::FRAME_BUFFER:
                return &m_resource.m_frame_buffer->m_handle;
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
        jegl_resource_blob m_blob_may_null;
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

namespace std
{
    template<>
    struct hash<jeecs::graphic::created_resource>
    {
        size_t operator()(const jeecs::graphic::created_resource& res) const noexcept
        {
            return std::hash<void*>()(res.m_resource.m_raw_ptr);
        }
    };
}

struct jegl_context_notifier
{
    struct cached_resource_statement
    {
        jeecs::graphic::created_resource m_resource;
        uint8_t m_keeped_count;

        template<jeecs::graphic::requirements::basic_graphic_resource T>
        cached_resource_statement(T* res)
            : m_resource(res)
            , m_keeped_count(0)
        {
        }

        cached_resource_statement(
            const cached_resource_statement&) = default;
        cached_resource_statement(
            cached_resource_statement&&) = default;
        cached_resource_statement& operator = (
            const cached_resource_statement&) = default;
        cached_resource_statement& operator = (
            cached_resource_statement&&) = default;

        bool try_drop()
        {
            constexpr uint8_t MAX_KEEP_COUNT = 15;

            if (m_resource.get_handle()->m_raw_ref_count->m_binding_count.load() > 1)
            {
                // 此资源依然活跃，为了避免频繁释放，通过 keep 机制延迟释放。
                if (++m_keeped_count > MAX_KEEP_COUNT)
                    m_keeped_count = MAX_KEEP_COUNT;
            }
            else if (--m_keeped_count == 0)
            {
                // 此资源没有其他持有者，并且 keep 计数归零，可以回收。
                m_resource.drop_resource();
                return true;
            }
            return false;
        }
    };

    using cached_resource_map_t =
        std::unordered_map<std::string, cached_resource_statement>;
    using cached_resource_blob_t =
        std::unordered_map<std::string, jeecs::graphic::cached_resource_blob>;
    using outdated_resource_blob_t =
        std::unordered_set<std::string>;
    using inited_resource_set_t =
        std::unordered_set<jeecs::graphic::created_resource>;
    using closing_resource_lock_free_list_t =
        jeecs::basic::atomic_list<jeecs::graphic::resource_to_destroy>;

    bool m_graphic_terminated;
    bool m_graphic_reboot;

    std::mutex m_update_mx;
    std::condition_variable m_update_waiter;
    bool m_update_request_flag;

    std::promise<void> m_promise;

    // 已经加载的资源缓存，会有一个清理机制，在必要时释放没有人使用的资源。
    std::shared_mutex _m_cached_resources_mx;
    cached_resource_map_t _m_cached_resources;

    // 资源的 blob 集合
    std::mutex _m_outdated_resource_blobs_mx;
    outdated_resource_blob_t _m_outdated_resource_blobs;

    // NOTE: 对以下资源的访问不需要上锁，因为对此的操作都被局限在图形线程内部，不会发生并发访问。
    size_t _m_increased_gpu_memory_size;                // 新增的 GPU 显存占用估算值，用于指示清除缓存时机。
    cached_resource_blob_t _m_created_resource_blobs;   // 已经创建的资源 blob。
    inited_resource_set_t _m_created_resources;         // 已经初始化的资源集合。
    closing_resource_lock_free_list_t _m_closing_resources; // 等待销毁的资源列表。
};

namespace jeecs::graphic
{
    thread_local jegl_context* _current_graphic_thread = nullptr;

    std::vector<jegl_context*> _jegl_alive_glthread_list;
    std::shared_mutex _jegl_alive_glthread_list_mx;

    jeecs_sync_callback_func_t _jegl_sync_callback_func = nullptr;
    void* _jegl_sync_callback_arg = nullptr;

    enum class jegl_resouce_state
    {
        READY,
        NEED_INIT,
        NEED_UPDATE,
        INVALID_CONTEXT,
    };
}

#if JE4_ENABLE_SHADER_WRAP_GENERATOR
struct shader_wrapper;
void jegl_shader_generate_shader_source(shader_wrapper* shader_generator, jegl_shader* write_to_shader);
#endif
void jegl_shader_free_generated_shader_source(jegl_shader* write_to_shader);

//////////////////////////////////// API /////////////////////////////////////////
void jegl_sync_init(jegl_context* thread, bool isreboot)
{
    if (jeecs::graphic::_current_graphic_thread == nullptr)
        jeecs::graphic::_current_graphic_thread = thread;

    assert(jeecs::graphic::_current_graphic_thread == thread);

    if (thread->m_universe_instance != nullptr)
    {
        if (thread->m_config.m_fps == 0)
            je_ecs_universe_set_frame_deltatime(
                thread->m_universe_instance,
                0.0);
        else
            je_ecs_universe_set_frame_deltatime(
                thread->m_universe_instance,
                1.0 / (double)thread->m_config.m_fps);
    }

    thread->m_graphic_impl_context = thread->m_apis->interface_startup(
        thread, &thread->m_config, isreboot);

    ++thread->m_version;
}

void _jegl_close_resouce_instance_by_api(
    jegl_context* thread, const jeecs::graphic::created_resource* res)
{
    switch (res->m_type)
    {
    case jeecs::graphic::created_resource::kind::SHADER:
        thread->m_apis->shader_close(
            thread->m_graphic_impl_context,
            res->resource<jegl_shader>());
        break;
    case jeecs::graphic::created_resource::kind::TEXTURE:
        thread->m_apis->texture_close(
            thread->m_graphic_impl_context,
            res->resource<jegl_texture>());
        break;
    case jeecs::graphic::created_resource::kind::VERTEX:
        thread->m_apis->vertex_close(
            thread->m_graphic_impl_context,
            res->resource<jegl_vertex>());
        break;
    case jeecs::graphic::created_resource::kind::UNIFORM_BUFFER:
        thread->m_apis->ubuffer_close(
            thread->m_graphic_impl_context,
            res->resource<jegl_uniform_buffer>());
        break;
    case jeecs::graphic::created_resource::kind::FRAME_BUFFER:
        thread->m_apis->framebuffer_close(
            thread->m_graphic_impl_context,
            res->resource<jegl_frame_buffer>());
        break;
    default:
        jeecs::debug::logfatal(
            "Unknown graphic resource `%d` type when destroying resource in graphic thread.",
            (int)res->m_type);
    }
}

void _jegl_close_resource_blob_by_api(
    jegl_context* thread, const jeecs::graphic::cached_resource_blob* blob)
{
    if (blob->m_blob_may_null == nullptr)
        // No need to free.
        return;

    switch (blob->m_type)
    {
    case jeecs::graphic::created_resource::kind::SHADER:
        thread->m_apis->shader_close_blob(
            thread->m_graphic_impl_context,
            blob->m_blob_may_null);
        break;
    case jeecs::graphic::created_resource::kind::TEXTURE:
        thread->m_apis->texture_close_blob(
            thread->m_graphic_impl_context,
            blob->m_blob_may_null);
        break;
    case jeecs::graphic::created_resource::kind::VERTEX:
        thread->m_apis->vertex_close_blob(
            thread->m_graphic_impl_context,
            blob->m_blob_may_null);
        break;
    default:
        jeecs::debug::logfatal(
            "Unknown graphic resource blob `%d` type when destroying resource blob in graphic thread.",
            (int)blob->m_type);
    }
}

jegl_sync_state jegl_sync_update(jegl_context* thread)
{
    std::unordered_map<
        jeecs::graphic::resource_to_destroy*,
        bool /* Need close graphic resource */>
        _waiting_to_free_resource;

    auto* notifier = thread->_m_thread_notifier;

    if (!notifier->m_graphic_terminated)
    {
        do
        {
            std::unique_lock uq1(notifier->m_update_mx);
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
        case jegl_update_action::JEGL_UPDATE_STOP:
            // graphic thread want to exit. mark stop update.
            notifier->m_graphic_terminated = true;
            break;
        case jegl_update_action::JEGL_UPDATE_SKIP:
            if (thread->m_config.m_fps == 0)
                je_clock_sleep_for(1.0 / (double)60.f);
            else
                je_clock_sleep_for(1.0 / (double)thread->m_config.m_fps);
            break;
        case jegl_update_action::JEGL_UPDATE_CONTINUE:
            // do nothing.
            break;
        }

        thread->_m_frame_rend_work(
            thread,
            thread->_m_frame_rend_work_arg,
            frame_update_state);

        if (jegl_update_action::JEGL_UPDATE_STOP ==
            thread->m_apis->update_draw_commit(
                thread->m_graphic_impl_context, frame_update_state))
        {
            notifier->m_graphic_terminated = true;
        }

        // 当资源新增估计占用值达到 GPU_MEMORY_CLEANUP_THRESHOLD 时，审视一遍缓存资源，释放没有被使用的资源。
        //      HINT: 这个数值约等于四个 2048 * 2048 RGBA8 贴图的大小。
        constexpr size_t GPU_MEMORY_CLEANUP_THRESHOLD = 64 * 1024 * 1024;
        if (notifier->_m_increased_gpu_memory_size >= GPU_MEMORY_CLEANUP_THRESHOLD)
        {
            // 显存资源每新增 GPU_MEMORY_CLEANUP_THRESHOLD 字节，就审视一遍是否有可以释放的缓存资源。
            notifier->_m_increased_gpu_memory_size -= GPU_MEMORY_CLEANUP_THRESHOLD;

            std::lock_guard g1(notifier->_m_cached_resources_mx);
            std::forward_list<const char*> resources_to_erase;

            for (auto& [res_key, cached_res] : notifier->_m_cached_resources)
            {
                if (cached_res.try_drop())
                {
                    // 标记为待移除
                    resources_to_erase.push_front(res_key.c_str());
                }
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
            std::lock_guard g1(notifier->m_update_mx);
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

            deleting_resource->m_resource.free_resouce_body();
            delete deleting_resource;
        }
    }
    else
    {
    is_reboot_or_shutdown:
        if (notifier->m_graphic_reboot)
        {
            notifier->m_graphic_reboot = false;
            return jegl_sync_state::JEGL_SYNC_REBOOT;
        }
        return jegl_sync_state::JEGL_SYNC_SHUTDOWN;
    }

    return jegl_sync_state::JEGL_SYNC_COMPLETE;
}

bool jegl_sync_shutdown(jegl_context* thread, bool isreboot)
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
        jeecs::graphic::_current_graphic_thread = nullptr;
        return true;
    }
    return false;
}

void jegl_register_sync_thread_callback(
    jeecs_sync_callback_func_t callback, void* arg)
{
    assert(callback != nullptr);

    jeecs::graphic::_jegl_sync_callback_func = callback;
    jeecs::graphic::_jegl_sync_callback_arg = arg;
}

jegl_context* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    jegl_context::frame_job_func_t frame_rend_work,
    void* arg)
{
    jegl_context* thread_handle = nullptr;

    if (config.m_title == nullptr)
    {
        jeecs::debug::logerr("Cannot startup graphic thread without valid title.");
    }
    else if (register_func != nullptr)
    {
        thread_handle = new jegl_context();

        thread_handle->m_version = 0;
        thread_handle->_m_thread_notifier = new jegl_context_notifier();
        thread_handle->m_apis = new jegl_graphic_api();

        memset(thread_handle->m_apis, 0, sizeof(jegl_graphic_api));
        register_func(thread_handle->m_apis);

        size_t err_api_no = 0;
        for (void** reador = (void**)thread_handle->m_apis;
            reador < (void**)(thread_handle->m_apis + 1);
            ++reador)
        {
            if (!*reador)
            {
                err_api_no++;
                jeecs::debug::logerr("GraphicAPI function: %zu is invalid.",
                    (size_t)(reador - (void**)thread_handle->m_apis));
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
            std::lock_guard g1(jeecs::graphic::_jegl_alive_glthread_list_mx);

            if (jeecs::graphic::_jegl_alive_glthread_list.end() ==
                std::find(
                    jeecs::graphic::_jegl_alive_glthread_list.begin(),
                    jeecs::graphic::_jegl_alive_glthread_list.end(),
                    thread_handle))
            {
                jeecs::graphic::_jegl_alive_glthread_list.push_back(thread_handle);
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

        assert(jeecs::graphic::_jegl_sync_callback_func != nullptr);
        thread_handle->_m_sync_callback_arg = jeecs::graphic::_jegl_sync_callback_arg;
        jeecs::graphic::_jegl_sync_callback_func(thread_handle, jeecs::graphic::_jegl_sync_callback_arg);
    }
    else
        jeecs::debug::logfatal("Fail to start up graphic thread, abort and return nullptr.");

    return thread_handle;
}

void jegl_finish()
{
    std::vector<jegl_context*> shutdown_glthreads;
    do
    {
        std::lock_guard g1(jeecs::graphic::_jegl_alive_glthread_list_mx);
        shutdown_glthreads = jeecs::graphic::_jegl_alive_glthread_list;
    } while (0);
    for (auto alive_glthread : shutdown_glthreads)
    {
        jegl_terminate_graphic_thread(alive_glthread);
    }
}

void jegl_terminate_graphic_thread(jegl_context* thread)
{
    do
    {
        std::lock_guard g1(jeecs::graphic::_jegl_alive_glthread_list_mx);
        auto fnd = std::find(
            jeecs::graphic::_jegl_alive_glthread_list.begin(),
            jeecs::graphic::_jegl_alive_glthread_list.end(),
            thread);
        if (fnd == jeecs::graphic::_jegl_alive_glthread_list.end())
            return;
        else
        {
            jeecs::graphic::_jegl_alive_glthread_list.erase(fnd);
        }

    } while (0);

    thread->_m_thread_notifier->m_graphic_terminated = true;

    do
    {
        std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_request_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    // Sync thread and wait for shutting-down
    thread->_m_thread_notifier->m_promise.get_future().get();

    // Close shared resources in contexrt.
    do
    {
        std::shared_lock sg1(thread->_m_thread_notifier->_m_cached_resources_mx);
        for (auto& [_path, cached_resource] : thread->_m_thread_notifier->_m_cached_resources)
        {
            (void)_path;
            cached_resource.m_resource.drop_resource();
        }
    } while (0);

    auto* closing_resource = thread->_m_thread_notifier->_m_closing_resources.pick_all();
    while (closing_resource)
    {
        auto* cur_closing_resource = closing_resource;
        closing_resource = closing_resource->last;

        cur_closing_resource->m_resource.free_resouce_body();
        delete cur_closing_resource;
    }

    // delete std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update));
    delete thread->_m_thread_notifier;
    delete thread->m_apis;
    delete thread;
}

bool jegl_update(
    jegl_context* thread,
    jegl_update_sync_mode mode,
    jegl_update_sync_callback_t callback_after_wait_may_null,
    void* callback_param)
{
    if (thread->_m_thread_notifier->m_graphic_terminated)
        return false;

    std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
    if (mode == jegl_update_sync_mode::JEGL_WAIT_LAST_FRAME_END)
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

    if (mode == jegl_update_sync_mode::JEGL_WAIT_THIS_FRAME_END)
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

void jegl_reboot_graphic_thread(jegl_context* thread_handle, const jegl_interface_config* config_may_null)
{
    if (config_may_null != nullptr)
        thread_handle->m_config = *config_may_null;

    thread_handle->_m_thread_notifier->m_graphic_reboot = true;
}

bool jegl_mark_shared_resources_outdated(
    jegl_context* context, const char* path)
{
    std::lock_guard g1(context->_m_thread_notifier->_m_outdated_resource_blobs_mx);
    if (context->_m_thread_notifier->_m_outdated_resource_blobs.insert(path).second)
    {
        std::lock_guard g2(context->_m_thread_notifier->_m_cached_resources_mx);

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

jeecs::graphic::cached_resource_blob::kind _jegl_try_get_resource_blob(
    jegl_resource_handle* resource_handle,
    jegl_resource_blob* out_blob)
{
    jeecs::graphic::cached_resource_blob::kind found_blob_type =
        jeecs::graphic::cached_resource_blob::kind::UNKNOWN;

    jegl_resource_blob blob = nullptr;

    if (resource_handle->m_path_may_null_if_builtin != nullptr)
    {
        auto* graphic_thread_context =
            jeecs::graphic::_current_graphic_thread->_m_thread_notifier;

        auto& cached_resource_blob_list =
            graphic_thread_context->_m_created_resource_blobs;

        auto blob_fnd = cached_resource_blob_list.find(resource_handle->m_path_may_null_if_builtin);
        if (blob_fnd != cached_resource_blob_list.end())
        {
            // Check if outdated?
            std::lock_guard g1(graphic_thread_context->_m_outdated_resource_blobs_mx);
            auto fnd = graphic_thread_context->_m_outdated_resource_blobs.find(
                resource_handle->m_path_may_null_if_builtin);

            if (fnd != graphic_thread_context->_m_outdated_resource_blobs.end())
            {
                // Already outdated.
                _jegl_close_resource_blob_by_api(
                    jeecs::graphic::_current_graphic_thread, &blob_fnd->second);

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
    jegl_resource_handle* resource_handle,
    jeecs::graphic::cached_resource_blob::kind blob_type,
    jegl_resource_blob blob_may_null)
{
    if (resource_handle->m_path_may_null_if_builtin == nullptr)
    {
        if (blob_may_null != nullptr)
        {
            jeecs::debug::logwarn(
                "Cannot cache resource blob for anonymous resource.");

            return false;
        }

        // If blob is null, return true to avoid useless blob close operation.
        return true;
    }

    assert(resource_handle->m_path_may_null_if_builtin != nullptr);

    auto result = jeecs::graphic::_current_graphic_thread->_m_thread_notifier
        ->_m_created_resource_blobs.insert(
            std::make_pair(
                resource_handle->m_path_may_null_if_builtin,
                jeecs::graphic::cached_resource_blob
                {
                    blob_type,
                    blob_may_null,
                }));

    if (blob_may_null != nullptr)
        return result.second;

    // If blob is null, return true to avoid useless blob close operation.
    return true;
}

template<jeecs::graphic::requirements::basic_graphic_resource T>
T* /* MAY NULL */ _jegl_try_load_shared_resource(jegl_context* context, const char* path)
{
    if (context != nullptr)
    {
        std::shared_lock sg1(context->_m_thread_notifier->_m_cached_resources_mx);

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
template<jeecs::graphic::requirements::basic_graphic_resource T>
T* _jegl_try_update_shared_resource(jegl_context* context, T* resource)
{
    if (context != nullptr)
    {
        const char* resource_path = resource->m_handle.m_path_may_null_if_builtin;
        assert(resource != nullptr && resource_path != nullptr);

        std::lock_guard g1(context->_m_thread_notifier->_m_cached_resources_mx);

        jegl_context_notifier::cached_resource_statement caching_statement(resource);
        auto&& [cached_resource_pair, insert_succ] =
            context->_m_thread_notifier->_m_cached_resources.insert(
                std::make_pair(resource_path, caching_statement));

        auto& cached_resource = cached_resource_pair.second;
        if (insert_succ)
        {
            jegl_share_resource_handle(caching_statement.m_resource.get_handle());
            return resource;
        }
        else if (cached_resource.m_resource.m_type == caching_statement.m_resource.m_type)
        {
            // 就在刚刚创建的瞬间，已经有一个相同路径的资源存在了，放弃新创建的
            caching_statement.m_resource.drop_resource();

            jegl_share_resource_handle(cached_resource.m_resource.get_handle());
            return cached_resource.m_resource.resource<T>();
        }
        else
        {
            // WTF?
            jeecs::debug::logwarn(
                "Different type of resource with same path detected in shared resource cache: `%s`.",
                resource_path);
            return resource;
        }
    }
    return resource;
}
template<jeecs::graphic::requirements::basic_graphic_resource T>
jeecs::graphic::jegl_resouce_state _jegl_check_resource_state(
    T* resource)
{
    auto* resource_handle = &resource->m_handle;

    if (!jeecs::graphic::_current_graphic_thread)
    {
        jeecs::debug::logerr("Graphic resource only usable in graphic thread.");
        return jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT;
    }

    if (resource_handle->m_graphic_thread != nullptr
        && jeecs::graphic::_current_graphic_thread != resource_handle->m_graphic_thread)
    {
        jeecs::debug::logerr("This resource has been used in graphic thread: %p.",
            resource_handle->m_graphic_thread);
        return jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT;
    }
    else if (nullptr == resource_handle->m_graphic_thread
        || resource_handle->m_graphic_thread_version != jeecs::graphic::_current_graphic_thread->m_version)
    {
        resource_handle->m_modified = false;
        resource_handle->m_graphic_thread = jeecs::graphic::_current_graphic_thread;
        resource_handle->m_graphic_thread_version = jeecs::graphic::_current_graphic_thread->m_version;

        auto result =
            resource_handle->m_graphic_thread->_m_thread_notifier->_m_created_resources.insert(
                resource);

        (void)result;
        assert(result.second); // New resource must be inserted.

        return jeecs::graphic::jegl_resouce_state::NEED_INIT;
    }
    else if (resource_handle->m_modified)
    {
        resource_handle->m_modified = false;
        return jeecs::graphic::jegl_resouce_state::NEED_UPDATE;
    }

    return jeecs::graphic::jegl_resouce_state::READY;
}

void _jegl_free_resource_instance(
    jeecs::graphic::resource_to_destroy* /* created by `new` */ del_res)
{
    assert(del_res != nullptr);

    auto* resource_handle = del_res->m_resource.get_handle();
    assert(resource_handle->m_raw_ref_count == nullptr);

    // Send this resource to destroing list;

    std::shared_lock sg1(jeecs::graphic::_jegl_alive_glthread_list_mx);
    auto fnd = std::find(
        jeecs::graphic::_jegl_alive_glthread_list.begin(),
        jeecs::graphic::_jegl_alive_glthread_list.end(),
        resource_handle->m_graphic_thread);

    if (fnd != jeecs::graphic::_jegl_alive_glthread_list.end())
        (*fnd)->_m_thread_notifier->_m_closing_resources.add_one(del_res);
    else
    {
        // 既然这个资源已经没有管理线程了，直接就地杀了埋了
        if (resource_handle->m_graphic_thread != nullptr)
            jeecs::debug::logwarn("Resource %p cannot free by correct graphic context, maybe it is out-dated? Free it!",
                resource_handle);

        del_res->m_resource.free_resouce_body();
        delete del_res;
    }
}

void _jegl_free_vertex_bone_data(const jegl_vertex::bone_data* b)
{
    je_mem_free(const_cast<char*>(b->m_name));
    free(const_cast<jegl_vertex::bone_data*>(b));
}

bool _jegl_close_resource_handle(jegl_resource_handle* resource_handle)
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

void _jegl_init_resource_handle(
    jegl_resource_handle* resource_handle,
    const char* path_may_null_if_builtin)
{
    if (path_may_null_if_builtin != nullptr)
    {
        resource_handle->m_path_may_null_if_builtin =
            jeecs::basic::make_new_string(path_may_null_if_builtin);
    }
    else
        resource_handle->m_path_may_null_if_builtin = nullptr;

    resource_handle->m_raw_ref_count = new jegl_resource_bind_counter(1);

    resource_handle->m_graphic_thread = nullptr;
    resource_handle->m_graphic_thread_version = 0;
    resource_handle->m_modified = false;
    resource_handle->m_ptr = nullptr;
}

void jegl_close_shader(jegl_shader* shader)
{
    if (_jegl_close_resource_handle(&shader->m_handle))
    {
        jegl_shader_free_generated_shader_source(shader);
        _jegl_free_resource_instance(new jeecs::graphic::resource_to_destroy(shader));
    }
}

void jegl_close_texture(jegl_texture* texture)
{
    if (_jegl_close_resource_handle(&texture->m_handle))
    {
        // close resource's raw data, then send this resource to closing-queue
        if (texture->m_pixels)
        {
            assert(0 == (texture->m_format & jegl_texture::format::FORMAT_MASK));
            stbi_image_free(texture->m_pixels);
        }
        else
            assert(0 != (texture->m_format & jegl_texture::format::FORMAT_MASK));
        _jegl_free_resource_instance(new jeecs::graphic::resource_to_destroy(texture));
    }
}

void jegl_close_vertex(jegl_vertex* vertex)
{
    if (_jegl_close_resource_handle(&vertex->m_handle))
    {
        // close resource's raw data, then send this resource to closing-queue
        free(const_cast<void*>(vertex->m_vertexs));
        free(const_cast<uint32_t*>(vertex->m_indices));
        free(const_cast<jegl_vertex::data_layout*>(vertex->m_formats));

        for (size_t bone_idx = 0; bone_idx < vertex->m_bone_count; ++bone_idx)
            _jegl_free_vertex_bone_data(vertex->m_bones[bone_idx]);

        free(const_cast<jegl_vertex::bone_data**>(vertex->m_bones));
        _jegl_free_resource_instance(new jeecs::graphic::resource_to_destroy(vertex));
    }
}

void jegl_close_framebuf(jegl_frame_buffer* framebuf)
{
    if (_jegl_close_resource_handle(&framebuf->m_handle))
    {
        // Close all frame rend out texture
        assert(framebuf->m_attachment_count > 0);
        jeecs::basic::resource<jeecs::graphic::texture>* attachments =
            reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
                framebuf->m_output_attachments);

        for (size_t i = 0; i < framebuf->m_attachment_count; ++i)
            attachments[i].~shared_pointer();

        free(attachments);
        _jegl_free_resource_instance(new jeecs::graphic::resource_to_destroy(framebuf));
    }
}

void jegl_close_uniformbuf(jegl_uniform_buffer* ubuffer)
{
    if (_jegl_close_resource_handle(&ubuffer->m_handle))
    {
        free(ubuffer->m_buffer);
        _jegl_free_resource_instance(new jeecs::graphic::resource_to_destroy(ubuffer));
    }
}

jegl_texture* jegl_create_texture(size_t width, size_t height, jegl_texture::format format)
{
    jegl_texture* texture_instance =
        reinterpret_cast<jegl_texture*>(malloc(sizeof(jegl_texture)));

    assert(texture_instance != nullptr);

    if (width == 0)
        width = 1;
    if (height == 0)
        height = 1;

    if ((format & jegl_texture::format::FORMAT_MASK) == 0)
    {
        texture_instance->m_pixels =
            (jegl_texture::pixel_data_t*)malloc(width * height * format);

        assert(texture_instance->m_pixels != nullptr);
        memset(texture_instance->m_pixels, 0, width * height * format);
    }
    else
        // For special format texture such as depth, do not alloc for pixel.
        texture_instance->m_pixels = nullptr;

    texture_instance->m_width = width;
    texture_instance->m_height = height;
    texture_instance->m_format = format;

    _jegl_init_resource_handle(&texture_instance->m_handle, nullptr);
    return texture_instance;
}

jegl_shader* _jegl_create_shader_instance_and_init(void);
jegl_shader* _jegl_load_shader_cache(jeecs_file* cache_file, const char* path);
void _jegl_create_shader_cache(jegl_shader* shader_resource, wo_integer_t virtual_file_crc64);

jegl_shader* _jegl_load_shader_source_impl(
    const char* path, const char* src, bool is_virtual_file)
{
    if (is_virtual_file)
    {
        if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, wo_crc64_str(src)))
            return _jegl_load_shader_cache(shader_cache, path);
    }

#if JE4_ENABLE_SHADER_WRAP_GENERATOR
    wo_vm vmm = wo_create_vm();
    if (!wo_load_source(vmm, path, src))
    {
        // Compile error
        jeecs::debug::logerr("Fail to load shader: %s.\n%s", path, wo_get_compile_error(vmm, WO_DEFAULT));
        wo_close_vm(vmm);
        return nullptr;
    }

    wo_run(vmm);

    wo_unref_value generate_shader_func;

    if (!wo_extern_symb(&generate_shader_func, vmm, "je::shader::generate_shader"))
    {
        jeecs::debug::logerr("Fail to load shader: %s. you should import je::shader.", path);
        wo_close_vm(vmm);
        return nullptr;
    }

    if (wo_value retval = wo_invoke_value(vmm, &generate_shader_func, 0, nullptr, nullptr))
    {
        shader_wrapper* shader_graph = reinterpret_cast<shader_wrapper*>(wo_pointer(retval));

        jegl_shader* shader_instance =
            _jegl_create_shader_instance_and_init();

        jegl_shader_generate_shader_source(shader_graph, shader_instance);

        _jegl_init_resource_handle(&shader_instance->m_handle, path);
        _jegl_create_shader_cache(shader_instance, is_virtual_file ? wo_crc64_str(src) : 0);

        wo_close_vm(vmm);

        return shader_instance;
    }
    else
    {
        jeecs::debug::logerr("Fail to load shader: %s: %s.", path, wo_get_runtime_error(vmm));
        wo_close_vm(vmm);
        return nullptr;
    }
#else
    jeecs::debug::logerr("Shader generator has been disabled.");
    return nullptr;
#endif
}

jegl_shader* jegl_load_shader_source(
    jegl_context* context, const char* path, const char* src, bool is_virtual_file)
{
    if (is_virtual_file)
    {
        auto* shared_shader =
            _jegl_try_load_shared_resource<jegl_shader>(
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

jegl_shader* jegl_load_shader(jegl_context* context, const char* path)
{
    auto* shared_shader =
        _jegl_try_load_shared_resource<jegl_shader>(
            context, path);

    if (shared_shader != nullptr)
        return shared_shader;

    if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, 0))
    {
        auto* cached_shader_instance = _jegl_load_shader_cache(shader_cache, path);
        assert(cached_shader_instance != nullptr);

        if (cached_shader_instance->m_enable_to_shared)
            return _jegl_try_update_shared_resource(
                context, cached_shader_instance);

        return cached_shader_instance;
    }
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        jeecs_file_close(texfile);

        auto* shader_resource = jegl_load_shader_source(context, path, src, false);
        free(src);

        return shader_resource;
    }
    jeecs::debug::logerr("Fail to open file: '%s'", path);
    return nullptr;
}

void jegl_share_resource_handle(jegl_resource_handle* resource_handle)
{
    ++resource_handle->m_raw_ref_count->m_binding_count;
}

jegl_texture* jegl_load_texture(jegl_context* context, const char* path)
{
    auto* shared_texture =
        _jegl_try_load_shared_resource<jegl_texture>(
            context, path);

    if (shared_texture != nullptr)
        return shared_texture;

    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        jegl_texture* texture_instance =
            reinterpret_cast<jegl_texture*>(malloc(sizeof(jegl_texture)));

        int w, h, cdepth;

        stbi_set_flip_vertically_on_load(true);

        stbi_io_callbacks res_reader;
        res_reader.read = [](void* f, char* data, int size)
            { return (int)jeecs_file_read(data, 1, (size_t)size, (jeecs_file*)f); };
        res_reader.eof = [](void* f)
            { return jeecs_file_tell((jeecs_file*)f) >= ((jeecs_file*)f)->m_file_length ? 1 : 0; };
        res_reader.skip = [](void* f, int n)
            { jeecs_file_seek((jeecs_file*)f, (size_t)n, je_read_file_seek_mode::JE_READ_FILE_SEEK_CURRENT); };

        texture_instance->m_pixels = stbi_load_from_callbacks(
            &res_reader, texfile, &w, &h, &cdepth, STBI_rgb_alpha);

        jeecs_file_close(texfile);

        if (texture_instance->m_pixels == nullptr)
        {
            jeecs::debug::logerr("Cannot load texture: Invalid image format of file: '%s'", path);
            free(texture_instance);
            return nullptr;
        }

        texture_instance->m_width = (size_t)w;
        texture_instance->m_height = (size_t)h;
        texture_instance->m_format = jegl_texture::RGBA;

        _jegl_init_resource_handle(&texture_instance->m_handle, path);

        return _jegl_try_update_shared_resource(
            context, texture_instance);
    }

    jeecs::debug::loginfo("Cannot load texture: Failed to open file: '%s'", path);
    return nullptr;
}

jegl_vertex* _jegl_create_vertex_impl(
    jegl_vertex::type type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const jegl_vertex::data_layout* format,
    size_t format_count)
{
    size_t data_size_per_point = 0;
    for (size_t i = 0; i < format_count; ++i)
        data_size_per_point += format[i].m_count * 4; /* Both int32 & float32 is 4 byte*/

    auto data_group_count = data_length / data_size_per_point;

    if (data_size_per_point == 0)
    {
        jeecs::debug::logerr("Bad format, point donot contain any data.");
        return nullptr;
    }
    if (data_group_count == 0)
    {
        jeecs::debug::logerr("Vertex data donot contain any completed point.");
        return nullptr;
    }

    if (data_length % data_size_per_point)
        jeecs::debug::logwarn("Vertex data & format not matched, please check.");

    assert(format_count > 0);

    jegl_vertex* vertex = reinterpret_cast<jegl_vertex*>(malloc(sizeof(jegl_vertex)));

    vertex->m_type = type;
    vertex->m_data_size_per_point = data_size_per_point;
    vertex->m_bones = nullptr;
    vertex->m_bone_count = 0;

    void* data_buffer = (void*)malloc(data_length);
    memcpy(data_buffer, datas, data_length);
    vertex->m_vertexs = data_buffer;
    vertex->m_vertex_length = data_length;

    uint32_t* index_buffer = (uint32_t*)malloc(index_count * sizeof(uint32_t));
    memcpy(index_buffer, indices, index_count * sizeof(uint32_t));
    vertex->m_indices = index_buffer;
    vertex->m_index_count = index_count;

    jegl_vertex::data_layout* formats =
        (jegl_vertex::data_layout*)malloc(format_count * sizeof(jegl_vertex::data_layout));
    memcpy(formats, format,
        format_count * sizeof(jegl_vertex::data_layout));
    vertex->m_formats = formats;
    vertex->m_format_count = format_count;

    // Calc size by default:
    float
        x_min = 0.f,
        x_max = 0.f,
        y_min = 0.f, y_max = 0.f,
        z_min = 0.f, z_max = 0.f;

    if (format[0].m_count == 3 && format[0].m_type == jegl_vertex::data_type::FLOAT32)
    {
        // First data group is position(by default).
        x_min = x_max = reinterpret_cast<const float*>(datas)[0];
        y_min = y_max = reinterpret_cast<const float*>(datas)[1];
        z_min = z_max = reinterpret_cast<const float*>(datas)[2];

        // First data group is position(by default).
        for (size_t i = 1; i < data_group_count; ++i)
        {
            const float* fdata = reinterpret_cast<const float*>(
                reinterpret_cast<const char*>(datas) + i * data_size_per_point);

            float x = fdata[0];
            float y = fdata[1];
            float z = fdata[2];

            x_min = std::min(x, x_min);
            x_max = std::max(x, x_max);
            y_min = std::min(y, y_min);
            y_max = std::max(y, y_max);
            z_min = std::min(z, z_min);
            z_max = std::max(z, z_max);
        }
    }
    else
        jeecs::debug::logwarn(
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

jegl_vertex* jegl_load_vertex(jegl_context* context, const char* path)
{
    const size_t MAX_BONE_COUNT = 256;

    auto* shared_vertex =
        _jegl_try_load_shared_resource<jegl_vertex>(
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

    std::stack<aiNode*> m_node_stack;
    m_node_stack.push(scene->mRootNode);

    struct jegl_standard_model_vertex_t
    {
        jeecs::math::vec3 m_vertex;
        jeecs::math::vec2 m_texcoord;
        jeecs::math::vec3 m_normal;
        jeecs::math::vec3 m_tangent;

        int32_t m_bone_id[4];
        float m_bone_weight[4];
    };
    static_assert(sizeof(jegl_standard_model_vertex_t) == 76);

    const jegl_vertex::data_layout jegl_standard_model_vertex_format[] = {
        {jegl_vertex::data_type::FLOAT32, 3},
        {jegl_vertex::data_type::FLOAT32, 2},
        {jegl_vertex::data_type::FLOAT32, 3},
        {jegl_vertex::data_type::FLOAT32, 3},
        {jegl_vertex::data_type::INT32, 4},
        {jegl_vertex::data_type::FLOAT32, 4},
    };

    std::vector<jegl_standard_model_vertex_t> vertex_datas;
    std::vector<uint32_t> index_datas;
    std::vector<const jegl_vertex::bone_data*> bones;

    while (!m_node_stack.empty())
    {
        auto* current_node = m_node_stack.top();
        m_node_stack.pop();

        for (size_t i = 0; i < current_node->mNumChildren; ++i)
            m_node_stack.push(current_node->mChildren[i]);

        size_t bone_counter = 0;

        for (size_t i = 0; i < current_node->mNumMeshes; ++i)
        {
            auto* mesh = scene->mMeshes[current_node->mMeshes[i]];
            const size_t current_model_vertex_offset = vertex_datas.size();

            const bool has_texcrood_0 = mesh->HasTextureCoords(0);
            const bool has_normals = mesh->HasNormals();

            for (size_t index = 0; index < mesh->mNumVertices; ++index)
            {
                auto& vertex = mesh->mVertices[index];
                auto& normal = mesh->mNormals[index];

                jegl_standard_model_vertex_t model_vertex = {};
                model_vertex.m_vertex = jeecs::math::vec3(
                    vertex.x,
                    vertex.y,
                    vertex.z);

                if (has_texcrood_0)
                {
                    model_vertex.m_texcoord = jeecs::math::vec2(
                        mesh->mTextureCoords[0][index].x,
                        mesh->mTextureCoords[0][index].y);
                }
                if (has_normals)
                {
                    model_vertex.m_normal = jeecs::math::vec3(
                        normal.x,
                        normal.y,
                        normal.z);
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
                const jeecs::math::vec3 tangent =
                    (jeecs::math::vec3(
                        duv20.y * edge10.x - duv10.y * edge20.x,
                        duv20.y * edge10.y - duv10.y * edge20.y,
                        duv20.y * edge10.z - duv10.y * edge20.z) * normalize_factor).unit();

                vertex_datas[p0idx].m_tangent += tangent;
                vertex_datas[p1idx].m_tangent += tangent;
                vertex_datas[p2idx].m_tangent += tangent;
            }
            for (size_t bone_idx = 0; bone_idx < mesh->mNumBones; ++bone_idx)
            {
                jegl_vertex::bone_data* bone_data =
                    (jegl_vertex::bone_data*)malloc(sizeof(jegl_vertex::bone_data));

                assert(bone_data != nullptr);
                bone_data->m_name = jeecs::basic::make_new_string(mesh->mBones[bone_idx]->mName.C_Str());
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
                size_t bone_id = bone_data->m_index;

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
        jegl_vertex::type::TRIANGLES,
        vertex_datas.data(),
        vertex_datas.size() * sizeof(jegl_standard_model_vertex_t),
        index_datas.data(),
        index_datas.size(),
        jegl_standard_model_vertex_format,
        sizeof(jegl_standard_model_vertex_format) / sizeof(jegl_vertex::data_layout));

    if (vertex != nullptr)
    {
        const jegl_vertex::bone_data** cbones =
            (const jegl_vertex::bone_data**)malloc(
                bones.size() * sizeof(const jegl_vertex::bone_data*));

        memcpy(
            cbones,
            bones.data(),
            bones.size() * sizeof(const jegl_vertex::bone_data*));

        vertex->m_bones = cbones;
        vertex->m_bone_count = bones.size();

        _jegl_init_resource_handle(&vertex->m_handle, path);

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


jegl_vertex* jegl_create_vertex(
    jegl_vertex::type type,
    const void* datas,
    size_t data_length,
    const uint32_t* indices,
    size_t index_count,
    const jegl_vertex::data_layout* format,
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
        _jegl_init_resource_handle(&vertex->m_handle, nullptr);

    return vertex;
}

jegl_frame_buffer* jegl_create_framebuf(
    size_t width,
    size_t height,
    const jegl_texture::format* color_attachment_formats,
    size_t color_attachment_count,
    bool contain_depth_attachment)
{
    if (width == 0 || height == 0 || color_attachment_count == 0)
    {
        jeecs::debug::logerr("Failed to create invalid framebuffer: size is zero or no attachment.");
        return nullptr;
    }

    jegl_frame_buffer* framebuf =
        reinterpret_cast<jegl_frame_buffer*>(
            malloc(sizeof(jegl_frame_buffer)));

    framebuf->m_attachment_count =
        color_attachment_count + (contain_depth_attachment ? 1 : 0);
    framebuf->m_width = width;
    framebuf->m_height = height;

    jeecs::basic::resource<jeecs::graphic::texture>* attachments =
        reinterpret_cast<jeecs::basic::resource<jeecs::graphic::texture> *>(
            malloc(
                framebuf->m_attachment_count
                * sizeof(jeecs::basic::resource<jeecs::graphic::texture>)));

    for (size_t i = 0; i < color_attachment_count; ++i)
        new (&attachments[i]) jeecs::basic::resource<jeecs::graphic::texture>(
            jeecs::graphic::texture::create(
                width,
                height,
                jegl_texture::format(
                    color_attachment_formats[i]
                    | jegl_texture::format::FRAMEBUF)));

    if (contain_depth_attachment)
        new (&attachments[color_attachment_count]) jeecs::basic::resource<jeecs::graphic::texture>(
            jeecs::graphic::texture::create(
                width,
                height,
                jegl_texture::format(
                    jegl_texture::format::DEPTH
                    | jegl_texture::format::FRAMEBUF)));

    framebuf->m_output_attachments =
        (jegl_frame_buffer::attachment_t*)attachments;

    _jegl_init_resource_handle(&framebuf->m_handle, nullptr);
    return framebuf;
}

jegl_uniform_buffer* jegl_create_uniformbuf(
    size_t binding_place,
    size_t length)
{
    jegl_uniform_buffer* uniformbuf =
        reinterpret_cast<jegl_uniform_buffer*>(
            malloc(sizeof(jegl_uniform_buffer)));

    uniformbuf->m_buffer = (uint8_t*)malloc(length);
    uniformbuf->m_buffer_size = length;
    uniformbuf->m_buffer_binding_place = binding_place;

    uniformbuf->m_update_begin_offset = 0;
    uniformbuf->m_update_length = 0;

    _jegl_init_resource_handle(&uniformbuf->m_handle, nullptr);
    return uniformbuf;
}

void jegl_update_uniformbuf(
    jegl_uniform_buffer* uniformbuf,
    const void* buf,
    size_t update_offset,
    size_t update_length)
{
    if (update_length != 0)
    {
        memcpy(uniformbuf->m_buffer + update_offset, buf, update_length);
        if (!uniformbuf->m_handle.m_modified)
        {
            uniformbuf->m_handle.m_modified = true;

            uniformbuf->m_update_begin_offset = update_offset;
            uniformbuf->m_update_length = update_length;
        }
        else
        {
            assert(uniformbuf->m_update_length != 0);

            size_t new_begin = std::min(uniformbuf->m_update_begin_offset, update_offset);
            size_t new_end = std::max(
                uniformbuf->m_update_begin_offset + uniformbuf->m_update_length,
                update_offset + update_length);

            uniformbuf->m_update_begin_offset = new_begin;
            uniformbuf->m_update_length = new_end - new_begin;
        }
    }
}

bool jegl_bind_shader(jegl_shader* shader)
{
    auto* thread_handle = jeecs::graphic::_current_graphic_thread;
    const auto* gapi = thread_handle->m_apis;

    bool updated = false, need_init = false;
    switch (_jegl_check_resource_state(shader))
    {
    case jeecs::graphic::jegl_resouce_state::READY:
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_UPDATE:
        updated = true;
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_INIT:
    {
        updated = need_init = true;

        jegl_resource_blob shader_blob;

        auto found_blob_kind = _jegl_try_get_resource_blob(&shader->m_handle, &shader_blob);
        if (found_blob_kind == jeecs::graphic::cached_resource_blob::kind::SHADER)
        {
            gapi->shader_init(
                thread_handle->m_graphic_impl_context,
                shader_blob,
                shader);
        }
        else
        {
            // Failed to get blob, need create it first.
            shader_blob = gapi->shader_create_blob(
                thread_handle->m_graphic_impl_context,
                shader);

            gapi->shader_init(
                thread_handle->m_graphic_impl_context,
                shader_blob,
                shader);

            if (found_blob_kind != jeecs::graphic::cached_resource_blob::kind::UNKNOWN
                || !_jegl_try_update_resource_blob(
                    &shader->m_handle,
                    jeecs::graphic::cached_resource_blob::kind::SHADER,
                    shader_blob))
            {
                // Failed to update blob, need free it.
                gapi->shader_close_blob(
                    thread_handle->m_graphic_impl_context,
                    shader_blob);
            }
        }
        break;
    }
    case jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT:
    default:
        return false;
    }

    if (!gapi->bind_shader(
        thread_handle->m_graphic_impl_context, shader))
        // Failed to bind shader, bad shader?
        return false;

    if (updated)
    {
        auto uniform_vars = shader->m_custom_uniforms;
        while (uniform_vars)
        {
            if (uniform_vars->m_index != jeecs::graphic::INVALID_UNIFORM_LOCATION)
            {
                if (uniform_vars->m_updated || need_init)
                {
                    assert(uniform_vars->m_uniform_type != jegl_shader::uniform_type::TEXTURE);

                    uniform_vars->m_updated = false;

                    jegl_set_uniform_value(
                        uniform_vars->m_index,
                        uniform_vars->m_uniform_type,
                        &uniform_vars->m_value);
                }
            }
            uniform_vars = uniform_vars->m_next;
        }
    }
    return true;
}

void jegl_bind_uniform_buffer(jegl_uniform_buffer* uniformbuf)
{
    auto* thread_handle = jeecs::graphic::_current_graphic_thread;
    const auto* gapi = thread_handle->m_apis;

    switch (_jegl_check_resource_state(uniformbuf))
    {
    case jeecs::graphic::jegl_resouce_state::READY:
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_UPDATE:
        gapi->ubuffer_update(
            thread_handle->m_graphic_impl_context,
            uniformbuf);
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_INIT:
        gapi->ubuffer_init(
            thread_handle->m_graphic_impl_context,
            uniformbuf);
        break;
    case jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT:
    default:
        // Donot bind invalid resource.
        return;
    }
    gapi->bind_uniform_buffer(
        thread_handle->m_graphic_impl_context, uniformbuf);
}

void jegl_draw_vertex(jegl_vertex* vert)
{
    auto* thread_handle = jeecs::graphic::_current_graphic_thread;
    const auto* gapi = thread_handle->m_apis;

    switch (_jegl_check_resource_state(vert))
    {
    case jeecs::graphic::jegl_resouce_state::READY:
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_UPDATE:
        gapi->vertex_update(
            thread_handle->m_graphic_impl_context,
            vert);
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_INIT:
    {
        jegl_resource_blob vertex_blob;

        auto found_blob_kind = _jegl_try_get_resource_blob(&vert->m_handle, &vertex_blob);
        if (found_blob_kind == jeecs::graphic::cached_resource_blob::kind::VERTEX)
        {
            gapi->vertex_init(
                thread_handle->m_graphic_impl_context,
                vertex_blob,
                vert);
        }
        else
        {
            // Failed to get blob, need create it first.
            vertex_blob = gapi->vertex_create_blob(
                thread_handle->m_graphic_impl_context,
                vert);

            gapi->vertex_init(
                thread_handle->m_graphic_impl_context,
                vertex_blob,
                vert);

            // Count the taken GPU memory size.
            thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size
                += vert->m_vertex_length * 4 /* FLOAT32 OR INT32 */
                + vert->m_index_count * sizeof(uint32_t);

            if (found_blob_kind != jeecs::graphic::cached_resource_blob::kind::UNKNOWN
                || !_jegl_try_update_resource_blob(
                    &vert->m_handle,
                    jeecs::graphic::cached_resource_blob::kind::VERTEX,
                    vertex_blob))
            {
                // Failed to update blob, need free it.
                gapi->vertex_close_blob(
                    thread_handle->m_graphic_impl_context,
                    vertex_blob);
            }
        }
        break;
    }
    case jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT:
    default:
        // Donot bind invalid resource.
        return;
    }
    gapi->draw_vertex(
        thread_handle->m_graphic_impl_context, vert);
}

void jegl_rend_to_framebuffer(
    jegl_frame_buffer* framebuffer,
    const int32_t(*viewport_xywh)[4],
    const jegl_frame_buffer_clear_operation* clear_operations)
{
    auto* thread_handle = jeecs::graphic::_current_graphic_thread;
    const auto* gapi = thread_handle->m_apis;

    if (framebuffer != nullptr)
    {
        switch (_jegl_check_resource_state(framebuffer))
        {
        case jeecs::graphic::jegl_resouce_state::READY:
            break;
        case jeecs::graphic::jegl_resouce_state::NEED_UPDATE:
            gapi->framebuffer_update(
                thread_handle->m_graphic_impl_context,
                framebuffer);
            break;
        case jeecs::graphic::jegl_resouce_state::NEED_INIT:
            gapi->framebuffer_init(
                thread_handle->m_graphic_impl_context,
                framebuffer);
            break;
        case jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT:
        default:
            // Donot bind invalid resource.
            return;
        }
    }
    gapi->bind_framebuf(
        thread_handle->m_graphic_impl_context,
        framebuffer,
        viewport_xywh,
        clear_operations);
}

void jegl_bind_texture(jegl_texture* texture, size_t pass)
{
    auto* thread_handle = jeecs::graphic::_current_graphic_thread;
    const auto* gapi = thread_handle->m_apis;

    switch (_jegl_check_resource_state(texture))
    {
    case jeecs::graphic::jegl_resouce_state::READY:
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_UPDATE:
        gapi->texture_update(
            thread_handle->m_graphic_impl_context,
            texture);
        break;
    case jeecs::graphic::jegl_resouce_state::NEED_INIT:
    {
        jegl_resource_blob texture_blob;

        auto found_blob_kind = _jegl_try_get_resource_blob(&texture->m_handle, &texture_blob);
        if (found_blob_kind == jeecs::graphic::cached_resource_blob::kind::TEXTURE)
        {
            gapi->texture_init(
                thread_handle->m_graphic_impl_context,
                texture_blob,
                texture);
        }
        else
        {
            // Failed to get blob, need create it first.
            texture_blob = gapi->texture_create_blob(
                thread_handle->m_graphic_impl_context,
                texture);

            gapi->texture_init(
                thread_handle->m_graphic_impl_context,
                texture_blob,
                texture);

            // Count the taken GPU memory size.
            thread_handle->_m_thread_notifier->_m_increased_gpu_memory_size
                += texture->m_width * texture->m_height * (texture->m_format & jegl_texture::format::COLOR_DEPTH_MASK);

            if (found_blob_kind != jeecs::graphic::cached_resource_blob::kind::UNKNOWN
                || !_jegl_try_update_resource_blob(
                    &texture->m_handle,
                    jeecs::graphic::cached_resource_blob::kind::TEXTURE,
                    texture_blob))
            {
                // Failed to update blob, need free it.
                gapi->vertex_close_blob(
                    thread_handle->m_graphic_impl_context,
                    texture_blob);
            }
        }
        break;
    }
    case jeecs::graphic::jegl_resouce_state::INVALID_CONTEXT:
    default:
        // Donot bind invalid resource.
        return;
    }
    gapi->bind_texture(
        thread_handle->m_graphic_impl_context,
        texture,
        pass);
}

void jegl_uniform_int(uint32_t location, int value)
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::INT, &value);
}

void jegl_uniform_int2(uint32_t location, int x, int y)
{
    int value[2] = { x, y };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::INT2, &value);
}

void jegl_uniform_int3(uint32_t location, int x, int y, int z)
{
    int value[3] = { x, y, z };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::INT3, &value);
}

void jegl_uniform_int4(uint32_t location, int x, int y, int z, int w)
{
    int value[4] = { x, y, z, w };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::INT4, &value);
}

void jegl_uniform_float(uint32_t location, float value)
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT, &value);
}

void jegl_uniform_float2(uint32_t location, float x, float y)
{
    float value[] = { x, y };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT2, &value);
}

void jegl_uniform_float3(uint32_t location, float x, float y, float z)
{
    float value[] = { x, y, z };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT3, &value);
}

void jegl_uniform_float4(uint32_t location, float x, float y, float z, float w)
{
    float value[] = { x, y, z, w };
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT4, &value);
}

void jegl_uniform_float2x2(uint32_t location, const float (*mat)[2])
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT2X2, mat);
}

void jegl_uniform_float3x3(uint32_t location, const float (*mat)[3])
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT3X3, mat);
}

void jegl_uniform_float4x4(uint32_t location, const float (*mat)[4])
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, jegl_shader::FLOAT4X4, mat);
}

void jegl_set_uniform_value(
    uint32_t location, jegl_shader::uniform_type type, const void* data)
{
    jeecs::graphic::_current_graphic_thread->m_apis->set_uniform(
        jeecs::graphic::_current_graphic_thread->m_graphic_impl_context, location, type, data);
}
