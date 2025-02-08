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

        virtual size_t Read(void* pvBuffer,
            size_t pSize,
            size_t pCount)override
        {
            return jeecs_file_read(pvBuffer, pSize, pCount, m_file);
        }

        virtual size_t Write(const void* pvBuffer,
            size_t pSize,
            size_t pCount)override
        {
            // NOT IMPL!
            abort();
        }

        virtual aiReturn Seek(size_t pOffset,
            aiOrigin pOrigin)
        {
            static_assert(aiOrigin::aiOrigin_CUR == je_read_file_seek_mode::JE_READ_FILE_CURRENT);
            static_assert(aiOrigin::aiOrigin_END == je_read_file_seek_mode::JE_READ_FILE_END);
            static_assert(aiOrigin::aiOrigin_SET == je_read_file_seek_mode::JE_READ_FILE_SET);

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
        virtual void Flush()
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

        bool Exists(const char* pFile) const override {
            auto* f = jeecs_file_open(pFile);
            if (f == nullptr)
                return false;

            jeecs_file_close(f);
            return true;
        }

        char getOsSeparator() const override {
            return '/';  // why not? it doesn't care
        }

        IOStream* Open(const char* pFile, const char* pMode = "rb") override {
            auto* f = jeecs_file_open(pFile);
            if (f == nullptr)
                return nullptr;

            return new je_file_io_stream(f);
        }

        void Close(IOStream* pFile) override {
            delete pFile;
        }

        bool ComparePaths(const char* one, const char* second) const override {
            return strcmp(one, second) == 0;
        }

        bool PushDirectory(const std::string& path) override {
            return false;
        }

        const std::string& CurrentDirectory() const override {
            return m_current_dir;
        }

        size_t StackSize() const override {
            return 0;
        }

        bool PopDirectory() override {
            return false;
        }

        bool CreateDirectory(const std::string& path) override {
            return false;
        }

        bool ChangeDirectory(const std::string& path) override {
            return false;
        }

        bool DeleteFile(const std::string& file) override {
            return false;
        }
    };
}

#if !JE4_STB_IMAGE_STATIC_IMPLED
#   define STB_IMAGE_IMPLEMENTATION
#endif
#include <stb_image.h>

#include <condition_variable>
#include <list>
#include <future>
#include <stack>
#include <queue>

struct _jegl_destroy_resouce
{
    jegl_resource* m_destroy_resource;
    _jegl_destroy_resouce* last;
};

struct jegl_context_notifier
{
    std::atomic_flag m_graphic_terminate_flag;

    std::mutex       m_update_mx;
    std::atomic_bool m_update_flag;
    std::condition_variable m_update_waiter;

    std::atomic_bool m_reboot_flag;

    // Aliving resource.
    std::unordered_set<jegl_resource*> _m_created_resources;

    // Created blobs.
    struct cached_resource_blob
    {
        size_t              m_version;
        jegl_resource_blob  m_blob;
    };

    struct cached_resource
    {
        jegl_resource* m_resource;
    };

    // NOTE: _m_cached_resources 不需要单独上锁，因为对此的操作都将在一个更大的全
    //      局锁(参见_je_graphic_shared_context_instance)的保护下
    std::unordered_map<std::string, cached_resource> _m_cached_resources;

    // NOTE: _m_cached_resource_blobs 不需要上锁，因为对此的操作都被局限在图形
    //      线程内部，不会发生并发访问。
    std::unordered_map<std::string, cached_resource_blob> _m_cached_resource_blobs;

    jeecs::basic::atomic_list<_jegl_destroy_resouce> _m_closing_resources;
};

thread_local jegl_context* _current_graphic_thread = nullptr;

struct shared_resource_instance
{
    jegl_resource* const m_resource;

    JECS_DISABLE_MOVE_AND_COPY(shared_resource_instance);
    shared_resource_instance(jegl_resource* res)
        : m_resource(res)
    {
        assert(m_resource != nullptr);
    }
    ~shared_resource_instance()
    {
        jegl_close_resource(m_resource);
    }
};

struct _je_graphic_shared_context
{
    // NOTE: Resource blob 受限于机制，本体是缓存在图形上下文中的，此处用于记录blob
    // 的unload-count-version，用于通知图形线程何时应当释放blob资源。
    std::shared_mutex share_smx;

    std::unordered_map<std::string, size_t> shared_unload_counts;
    std::unordered_map<std::string, size_t> shared_resource_used_counts;

    size_t _get_shared_blob_unload_counter(const std::string& path)
    {
        auto fnd = shared_unload_counts.find(path);
        if (fnd == shared_unload_counts.end())
            return 0;
        return fnd->second;
    }
    void _unload_share(const std::string& path)
    {
        ++shared_unload_counts[path];
    }

    bool _obsolete_shared_resource_cache_nolock(const char* path, bool free_blob);
    bool mark_shared_resources_outdated(const char* path);

    static jegl_resource* _share_resource(jegl_resource* resource)
    {
        assert(resource != nullptr);
        if (resource->m_raw_ref_count == nullptr)
            resource->m_raw_ref_count = reinterpret_cast<void*>(new std::atomic_uint32_t(1));

        std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count))->fetch_add(1);

        return resource;
    }

    void _free_resource_in_gcontext(jegl_context* context)
    {
        std::lock_guard g1(share_smx);

        for (auto& [_, res] : context->_m_thread_notifier->_m_cached_resources)
            jegl_close_resource(res.m_resource);
    }
    jegl_resource* try_update_shared_resource(jegl_context* context, jegl_resource* resource)
    {
        if (context != nullptr)
        {
            assert(resource != nullptr && resource->m_path != nullptr);

            std::lock_guard g1(share_smx);

            auto current_version = _get_shared_blob_unload_counter(resource->m_path);

            auto fnd = context->_m_thread_notifier->_m_cached_resources.find(resource->m_path);
            if (fnd != context->_m_thread_notifier->_m_cached_resources.end()
                && fnd->second.m_resource->m_type == resource->m_type)
            {
                jegl_close_resource(resource);
                return _share_resource(fnd->second.m_resource);
            }

            // Create a new one!
            auto& shared_resource_cache = context->_m_thread_notifier->_m_cached_resources[resource->m_path];
            shared_resource_cache.m_resource = resource;
            return _share_resource(resource);
        }
        return resource;
    }

    jegl_resource* try_load_shared_resource(jegl_context* context, const char* path)
    {
        if (context != nullptr)
        {
            std::shared_lock sg1(share_smx);

            auto fnd = context->_m_thread_notifier->_m_cached_resources.find(path);
            if (fnd != context->_m_thread_notifier->_m_cached_resources.end())
            {
                shared_resource_used_counts[path]++;
                return _share_resource(fnd->second.m_resource);
            }
        }

        return nullptr;
    }
    void shrink_shared_resource_cache(size_t target_resource_count);
};
_je_graphic_shared_context _je_graphic_shared_context_instance;

void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader);
void jegl_shader_free_generated_glsl(jegl_shader* write_to_shader);

//////////////////////////////////// API /////////////////////////////////////////

std::vector<jegl_context*>   _jegl_alive_glthread_list;
std::shared_mutex           _jegl_alive_glthread_list_mx;

bool _je_graphic_shared_context::_obsolete_shared_resource_cache_nolock(const char* path, bool free_blob)
{
    if (free_blob)
        _unload_share(path);

    bool marked = false;
    for (jegl_context* ctx : _jegl_alive_glthread_list)
    {
        auto fnd = ctx->_m_thread_notifier->_m_cached_resources.find(path);
        if (fnd != ctx->_m_thread_notifier->_m_cached_resources.end())
        {
            jegl_close_resource(fnd->second.m_resource);
            ctx->_m_thread_notifier->_m_cached_resources.erase(fnd);
            marked = true;
        }
    }
    return marked;
}
bool _je_graphic_shared_context::mark_shared_resources_outdated(const char* path)
{
    std::shared_lock sg1(_jegl_alive_glthread_list_mx);
    std::lock_guard g1(share_smx);

    return _obsolete_shared_resource_cache_nolock(path, true);
}
void _je_graphic_shared_context::shrink_shared_resource_cache(size_t target_resource_count)
{
    std::shared_lock sg1(_jegl_alive_glthread_list_mx);
    std::lock_guard g1(share_smx);

    struct obsolete_resource
    {
        std::string m_path;
        size_t m_loadcount;
        bool operator > (const obsolete_resource& another)const noexcept
        {
            return m_loadcount > another.m_loadcount;
        }
    };

    std::priority_queue<
        obsolete_resource,
        std::vector<obsolete_resource>,
        std::greater<obsolete_resource>
    > obs_queue;

    for (auto& [path, loadcount] : shared_resource_used_counts)
        obs_queue.push(obsolete_resource{ path, loadcount });

    while (obs_queue.size() > target_resource_count)
    {
        const auto& obs_res = obs_queue.top();

        shared_resource_used_counts.erase(obs_res.m_path);
        _obsolete_shared_resource_cache_nolock(obs_res.m_path.c_str(), false);

        obs_queue.pop();
    }
}

jeecs_sync_callback_func_t _jegl_sync_callback_func = nullptr;
void* _jegl_sync_callback_arg = nullptr;

void jegl_sync_init(jegl_context* thread, bool isreboot)
{
    if (_current_graphic_thread == nullptr)
        _current_graphic_thread = thread;

    assert(_current_graphic_thread == thread);

    if (thread->m_config.m_fps == 0)
        je_ecs_universe_set_frame_deltatime(
            thread->m_universe_instance,
            0.0);
    else
        je_ecs_universe_set_frame_deltatime(
            thread->m_universe_instance,
            1.0 / (double)thread->m_config.m_fps);

    thread->m_userdata = thread->m_apis->interface_startup(
        thread, &thread->m_config, isreboot);

    ++thread->m_version;
}

jegl_sync_state jegl_sync_update(jegl_context* thread)
{
    std::unordered_map<_jegl_destroy_resouce*, bool> _waiting_to_free_resource;

    if (thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set())
    {
        do
        {
            std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
            thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
                return thread->_m_thread_notifier->m_update_flag;
                });
        } while (0);

        // Ready for rend..
        if (!thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set()
            || thread->_m_thread_notifier->m_reboot_flag)
            goto is_reboot_or_shutdown;

        switch (thread->m_apis->update_frame_ready(thread->m_userdata))
        {
        case jegl_graphic_api::update_action::STOP:
            // graphic thread want to exit. mark stop update
            std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->store(true);
            break;
        case jegl_graphic_api::update_action::CONTINUE:
            // Only clear commited rendchains when update action is CONTINUE.
            thread->_m_frame_rend_work(thread, thread->_m_frame_rend_work_arg);

            if (thread->m_apis->update_draw_commit(thread->m_userdata) == jegl_graphic_api::update_action::STOP)
                std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->store(true);
            break;
        case jegl_graphic_api::update_action::SKIP:
            // Skip current frame.
            if (thread->m_config.m_fps == 0)
                // If vsync is available, wait for 1./120. sec here to decrease CPU usage.
                je_clock_sleep_for(1. / 120.);

            break;
        }

        auto* del_res = thread->_m_thread_notifier->_m_closing_resources.pick_all();
        while (del_res)
        {
            auto* cur_del_res = del_res;
            del_res = del_res->last;

            assert(cur_del_res->m_destroy_resource->m_graphic_thread == thread);

            if (cur_del_res->m_destroy_resource->m_graphic_thread_version == thread->m_version)
            {
                auto* counter = std::launder(reinterpret_cast<std::atomic_uint32_t*>(
                    cur_del_res->m_destroy_resource->m_binding_count));

                if (counter->load() != 0)
                    thread->_m_thread_notifier->_m_closing_resources.add_one(cur_del_res);
                else
                    _waiting_to_free_resource[cur_del_res] = true;
            }
            else
            {
                // Free this
                _waiting_to_free_resource[cur_del_res] = false;
            }
        }

        do {
            std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
            thread->_m_thread_notifier->m_update_flag = false;
            thread->_m_thread_notifier->m_update_waiter.notify_all();
        } while (0);

        for (auto [deleting_resource, need_release] : _waiting_to_free_resource)
        {
            if (need_release)
            {
                thread->m_apis->close_resource(thread->m_userdata, deleting_resource->m_destroy_resource);
                thread->_m_thread_notifier->_m_created_resources.erase(deleting_resource->m_destroy_resource);
            }
            auto* binding_counter = std::launder(reinterpret_cast<std::atomic_uint32_t*>(
                deleting_resource->m_destroy_resource->m_binding_count));

            assert(binding_counter->load() == 0);

            delete binding_counter;
            delete deleting_resource->m_destroy_resource;
            delete deleting_resource;
        }
    }
    else
    {
    is_reboot_or_shutdown:
        if (thread->_m_thread_notifier->m_reboot_flag)
        {
            thread->_m_thread_notifier->m_reboot_flag = false;
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
        thread->m_userdata,
        isreboot);

    for (auto& [_, resource_blob] : thread->_m_thread_notifier->_m_cached_resource_blobs)
    {
        thread->m_apis->close_resource_blob_cache(thread->m_userdata, resource_blob.m_blob);
    }
    thread->_m_thread_notifier->_m_cached_resource_blobs.clear();

    for (auto* resource : thread->_m_thread_notifier->_m_created_resources)
    {
        thread->m_apis->close_resource(thread->m_userdata, resource);
        resource->m_graphic_thread = nullptr;
        resource->m_graphic_thread_version = 0;
    }
    thread->_m_thread_notifier->_m_created_resources.clear();

    thread->m_apis->interface_shutdown(
        thread,
        thread->m_userdata,
        isreboot);

    thread->m_userdata = nullptr;

    if (!isreboot)
    {
        // Really shutdown, give promise!
        auto* promise = std::launder(
            reinterpret_cast<std::promise<void>*>(
                thread->_m_promise));

        promise->set_value();
        _current_graphic_thread = nullptr;
        return true;
    }
    return false;
}

void jegl_register_sync_thread_callback(jeecs_sync_callback_func_t callback, void* arg)
{
    assert(callback != nullptr);

    _jegl_sync_callback_func = callback;
    _jegl_sync_callback_arg = arg;
}

jegl_context* jegl_start_graphic_thread(
    jegl_interface_config config,
    void* universe_instance,
    jeecs_api_register_func_t register_func,
    void(*frame_rend_work)(jegl_context*, void*),
    void* arg)
{
    jegl_context* thread_handle = nullptr;
    if (register_func != nullptr)
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

    if (thread_handle == nullptr)
    {
        jeecs::debug::logfatal("Fail to start up graphic thread, abort and return nullptr.");
    }

    // Register finish functions
    do
    {
        std::lock_guard g1(_jegl_alive_glthread_list_mx);

        if (_jegl_alive_glthread_list.end() ==
            std::find(_jegl_alive_glthread_list.begin(), _jegl_alive_glthread_list.end(),
                thread_handle))
        {
            _jegl_alive_glthread_list.push_back(thread_handle);
        }
    } while (0);

    // Take place.
    thread_handle->m_config = config;
    thread_handle->_m_thread_notifier->m_graphic_terminate_flag.test_and_set();
    thread_handle->_m_thread_notifier->m_update_flag = false;
    thread_handle->_m_thread_notifier->m_reboot_flag = false;
    thread_handle->m_universe_instance = universe_instance;
    thread_handle->m_stop_update = new std::atomic_bool(false);
    thread_handle->_m_frame_rend_work = frame_rend_work;
    thread_handle->_m_frame_rend_work_arg = arg;

    thread_handle->_m_promise = new std::promise<void>();

    assert(_jegl_sync_callback_func != nullptr);
    thread_handle->_m_sync_callback_arg = _jegl_sync_callback_arg;
    _jegl_sync_callback_func(thread_handle, _jegl_sync_callback_arg);

    return thread_handle;
}

void jegl_finish()
{
    std::vector<jegl_context*> shutdown_glthreads;
    do
    {
        std::lock_guard g1(_jegl_alive_glthread_list_mx);
        shutdown_glthreads = _jegl_alive_glthread_list;
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
        std::lock_guard g1(_jegl_alive_glthread_list_mx);
        auto fnd = std::find(_jegl_alive_glthread_list.begin(), _jegl_alive_glthread_list.end(), thread);
        if (fnd == _jegl_alive_glthread_list.end())
            return;
        else
        {
            _jegl_alive_glthread_list.erase(fnd);
        }

    } while (0);

    assert(thread->_m_thread_notifier->m_graphic_terminate_flag.test_and_set());
    thread->_m_thread_notifier->m_graphic_terminate_flag.clear();

    do
    {
        std::lock_guard g1(thread->_m_thread_notifier->m_update_mx);
        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();
    } while (0);

    auto* promise = std::launder(reinterpret_cast<std::promise<void>*>(thread->_m_promise));
    promise->get_future().get();

    _je_graphic_shared_context_instance._free_resource_in_gcontext(thread);

    auto* closing_resource = thread->_m_thread_notifier->_m_closing_resources.pick_all();
    while (closing_resource)
    {
        auto* cur_closing_resource = closing_resource;
        closing_resource = closing_resource->last;

        delete cur_closing_resource->m_destroy_resource;
        delete cur_closing_resource;
    }

    delete promise;

    delete std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update));
    delete thread->_m_thread_notifier;
    delete thread->m_apis;
    delete thread;
}

bool jegl_update(jegl_context* thread, jegl_update_sync_mode mode)
{
    if (std::launder(reinterpret_cast<std::atomic_bool*>(thread->m_stop_update))->load())
        return false;

    do
    {
        std::unique_lock uq1(thread->_m_thread_notifier->m_update_mx);
        if (mode == jegl_update_sync_mode::JEGL_WAIT_LAST_FRAME_END)
        {
            // Wait until `last` frame draw end.
            thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
                return !thread->_m_thread_notifier->m_update_flag;
                });
        }

        thread->_m_thread_notifier->m_update_flag = true;
        thread->_m_thread_notifier->m_update_waiter.notify_all();

        if (mode == jegl_update_sync_mode::JEGL_WAIT_THIS_FRAME_END)
        {
            // Wait until `this` frame draw end.
            thread->_m_thread_notifier->m_update_waiter.wait(uq1, [thread]()->bool {
                return !thread->_m_thread_notifier->m_update_flag;
                });
        }
    } while (0);

    return true;
}

void jegl_reboot_graphic_thread(jegl_context* thread_handle, const jegl_interface_config* config)
{
    if (config)
        thread_handle->m_config = *config;

    thread_handle->_m_thread_notifier->m_reboot_flag = true;
}

bool jegl_mark_shared_resources_outdated(const char* path)
{
    return _je_graphic_shared_context_instance.mark_shared_resources_outdated(path);
}

bool jegl_using_resource(jegl_resource* resource)
{
    bool need_init_resouce = false;

    if (!_current_graphic_thread)
    {
        jeecs::debug::logerr("Graphic resource only usable in graphic thread.");
        return false;
    }

    if (!resource->m_graphic_thread)
    {
        need_init_resouce = true;
        resource->m_graphic_thread = _current_graphic_thread;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }
    if (_current_graphic_thread != resource->m_graphic_thread)
    {
        jeecs::debug::logerr("This resource has been used in graphic thread: %p.", resource->m_graphic_thread);
        return false;
    }
    if (resource->m_graphic_thread_version != _current_graphic_thread->m_version)
    {
        need_init_resouce = true;
        resource->m_graphic_thread_version = _current_graphic_thread->m_version;
    }

    // If resource is died, ignore it.
    if (resource->m_custom_resource == nullptr)
        need_init_resouce = false;

    if (need_init_resouce)
    {
        jegl_resource_blob resource_blob = nullptr;

        size_t resource_blob_version = 0;
        if (resource->m_path != nullptr)
        {
            do
            {
                std::shared_lock sg1(_je_graphic_shared_context_instance.share_smx);

                resource_blob_version =
                    _je_graphic_shared_context_instance._get_shared_blob_unload_counter(
                        resource->m_path);
            } while (0);

            auto fnd = _current_graphic_thread->_m_thread_notifier
                ->_m_cached_resource_blobs.find(resource->m_path);

            if (fnd != _current_graphic_thread->_m_thread_notifier
                ->_m_cached_resource_blobs.end())
            {
                // Check if outdated?
                if (fnd->second.m_version == resource_blob_version)
                    resource_blob = fnd->second.m_blob;
                else
                {
                    // Clear outdated blob.
                    _current_graphic_thread->m_apis
                        ->close_resource_blob_cache(_current_graphic_thread->m_userdata, fnd->second.m_blob);
                }
            }
        }

        if (resource_blob == nullptr)
            resource_blob = _current_graphic_thread->m_apis->create_resource_blob_cache(
                _current_graphic_thread->m_userdata, resource);

        if (resource->m_path != nullptr)
        {
            auto& cached_blob = _current_graphic_thread->_m_thread_notifier
                ->_m_cached_resource_blobs[resource->m_path];

            cached_blob.m_blob = resource_blob;
            cached_blob.m_version = resource_blob_version;
        }

        // Create resource by blob.
        _current_graphic_thread->m_apis->create_resource(
            _current_graphic_thread->m_userdata, resource_blob, resource);

        if (resource->m_path == nullptr)
            _current_graphic_thread->m_apis->close_resource_blob_cache(
                _current_graphic_thread->m_userdata, resource_blob);

        _current_graphic_thread->_m_thread_notifier->_m_created_resources.insert(resource);
    }

    _current_graphic_thread->m_apis->using_resource(
        _current_graphic_thread->m_userdata, resource);

    return need_init_resouce;
}

void _jegl_free_resource_instance(jegl_resource* resource)
{
    assert(resource != nullptr);
    assert(resource->m_binding_count != nullptr);
    assert(resource->m_custom_resource == nullptr);
    assert(resource->m_raw_ref_count == nullptr);

    // Send this resource to destroing list;
    auto* del_res = new _jegl_destroy_resouce;
    del_res->m_destroy_resource = resource;

    std::shared_lock sg1(_jegl_alive_glthread_list_mx);
    auto fnd = std::find(
        _jegl_alive_glthread_list.begin(),
        _jegl_alive_glthread_list.end(),
        del_res->m_destroy_resource->m_graphic_thread);

    if (fnd != _jegl_alive_glthread_list.end())
        (*fnd)->_m_thread_notifier->_m_closing_resources.add_one(del_res);
    else
    {
        // 既然这个资源已经没有管理线程了，直接就地杀了埋了
        if (del_res->m_destroy_resource->m_graphic_thread != nullptr)
            jeecs::debug::logwarn("Resource %p cannot free by correct graphic context, maybe it is out-dated? Free it!",
                del_res->m_destroy_resource);

        auto* binding_count = std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_binding_count));
        if (--*binding_count == jeecs::typing::INVALID_UINT32)
        {
            delete binding_count;
            delete del_res->m_destroy_resource;
        }
        else
        {
            // 资源尚被其他RendChain使用，将被RendChain负责释放
            // NOTE: 代码跑到这边只有以下情况：
            //  * 资源仅被绑定到RendChain上，但是RendChain尚未被提交
            //  * 资源被绑定到RendChain上，但是被覆盖（一般见于纹理）
            // 无论哪一种，都无所谓，因为资源并没有被图形实现创建，因此也不必担心释放问题
        }
        delete del_res;
    }
}

void _jegl_free_vertex_bone_data(const jegl_vertex::bone_data* b)
{
    je_mem_free((void*)b->m_name);
    je_mem_free((void*)b);
}

void jegl_close_resource(jegl_resource* resource)
{
    assert(resource != nullptr);
    assert(resource->m_custom_resource != nullptr);

    if (resource->m_raw_ref_count == nullptr ||
        1 == std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count))->fetch_sub(1))
    {
        switch (resource->m_type)
        {
        case jegl_resource::TEXTURE:
            // close resource's raw data, then send this resource to closing-queue
            if (resource->m_raw_texture_data->m_pixels)
            {
                assert(0 == (resource->m_raw_texture_data->m_format & jegl_texture::format::FORMAT_MASK));
                stbi_image_free(resource->m_raw_texture_data->m_pixels);
                delete resource->m_raw_texture_data;
            }
            else
                assert(0 != (resource->m_raw_texture_data->m_format & jegl_texture::format::FORMAT_MASK));
            break;
        case jegl_resource::SHADER:
            // close resource's raw data, then send this resource to closing-queue
            jegl_shader_free_generated_glsl(resource->m_raw_shader_data);
            delete resource->m_raw_shader_data;
            break;
        case jegl_resource::VERTEX:
            // close resource's raw data, then send this resource to closing-queue
            je_mem_free((void*)resource->m_raw_vertex_data->m_vertexs);
            je_mem_free((void*)resource->m_raw_vertex_data->m_indexs);
            je_mem_free((void*)resource->m_raw_vertex_data->m_formats);
            for (size_t bone_idx = 0; bone_idx < resource->m_raw_vertex_data->m_bone_count; ++bone_idx)
                _jegl_free_vertex_bone_data(resource->m_raw_vertex_data->m_bones[bone_idx]);

            je_mem_free((void*)resource->m_raw_vertex_data->m_bones);

            delete resource->m_raw_vertex_data;
            break;
        case jegl_resource::FRAMEBUF:
            // Close all frame rend out texture
            if (!resource->m_raw_framebuf_data)
                assert(resource->m_raw_framebuf_data->m_attachment_count == 0);
            else
            {
                assert(resource->m_raw_framebuf_data->m_attachment_count > 0);
                jeecs::basic::resource<jeecs::graphic::texture>* attachments =
                    (jeecs::basic::resource<jeecs::graphic::texture>*)resource->m_raw_framebuf_data->m_output_attachments;

                delete[]attachments;
            }
            delete resource->m_raw_framebuf_data;
            break;
        case jegl_resource::UNIFORMBUF:
            je_mem_free(resource->m_raw_uniformbuf_data->m_buffer);
            delete resource->m_raw_uniformbuf_data;
            break;
        default:
            jeecs::debug::logerr("Unknown resource type to close.");
            return;
        }

        if (resource->m_path)
            je_mem_free((void*)resource->m_path);

        resource->m_custom_resource = nullptr;

        if (resource->m_raw_ref_count != nullptr)
        {
            delete std::launder(reinterpret_cast<std::atomic_uint32_t*>(resource->m_raw_ref_count));
            resource->m_raw_ref_count = nullptr;
        }
        _jegl_free_resource_instance(resource);
    }
}

jegl_resource* _create_resource()
{
    jegl_resource* res = new jegl_resource{};
    res->m_binding_count =
        reinterpret_cast<void*>(new std::atomic_uint32_t(0));
    return res;
}

jegl_resource* jegl_create_texture(size_t width, size_t height, jegl_texture::format format)
{
    jegl_resource* texture = _create_resource();
    texture->m_type = jegl_resource::TEXTURE;
    texture->m_raw_texture_data = new jegl_texture();
    texture->m_path = nullptr;

    if (width == 0) width = 1;
    if (height == 0) height = 1;

    if ((format & jegl_texture::format::FORMAT_MASK) == 0)
    {
        texture->m_raw_texture_data->m_pixels = (jegl_texture::pixel_data_t*)malloc(width * height * format);
        assert(texture->m_raw_texture_data->m_pixels);

        memset(texture->m_raw_texture_data->m_pixels, 0, width * height * format);
    }
    else
        // For special format texture such as depth, do not alloc for pixel.
        texture->m_raw_texture_data->m_pixels = nullptr;

    texture->m_raw_texture_data->m_width = width;
    texture->m_raw_texture_data->m_height = height;
    texture->m_raw_texture_data->m_format = format;
    texture->m_raw_texture_data->m_modified = false;

    return texture;
}

jegl_resource* _jegl_load_shader_cache(jeecs_file* cache_file, const char* path)
{
    assert(cache_file != nullptr);

    jegl_shader* _shader = new jegl_shader();

    jegl_resource* shader = _create_resource();
    shader->m_type = jegl_resource::SHADER;
    shader->m_raw_shader_data = _shader;

    uint64_t
        vertex_glsl_src_len,
        fragment_glsl_src_len,
        vertex_hlsl_src_len,
        fragment_hlsl_src_len,
        vertex_spirv_src_len,
        fragment_spirv_src_len;

    // 1. Read generated source
    jeecs_file_read(&vertex_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_vertex_glsl_src = (const char*)je_mem_alloc((size_t)vertex_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_glsl_src), sizeof(char), (size_t)vertex_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_glsl_src)[(size_t)vertex_glsl_src_len] = 0;
    jeecs_file_read(&fragment_glsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_fragment_glsl_src = (const char*)je_mem_alloc((size_t)fragment_glsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_glsl_src), sizeof(char), (size_t)fragment_glsl_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_glsl_src)[(size_t)fragment_glsl_src_len] = 0;

    jeecs_file_read(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_vertex_hlsl_src = (const char*)je_mem_alloc((size_t)vertex_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_vertex_hlsl_src), sizeof(char), (size_t)vertex_hlsl_src_len, cache_file);
    const_cast<char*>(_shader->m_vertex_hlsl_src)[(size_t)vertex_hlsl_src_len] = 0;
    jeecs_file_read(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cache_file);
    _shader->m_fragment_hlsl_src = (const char*)je_mem_alloc((size_t)fragment_hlsl_src_len + 1);
    jeecs_file_read(const_cast<char*>(_shader->m_fragment_hlsl_src), sizeof(char), (size_t)fragment_hlsl_src_len, cache_file);
    const_cast<char*>(_shader->m_fragment_hlsl_src)[(size_t)fragment_hlsl_src_len] = 0;

    jeecs_file_read(&vertex_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)vertex_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);
    _shader->m_vertex_spirv_count = (size_t)vertex_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_vertex_spirv_codes = (const jegl_shader::spir_v_code_t*)je_mem_alloc((size_t)vertex_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t*>(_shader->m_vertex_spirv_codes), sizeof(char), (size_t)vertex_spirv_src_len, cache_file);
    jeecs_file_read(&fragment_spirv_src_len, sizeof(uint64_t), 1, cache_file);
    assert((size_t)fragment_spirv_src_len % sizeof(jegl_shader::spir_v_code_t) == 0);
    _shader->m_fragment_spirv_count = (size_t)fragment_spirv_src_len / sizeof(jegl_shader::spir_v_code_t);
    _shader->m_fragment_spirv_codes = (const jegl_shader::spir_v_code_t*)je_mem_alloc((size_t)fragment_spirv_src_len);
    jeecs_file_read(const_cast<jegl_shader::spir_v_code_t*>(_shader->m_fragment_spirv_codes), sizeof(char), (size_t)fragment_spirv_src_len, cache_file);

    // 2. read shader config
    jeecs_file_read(&_shader->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cache_file);
    jeecs_file_read(&_shader->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cache_file);
    jeecs_file_read(&_shader->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cache_file);

    // 3. read if shader is enable to shared?
    jeecs_file_read(&_shader->m_enable_to_shared, sizeof(bool), 1, cache_file);

    // 4. read and generate custom variable & uniform block informs

    // 4.1 read and generate custom variable
    uint64_t custom_uniform_count;
    jeecs_file_read(&custom_uniform_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniforms_count = (size_t)custom_uniform_count;
    _shader->m_custom_uniforms =
        new jegl_shader::unifrom_variables[_shader->m_custom_uniforms_count];

    for (uint64_t i = 0; i < custom_uniform_count; ++i)
    {
        jegl_shader::unifrom_variables* current_variable =
            &_shader->m_custom_uniforms[i];

        // 4.1.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_variable->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_variable->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_variable->m_name)[(size_t)uniform_name_len] = 0;

        // 4.1.2 read type
        jeecs_file_read(&current_variable->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cache_file);

        // 4.1.3 read data
        static_assert(sizeof(current_variable->mat4x4) == sizeof(float[4][4]));
        jeecs_file_read(&current_variable->mat4x4, sizeof(float[4][4]), 1, cache_file);

        current_variable->m_index = jeecs::typing::INVALID_UINT32;
        current_variable->m_updated = false;
    }

    // 4.2 read uniform block informs
    uint64_t custom_uniform_block_count;
    jeecs_file_read(&custom_uniform_block_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_custom_uniform_blocks = nullptr;

    jegl_shader::uniform_blocks* last_create_block = nullptr;
    for (uint64_t i = 0; i < custom_uniform_block_count; ++i)
    {
        jegl_shader::uniform_blocks* current_block = new jegl_shader::uniform_blocks();
        if (_shader->m_custom_uniform_blocks == nullptr)
            _shader->m_custom_uniform_blocks = current_block;

        if (last_create_block != nullptr)
            last_create_block->m_next = current_block;

        // 4.2.1 read name
        uint64_t uniform_name_len;
        jeecs_file_read(&uniform_name_len, sizeof(uint64_t), 1, cache_file);
        current_block->m_name = (const char*)je_mem_alloc((size_t)uniform_name_len + 1);
        jeecs_file_read(const_cast<char*>(current_block->m_name), sizeof(char), (size_t)uniform_name_len, cache_file);
        const_cast<char*>(current_block->m_name)[(size_t)uniform_name_len] = 0;

        // 4.2.2 read binding place
        static_assert(sizeof(current_block->m_specify_binding_place) == sizeof(uint32_t));
        jeecs_file_read(&current_block->m_specify_binding_place, sizeof(uint32_t), 1, cache_file);

        last_create_block = current_block;
        current_block->m_next = nullptr;
    }

    // 4.3 read shader vertex layout
    uint64_t vertex_in_count;
    jeecs_file_read(&vertex_in_count, sizeof(uint64_t), 1, cache_file);

    _shader->m_vertex_in_count = (size_t)vertex_in_count;
    _shader->m_vertex_in = new jegl_shader::vertex_in_variables[_shader->m_vertex_in_count];

    jeecs_file_read(_shader->m_vertex_in, sizeof(jegl_shader::vertex_in_variables),
        _shader->m_vertex_in_count, cache_file);

    // 4.4 read sampler informations;
    uint64_t sampler_count;
    jeecs_file_read(&sampler_count, sizeof(uint64_t), 1, cache_file);
    _shader->m_sampler_count = (size_t)sampler_count;
    _shader->m_sampler_methods = new jegl_shader::sampler_method[(size_t)sampler_count];
    for (uint64_t i = 0; i < sampler_count; ++i)
    {
        auto& sampler = _shader->m_sampler_methods[i];
        jeecs_file_read(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cache_file);
        jeecs_file_read(&sampler.m_sampler_id, sizeof(uint32_t), 1, cache_file);

        uint64_t pass_id_count;
        jeecs_file_read(&pass_id_count, sizeof(uint64_t), 1, cache_file);
        sampler.m_pass_id_count = (size_t)pass_id_count;
        sampler.m_pass_ids = new uint32_t[sampler.m_pass_id_count];
        jeecs_file_read(sampler.m_pass_ids, sizeof(uint32_t), sampler.m_pass_id_count, cache_file);
    }

    jeecs_file_close(cache_file);

    shader->m_path = jeecs::basic::make_new_string(path);
    return shader;
}

void _jegl_create_shader_cache(jegl_resource* shader_resource, wo_integer_t virtual_file_crc64)
{
    assert(shader_resource->m_path != nullptr
        && shader_resource->m_raw_shader_data
        && shader_resource->m_type == jegl_resource::type::SHADER);

    if (auto* cachefile = jeecs_create_cache_file(
        shader_resource->m_path,
        SHADER_CACHE_VERSION,
        virtual_file_crc64))
    {
        auto* raw_shader_data = shader_resource->m_raw_shader_data;

        uint64_t vertex_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_glsl_src);
        uint64_t fragment_glsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_glsl_src);
        uint64_t vertex_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_vertex_hlsl_src);
        uint64_t fragment_hlsl_src_len = (uint64_t)strlen(raw_shader_data->m_fragment_hlsl_src);
        uint64_t vertex_spirv_src_len = (uint64_t)raw_shader_data->m_vertex_spirv_count * sizeof(jegl_shader::spir_v_code_t);
        uint64_t fragment_spirv_src_len = (uint64_t)raw_shader_data->m_fragment_spirv_count * sizeof(jegl_shader::spir_v_code_t);

        // 1. write shader generated source to cache
        jeecs_write_cache_file(&vertex_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_glsl_src, sizeof(char), (size_t)vertex_glsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_glsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_glsl_src, sizeof(char), (size_t)fragment_glsl_src_len, cachefile);

        jeecs_write_cache_file(&vertex_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_hlsl_src, sizeof(char), (size_t)vertex_hlsl_src_len, cachefile);
        jeecs_write_cache_file(&fragment_hlsl_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_hlsl_src, sizeof(char), (size_t)fragment_hlsl_src_len, cachefile);

        jeecs_write_cache_file(&vertex_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_spirv_codes, sizeof(char), (size_t)vertex_spirv_src_len, cachefile);
        jeecs_write_cache_file(&fragment_spirv_src_len, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_fragment_spirv_codes, sizeof(char), (size_t)fragment_spirv_src_len, cachefile);

        // 2. write shader config to cache
        /*
            depth_test_method   m_depth_test;
            depth_mask_method   m_depth_mask;
            blend_method        m_blend_src_mode, m_blend_dst_mode;
            cull_mode           m_cull_mode;
        */
        jeecs_write_cache_file(&raw_shader_data->m_depth_test, sizeof(jegl_shader::depth_test_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_depth_mask, sizeof(jegl_shader::depth_mask_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_src_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_blend_dst_mode, sizeof(jegl_shader::blend_method), 1, cachefile);
        jeecs_write_cache_file(&raw_shader_data->m_cull_mode, sizeof(jegl_shader::cull_mode), 1, cachefile);

        // 3. write if shader is enable to shared?
        jeecs_write_cache_file(&raw_shader_data->m_enable_to_shared, sizeof(bool), 1, cachefile);

        // 4. write shader custom variable & uniform block informs.
        uint64_t count_for_uniform = (uint64_t)raw_shader_data->m_custom_uniforms_count;
        uint64_t count_for_uniform_block = 0;

        auto* custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            ++count_for_uniform_block;
            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.1 write shader custom variable
        jeecs_write_cache_file(&count_for_uniform, sizeof(uint64_t), 1, cachefile);
        for (size_t i = 0; i < raw_shader_data->m_custom_uniforms_count; ++i)
        {
            auto* current_custom_uniform = &raw_shader_data->m_custom_uniforms[i];

            // 4.1.1 write name
            uint64_t uniform_name_len = (uint64_t)strlen(current_custom_uniform->m_name);
            jeecs_write_cache_file(&uniform_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(current_custom_uniform->m_name, sizeof(char), (size_t)uniform_name_len, cachefile);

            // 4.1.2 write type
            jeecs_write_cache_file(&current_custom_uniform->m_uniform_type, sizeof(jegl_shader::uniform_type), 1, cachefile);

            // 4.1.3 write data
            static_assert(sizeof(current_custom_uniform->mat4x4) == sizeof(float[4][4]));
            jeecs_write_cache_file(&current_custom_uniform->mat4x4, sizeof(float[4][4]), 1, cachefile);
        }

        // 4.2 write shader custom uniform block informs
        jeecs_write_cache_file(&count_for_uniform_block, sizeof(uint64_t), 1, cachefile);
        custom_uniform_block = raw_shader_data->m_custom_uniform_blocks;
        while (custom_uniform_block)
        {
            // 4.2.1 write name
            uint64_t uniform_block_name_len = (uint64_t)strlen(custom_uniform_block->m_name);
            jeecs_write_cache_file(&uniform_block_name_len, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(custom_uniform_block->m_name, sizeof(char), (size_t)uniform_block_name_len, cachefile);

            // 4.2.2 write place
            static_assert(sizeof(custom_uniform_block->m_specify_binding_place) == sizeof(uint32_t));
            jeecs_write_cache_file(&custom_uniform_block->m_specify_binding_place, sizeof(uint32_t), 1, cachefile);

            custom_uniform_block = custom_uniform_block->m_next;
        }

        // 4.3 write shader vertex layout
        uint64_t vertex_in_count = (uint64_t)raw_shader_data->m_vertex_in_count;
        jeecs_write_cache_file(&vertex_in_count, sizeof(uint64_t), 1, cachefile);
        jeecs_write_cache_file(raw_shader_data->m_vertex_in, sizeof(jegl_shader::vertex_in_variables),
            raw_shader_data->m_vertex_in_count, cachefile);

        // 4.4 write sampler informations;
        uint64_t sampler_count = (uint64_t)raw_shader_data->m_sampler_count;
        jeecs_write_cache_file(&sampler_count, sizeof(uint64_t), 1, cachefile);
        for (uint64_t i = 0; i < sampler_count; ++i)
        {
            auto& sampler = raw_shader_data->m_sampler_methods[i];
            jeecs_write_cache_file(&sampler.m_min, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mag, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_mip, sizeof(jegl_shader::fliter_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_uwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_vwrap, sizeof(jegl_shader::wrap_mode), 1, cachefile);
            jeecs_write_cache_file(&sampler.m_sampler_id, sizeof(uint32_t), 1, cachefile);

            uint64_t sampler_count = (uint64_t)sampler.m_pass_id_count;
            jeecs_write_cache_file(&sampler_count, sizeof(uint64_t), 1, cachefile);
            jeecs_write_cache_file(sampler.m_pass_ids, sizeof(uint32_t), sampler.m_pass_id_count, cachefile);
        }

        jeecs_close_cache_file(cachefile);
    }
}

jegl_resource* jegl_load_shader_source(const char* path, const char* src, bool is_virtual_file)
{
    if (is_virtual_file)
    {
        if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, wo_crc64_str(src)))
            return _jegl_load_shader_cache(shader_cache, path);
    }

    wo_vm vmm = wo_create_vm();
    if (!wo_load_source(vmm, path, src))
    {
        // Compile error
        jeecs::debug::logerr("Fail to load shader: %s.\n%s", path, wo_get_compile_error(vmm, WO_DEFAULT));
        wo_close_vm(vmm);
        return nullptr;
    }

    wo_run(vmm);

    wo_integer_t generate_shader_func;
    [[maybe_unused]] wo_handle_t generate_shader_jit_func;

    if (!wo_extern_symb(vmm, "shader::generate", &generate_shader_func, &generate_shader_jit_func))
    {
        jeecs::debug::logerr("Fail to load shader: %s. you should import je::shader.", path);
        wo_close_vm(vmm);
        return nullptr;
    }

    if (wo_value retval = wo_invoke_rsfunc(vmm, generate_shader_func, 0, nullptr, nullptr))
    {
        void* shader_graph = wo_pointer(retval);

        jegl_shader* _shader = new jegl_shader();

        jegl_shader_generate_glsl(shader_graph, _shader);

        jegl_resource* shader = _create_resource();
        shader->m_type = jegl_resource::SHADER;
        shader->m_raw_shader_data = _shader;
        shader->m_path = jeecs::basic::make_new_string(path);

        _jegl_create_shader_cache(shader, is_virtual_file ? wo_crc64_str(src) : 0);

        wo_close_vm(vmm);
        return shader;
    }
    else
    {
        jeecs::debug::logerr("Fail to load shader: %s: %s.", path, wo_get_runtime_error(vmm));
        wo_close_vm(vmm);
        return nullptr;
    }

}

jegl_resource* jegl_try_update_shared_resource(jegl_context* context, jegl_resource* resource)
{
    return _je_graphic_shared_context_instance.try_update_shared_resource(context, resource);
}

jegl_resource* jegl_try_load_shared_resource(jegl_context* context, const char* path)
{
    return _je_graphic_shared_context_instance.try_load_shared_resource(context, path);
}

void jegl_shrink_shared_resource_cache(jegl_context* context, size_t shrink_target_count)
{
    _je_graphic_shared_context_instance.shrink_shared_resource_cache(shrink_target_count);
}

jegl_resource* jegl_load_shader(jegl_context* context, const char* path)
{
    auto* shared_shader = jegl_try_load_shared_resource(context, path);
    if (shared_shader != nullptr && shared_shader->m_type == jegl_resource::type::SHADER)
        return shared_shader;

    if (jeecs_file* shader_cache = jeecs_load_cache_file(path, SHADER_CACHE_VERSION, 0))
    {
        auto* shader_resource = _jegl_load_shader_cache(shader_cache, path);

        assert(shader_resource != nullptr && shader_resource->m_raw_shader_data != nullptr);

        if (shader_resource->m_raw_shader_data->m_enable_to_shared)
            return jegl_try_update_shared_resource(context, shader_resource);
        return shader_resource;
    }
    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        char* src = (char*)malloc(texfile->m_file_length + 1);
        jeecs_file_read(src, sizeof(char), texfile->m_file_length, texfile);
        src[texfile->m_file_length] = 0;

        jeecs_file_close(texfile);

        auto* shader_resource = jegl_load_shader_source(path, src, false);
        free(src);

        if (shader_resource != nullptr)
        {
            assert(shader_resource != nullptr && shader_resource->m_raw_shader_data != nullptr);
            if (shader_resource->m_raw_shader_data->m_enable_to_shared)
                return jegl_try_update_shared_resource(context, shader_resource);
            return shader_resource;
        }
        return nullptr;

    }
    jeecs::debug::logerr("Fail to open file: '%s'", path);
    return nullptr;
}

jegl_resource* jegl_load_texture(jegl_context* context, const char* path)
{
    auto* shared_texture = jegl_try_load_shared_resource(context, path);
    if (shared_texture != nullptr && shared_texture->m_type == jegl_resource::type::TEXTURE)
        return shared_texture;

    if (jeecs_file* texfile = jeecs_file_open(path))
    {
        jegl_resource* texture = _create_resource();
        texture->m_type = jegl_resource::TEXTURE;
        texture->m_raw_texture_data = new jegl_texture();
        texture->m_path = jeecs::basic::make_new_string(path);

        int w, h, cdepth;

        stbi_set_flip_vertically_on_load(true);

        stbi_io_callbacks res_reader;
        res_reader.read = [](void* f, char* data, int size) {return (int)jeecs_file_read(data, 1, (size_t)size, (jeecs_file*)f); };
        res_reader.eof = [](void* f) {return jeecs_file_tell((jeecs_file*)f) >= ((jeecs_file*)f)->m_file_length ? 1 : 0; };
        res_reader.skip = [](void* f, int n) {jeecs_file_seek((jeecs_file*)f, (size_t)n, je_read_file_seek_mode::JE_READ_FILE_CURRENT); };

        texture->m_raw_texture_data->m_pixels = stbi_load_from_callbacks(&res_reader, texfile, &w, &h, &cdepth, STBI_rgb_alpha);

        jeecs_file_close(texfile);

        if (texture->m_raw_texture_data->m_pixels == nullptr)
        {
            jeecs::debug::logerr("Cannot load texture: Invalid image format of file: '%s'", path);
            delete texture->m_raw_texture_data;
            delete texture;
            return nullptr;
        }

        texture->m_raw_texture_data->m_width = (size_t)w;
        texture->m_raw_texture_data->m_height = (size_t)h;
        texture->m_raw_texture_data->m_format = jegl_texture::RGBA;
        texture->m_raw_texture_data->m_modified = false;

        return jegl_try_update_shared_resource(context, texture);
    }

    jeecs::debug::loginfo("Cannot load texture: Failed to open file: '%s'", path);
    return nullptr;
}

jegl_resource* jegl_load_vertex(jegl_context* context, const char* path)
{
    const size_t MAX_BONE_COUNT = 256;

    auto* shared_vertex = jegl_try_load_shared_resource(context, path);
    if (shared_vertex != nullptr && shared_vertex->m_type == jegl_resource::type::VERTEX)
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
        float m_vertex[3];
        float m_texcoord[2];
        float m_normal[3];

        int32_t m_bone_id[4];
        float m_bone_weight[4];
    };
    static_assert(sizeof(jegl_standard_model_vertex_t) == 64);

    const jegl_vertex::data_layout jegl_standard_model_vertex_format[] = {
        {jegl_vertex::data_type::FLOAT32, 3},
        {jegl_vertex::data_type::FLOAT32, 2},
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

            const auto* texcrood_0 = mesh->mTextureCoords[0];

            for (size_t index = 0; index < mesh->mNumVertices; ++index)
            {
                auto& vertex = mesh->mVertices[index];
                auto& normal = mesh->mNormals[index];

                jegl_standard_model_vertex_t model_vertex = {};
                model_vertex.m_vertex[0] = vertex.x;
                model_vertex.m_vertex[1] = vertex.y;
                model_vertex.m_vertex[2] = vertex.z;
                if (texcrood_0 != nullptr)
                {
                    model_vertex.m_texcoord[0] = texcrood_0[index].x;
                    model_vertex.m_texcoord[1] = texcrood_0[index].y;
                }
                model_vertex.m_normal[0] = normal.x;
                model_vertex.m_normal[1] = normal.y;
                model_vertex.m_normal[2] = normal.z;

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

                for (size_t fvertex_index = 0; fvertex_index < face.mNumIndices; ++fvertex_index)
                    index_datas.push_back(face.mIndices[fvertex_index] + current_model_vertex_offset);
            }

            for (size_t bone_idx = 0; bone_idx < mesh->mNumBones; ++bone_idx)
            {
                jegl_vertex::bone_data* bone_data =
                    (jegl_vertex::bone_data*)je_mem_alloc(sizeof(jegl_vertex::bone_data));

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
                        if (vertex.m_vertex[i] == -1
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
        for (size_t i = 0; i < 4; ++i)
        {
            if (vdata.m_bone_id[i] == -1)
            {
                vdata.m_bone_id[i] = 0;
                vdata.m_bone_weight[i] = 0.f;
            }
        }
    }

    auto* vertex = jegl_create_vertex(
        jegl_vertex::type::TRIANGLES,
        vertex_datas.data(),
        vertex_datas.size() * sizeof(jegl_standard_model_vertex_t),
        index_datas.data(),
        index_datas.size(),
        jegl_standard_model_vertex_format,
        sizeof(jegl_standard_model_vertex_format) / sizeof(jegl_vertex::data_layout));

    if (vertex != nullptr)
    {
        vertex->m_path = jeecs::basic::make_new_string(path);

        const jegl_vertex::bone_data** cbones =
            (const jegl_vertex::bone_data**)je_mem_alloc(
                bones.size() * sizeof(const jegl_vertex::bone_data*));

        memcpy(
            cbones,
            bones.data(),
            bones.size() * sizeof(const jegl_vertex::bone_data*));

        vertex->m_raw_vertex_data->m_bones = cbones;
        vertex->m_raw_vertex_data->m_bone_count = bones.size();

        return jegl_try_update_shared_resource(context, vertex);
    }
    else
    {
        for (auto* bonedata : bones)
            _jegl_free_vertex_bone_data(bonedata);
    }
    return nullptr;
}

jegl_resource* jegl_create_vertex(
    jegl_vertex::type               type,
    const void* datas,
    size_t                          data_length,
    const uint32_t* indexs,
    size_t                          index_count,
    const jegl_vertex::data_layout* format,
    size_t                          format_count)
{
    size_t data_size_per_point = 0;
    for (size_t i = 0; i < format_count; ++i)
        data_size_per_point += format[i].m_count * 4;  /* Both int32 & float32 is 4 byte*/

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

    jegl_resource* vertex = _create_resource();
    vertex->m_type = jegl_resource::VERTEX;
    vertex->m_raw_vertex_data = new jegl_vertex();

    vertex->m_raw_vertex_data->m_type = type;
    vertex->m_raw_vertex_data->m_data_size_per_point = data_size_per_point;
    vertex->m_raw_vertex_data->m_bones = nullptr;
    vertex->m_raw_vertex_data->m_bone_count = 0;

    void* data_buffer = (void*)je_mem_alloc(data_length);
    memcpy(data_buffer, datas, data_length);
    vertex->m_raw_vertex_data->m_vertexs = data_buffer;
    vertex->m_raw_vertex_data->m_vertex_length = data_length;

    uint32_t* index_buffer = (uint32_t*)je_mem_alloc(index_count * sizeof(uint32_t));
    memcpy(index_buffer, indexs, index_count * sizeof(uint32_t));
    vertex->m_raw_vertex_data->m_indexs = index_buffer;
    vertex->m_raw_vertex_data->m_index_count = index_count;

    jegl_vertex::data_layout* formats
        = (jegl_vertex::data_layout*)je_mem_alloc(format_count * sizeof(jegl_vertex::data_layout));
    memcpy(formats, format,
        format_count * sizeof(jegl_vertex::data_layout));
    vertex->m_raw_vertex_data->m_formats = formats;
    vertex->m_raw_vertex_data->m_format_count = format_count;

    // Calc size by default:
    float
        x_min = 0.f, x_max = 0.f,
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
        jeecs::debug::logwarn("Position data of vertex(%p) mismatch, first data should be position and with length '3'", vertex);

    vertex->m_raw_vertex_data->m_x_min = x_min;
    vertex->m_raw_vertex_data->m_x_max = x_max;
    vertex->m_raw_vertex_data->m_y_min = y_min;
    vertex->m_raw_vertex_data->m_y_max = y_max;
    vertex->m_raw_vertex_data->m_z_min = z_min;
    vertex->m_raw_vertex_data->m_z_max = z_max;

    return vertex;
}


jegl_resource* jegl_create_framebuf(
    size_t width, size_t height,
    const jegl_texture::format* attachment_formats,
    size_t attachment_count)
{
    if (width == 0 || height == 0 || attachment_count == 0)
    {
        jeecs::debug::logwarn("Failed to create invalid framebuffer: size is zero or no attachment.");
        return nullptr;
    }

    jegl_resource* framebuf = _create_resource();
    framebuf->m_type = jegl_resource::FRAMEBUF;
    framebuf->m_raw_framebuf_data = new jegl_frame_buffer();
    framebuf->m_path = nullptr;

    framebuf->m_raw_framebuf_data->m_attachment_count = attachment_count;
    framebuf->m_raw_framebuf_data->m_width = width;
    framebuf->m_raw_framebuf_data->m_height = height;

    jeecs::basic::resource<jeecs::graphic::texture>* attachments = nullptr;
    if (attachment_count > 0)
    {
        attachments = new jeecs::basic::resource<jeecs::graphic::texture>[attachment_count];

        for (size_t i = 0; i < attachment_count; ++i)
            attachments[i] = jeecs::graphic::texture::create(width, height,
                jegl_texture::format(attachment_formats[i] | jegl_texture::format::FRAMEBUF));
    }

    framebuf->m_raw_framebuf_data->m_output_attachments = (jegl_frame_buffer::attachment_t*)attachments;

    return framebuf;
}

jegl_resource* jegl_create_uniformbuf(
    size_t binding_place,
    size_t length)
{
    jegl_resource* uniformbuf = _create_resource();
    uniformbuf->m_type = jegl_resource::UNIFORMBUF;
    uniformbuf->m_raw_uniformbuf_data = new jegl_uniform_buffer();
    uniformbuf->m_path = nullptr;

    uniformbuf->m_raw_uniformbuf_data->m_buffer = (uint8_t*)je_mem_alloc(length);
    uniformbuf->m_raw_uniformbuf_data->m_buffer_size = length;
    uniformbuf->m_raw_uniformbuf_data->m_buffer_binding_place = binding_place;

    uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = 0;
    uniformbuf->m_raw_uniformbuf_data->m_update_length = 0;

    return uniformbuf;
}

void jegl_update_uniformbuf(jegl_resource* uniformbuf, const void* buf, size_t update_offset, size_t update_length)
{
    assert(uniformbuf->m_type == jegl_resource::UNIFORMBUF && update_length != 0);

    if (update_length != 0)
    {
        memcpy(uniformbuf->m_raw_uniformbuf_data->m_buffer + update_offset, buf, update_length);
        if (uniformbuf->m_raw_uniformbuf_data->m_update_length != 0)
        {
            size_t new_begin = std::min(uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset, update_offset);
            size_t new_end = std::max(
                uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset
                + uniformbuf->m_raw_uniformbuf_data->m_update_length,
                update_offset + update_length);

            uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = new_begin;
            uniformbuf->m_raw_uniformbuf_data->m_update_length = new_end - new_begin;
        }
        else
        {
            uniformbuf->m_raw_uniformbuf_data->m_update_begin_offset = update_offset;
            uniformbuf->m_raw_uniformbuf_data->m_update_length = update_length;
        }
    }
}

void jegl_bind_shader(jegl_resource* shader)
{
    bool need_init_uvar = jegl_using_resource(shader);
    _current_graphic_thread->m_apis->bind_shader(_current_graphic_thread->m_userdata, shader);

    assert(shader->m_type == jegl_resource::SHADER
        && shader->m_raw_shader_data != nullptr);

    for (size_t i = 0; i < shader->m_raw_shader_data->m_custom_uniforms_count; ++i)
    {
        auto* current_uniform_variable = &shader->m_raw_shader_data->m_custom_uniforms[i];

        if (current_uniform_variable->m_index != jeecs::typing::INVALID_UINT32)
        {
            if (current_uniform_variable->m_updated || need_init_uvar)
            {
                current_uniform_variable->m_updated = false;
                switch (current_uniform_variable->m_uniform_type)
                {
                case jegl_shader::uniform_type::FLOAT:
                    jegl_uniform_float(current_uniform_variable->m_index, current_uniform_variable->x);
                    break;
                case jegl_shader::uniform_type::FLOAT2:
                    jegl_uniform_float2(current_uniform_variable->m_index, current_uniform_variable->x, current_uniform_variable->y);
                    break;
                case jegl_shader::uniform_type::FLOAT3:
                    jegl_uniform_float3(current_uniform_variable->m_index, current_uniform_variable->x, current_uniform_variable->y, current_uniform_variable->z);
                    break;
                case jegl_shader::uniform_type::FLOAT4:
                    jegl_uniform_float4(current_uniform_variable->m_index, current_uniform_variable->x, current_uniform_variable->y, current_uniform_variable->z, current_uniform_variable->w);
                    break;
                case jegl_shader::uniform_type::FLOAT4X4:
                    jegl_uniform_float4x4(current_uniform_variable->m_index, current_uniform_variable->mat4x4);
                    break;
                case jegl_shader::uniform_type::INT:
                case jegl_shader::uniform_type::TEXTURE:
                    jegl_uniform_int(current_uniform_variable->m_index, current_uniform_variable->ix);
                    break;
                case jegl_shader::uniform_type::INT2:
                    jegl_uniform_int2(current_uniform_variable->m_index, current_uniform_variable->ix, current_uniform_variable->iy);
                    break;
                case jegl_shader::uniform_type::INT3:
                    jegl_uniform_int3(current_uniform_variable->m_index, current_uniform_variable->ix, current_uniform_variable->iy, current_uniform_variable->iz);
                    break;
                case jegl_shader::uniform_type::INT4:
                    jegl_uniform_int4(current_uniform_variable->m_index, current_uniform_variable->ix, current_uniform_variable->iy, current_uniform_variable->iz, current_uniform_variable->iw);
                    break;
                default:
                    jeecs::debug::logerr("Unsupport uniform variable type."); break;
                    break;
                }
            }
        }
    }
}

void jegl_bind_uniform_buffer(jegl_resource* uniformbuf)
{
    jegl_using_resource(uniformbuf);
    _current_graphic_thread->m_apis->bind_uniform_buffer(
        _current_graphic_thread->m_userdata, uniformbuf);
}

void jegl_draw_vertex(jegl_resource* vert)
{
    jegl_using_resource(vert);
    _current_graphic_thread->m_apis->draw_vertex(
        _current_graphic_thread->m_userdata, vert);
}

void jegl_clear_framebuffer_color(float color[4])
{
    _current_graphic_thread->m_apis->clear_frame_color(
        _current_graphic_thread->m_userdata, color);
}
void jegl_clear_framebuffer_depth()
{
    _current_graphic_thread->m_apis->clear_frame_depth(
        _current_graphic_thread->m_userdata);
}

void jegl_rend_to_framebuffer(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    if (framebuffer != nullptr)
        jegl_using_resource(framebuffer);

    _current_graphic_thread->m_apis->bind_framebuf(
        _current_graphic_thread->m_userdata, framebuffer, x, y, w, h);
}

void jegl_bind_texture(jegl_resource* texture, size_t pass)
{
    jegl_using_resource(texture);
    _current_graphic_thread->m_apis->bind_texture(
        _current_graphic_thread->m_userdata, texture, pass);
}

void jegl_uniform_int(uint32_t location, int value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::INT, &value);
}

void jegl_uniform_int2(uint32_t location, int x, int y)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    int value[2] = { x,y };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::INT2, &value);
}

void jegl_uniform_int3(uint32_t location, int x, int y, int z)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    int value[3] = { x,y,z };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::INT3, &value);
}

void jegl_uniform_int4(uint32_t location, int x, int y, int z, int w)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    int value[4] = { x,y,z,w };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::INT4, &value);
}

void jegl_uniform_float(uint32_t location, float value)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::FLOAT, &value);
}

void jegl_uniform_float2(uint32_t location, float x, float y)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    float value[] = { x,y };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::FLOAT2, &value);
}

void jegl_uniform_float3(uint32_t location, float x, float y, float z)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    float value[] = { x,y,z };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::FLOAT3, &value);
}

void jegl_uniform_float4(uint32_t location, float x, float y, float z, float w)
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    float value[] = { x,y,z,w };
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::FLOAT4, &value);
}

void jegl_uniform_float4x4(uint32_t location, const float(*mat)[4])
{
    // NOTE: This method designed for using after 'jegl_using_resource'
    _current_graphic_thread->m_apis->set_uniform(
        _current_graphic_thread->m_userdata, location, jegl_shader::FLOAT4X4, mat);
}
