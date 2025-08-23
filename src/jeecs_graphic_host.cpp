// jeecs_graphic_host.cpp
// 图形线程中间管理器

#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

namespace jeecs
{
    struct rendchain_branch
    {
        JECS_DISABLE_MOVE_AND_COPY(rendchain_branch);

        static constexpr uint8_t BRANCH_CHAIN_POOL_SIZE = 2;

        struct allocated_chain_t
        {
            JECS_DISABLE_MOVE_AND_COPY(allocated_chain_t);

            allocated_chain_t() = default;

            std::vector<jegl_rendchain *> m_allocated_chains;
            size_t m_allocated_chains_count;
            int m_priority;
        };

        allocated_chain_t m_chain_buffers[BRANCH_CHAIN_POOL_SIZE];
        uint8_t m_writing_chain_buffer_index;
        allocated_chain_t *m_writing_chain_buffer_p;
        allocated_chain_t *m_rendering_chain_buffer_p;

        rendchain_branch()
            : m_writing_chain_buffer_index(0), m_writing_chain_buffer_p(&m_chain_buffers[0]), m_rendering_chain_buffer_p(&m_chain_buffers[(BRANCH_CHAIN_POOL_SIZE - 1) % BRANCH_CHAIN_POOL_SIZE])
        {
            for (auto &chain_buffer : m_chain_buffers)
            {
                chain_buffer.m_allocated_chains_count = 0;
                chain_buffer.m_priority = 0;
            }
        }
        ~rendchain_branch()
        {
            for (auto &chain_buffer : m_chain_buffers)
                for (auto *chain : chain_buffer.m_allocated_chains)
                    jegl_rchain_close(chain);
        }

        void new_frame(int priority)
        {
            auto &chain_buffer = get_commiting_chain_buffer();
            chain_buffer.m_allocated_chains_count = 0;
            chain_buffer.m_priority = priority;
        }
        jegl_rendchain *allocate_new_chain(jegl_resource *framebuffer, size_t x, size_t y, size_t w, size_t h)
        {
            auto &chain_buffer = get_commiting_chain_buffer();

            if (chain_buffer.m_allocated_chains_count >= chain_buffer.m_allocated_chains.size())
            {
                assert(chain_buffer.m_allocated_chains_count == chain_buffer.m_allocated_chains.size());
                chain_buffer.m_allocated_chains.push_back(jegl_rchain_create());
            }
            auto *rchain = chain_buffer.m_allocated_chains[chain_buffer.m_allocated_chains_count++];
            jegl_rchain_begin(rchain, framebuffer, x, y, w, h);
            return rchain;
        }

        allocated_chain_t &get_commiting_chain_buffer()
        {
            return *m_writing_chain_buffer_p;
        }
        allocated_chain_t &get_rendering_chain_buffer()
        {
            return *m_rendering_chain_buffer_p;
        }
        void flip_chain_buffer()
        {
            m_rendering_chain_buffer_p = m_writing_chain_buffer_p;
            m_writing_chain_buffer_p = &m_chain_buffers[++m_writing_chain_buffer_index % BRANCH_CHAIN_POOL_SIZE];
        }

        void _commit_frame(jegl_context *thread, jegl_update_action action)
        {
            auto &chain_buffer = get_rendering_chain_buffer();

            assert(chain_buffer.m_allocated_chains_count <= chain_buffer.m_allocated_chains.size());

            auto chain_iter = chain_buffer.m_allocated_chains.begin();
            const auto chain_iter_end =
                chain_buffer.m_allocated_chains.begin() + chain_buffer.m_allocated_chains_count;

            while (chain_iter != chain_iter_end)
            {
                auto *rend_job_chain = *(chain_iter++);
                if (action != jegl_update_action::JEGL_UPDATE_CONTINUE && jegl_rchain_get_target_framebuf(rend_job_chain) == nullptr)
                    continue;

                jegl_rchain_commit(rend_job_chain, thread);
            }
        }
    };
    struct graphic_uhost
    {
        JECS_DISABLE_MOVE_AND_COPY(graphic_uhost);

        jegl_context *glthread = nullptr;
        jeecs::game_universe universe;

        // 当图形实现请求跳过绘制时，是否跳过全部绘制流程
        // * 如果为true，则跳过全部的绘制流程，反之，则仍然绘制以非屏幕缓冲区的绘制操作
        bool m_skip_all_draw;

        std::mutex m_rendchain_branchs_mx;
        std::vector<rendchain_branch *> m_rendchain_branchs;

        static void _update_frame_universe_job(void *host)
        {
            auto *graphic_host = std::launder(reinterpret_cast<graphic_uhost *>(host));

            constexpr jegl_update_sync_mode SYNC_MODE =
                rendchain_branch::BRANCH_CHAIN_POOL_SIZE > 1
                    ? jegl_update_sync_mode::JEGL_WAIT_LAST_FRAME_END
                    : jegl_update_sync_mode::JEGL_WAIT_THIS_FRAME_END;

            // ATTENTION: 注意，由于渲染线程在此处翻转，因此任何其他的绘制操作都不能与此同时发生
            //  由于 _update_frame_universe_job 是一个 universe_after_call_once_job，所以此阶段
            //  同时不能有其他绘制相关操作。
            if (!jegl_update(graphic_host->glthread, SYNC_MODE, [](void *p)
                             {
                                 auto *graphic_host = std::launder(reinterpret_cast<graphic_uhost *>(p));
                                 for (auto &branch : graphic_host->m_rendchain_branchs)
                                     branch->flip_chain_buffer();
                             },
                             host))
            {
                graphic_host->universe.stop();
            }
        }
        rendchain_branch *alloc_pipeline()
        {
            rendchain_branch *pipe = new rendchain_branch();

            std::lock_guard g1(m_rendchain_branchs_mx);
            m_rendchain_branchs.push_back(pipe);

            return pipe;
        }
        void free_pipeline(rendchain_branch *pipe)
        {
            std::lock_guard g1(m_rendchain_branchs_mx);

            auto fnd = std::find(m_rendchain_branchs.begin(), m_rendchain_branchs.end(), pipe);
            assert(fnd != m_rendchain_branchs.end());

            m_rendchain_branchs.erase(fnd);

            delete pipe;
        }

        void _frame_rend_impl(jegl_update_action action)
        {
            if (action == jegl_update_action::JEGL_UPDATE_CONTINUE)
            {
                // Clear main frame buffer
                float clearcolor[] = {0.f, 0.f, 0.f, 0.f};
                jegl_clear_framebuffer_color(clearcolor);
                jegl_clear_framebuffer_depth();
            }
            else if (m_skip_all_draw)
                return;

            do
            {
                std::lock_guard g1(m_rendchain_branchs_mx);

                std::stable_sort(m_rendchain_branchs.begin(), m_rendchain_branchs.end(),
                                 [](rendchain_branch *a, rendchain_branch *b)
                                 {
                                     return a->get_rendering_chain_buffer().m_priority <
                                            b->get_rendering_chain_buffer().m_priority;
                                 });

                for (auto *gpipe : m_rendchain_branchs)
                    gpipe->_commit_frame(glthread, action);

            } while (0);

            jegl_rend_to_framebuffer(nullptr, 0, 0, 0, 0);
        }

        graphic_uhost(jeecs::game_universe _universe, const jegl_interface_config *_config)
            : universe(_universe), m_skip_all_draw(true)
        {
            auto host_graphic_api = jegl_get_host_graphic_api();

            jegl_interface_config config = {};
            if (_config == nullptr)
            {
                config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
                config.m_width = 640;
                config.m_height = 480;

#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C

                if (host_graphic_api == jegl_using_dx11_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " DirectX 11)";
#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
                else if (host_graphic_api == jegl_using_opengl3_apis)
#   ifdef JE_ENABLE_GL330_GAPI
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " OpenGL 3.3)";
#   elif defined(JE_ENABLE_GLES300_GAPI)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " OpenGLES 3.0)";
#   else
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " WebGL 2.0)";
#   endif
#endif
                else if (host_graphic_api == jegl_using_vk130_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Vulkan1.3)";
                else if (host_graphic_api == jegl_using_metal_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Metal)";
                else if (host_graphic_api == jegl_using_none_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " None)";
                else
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Custom Graphic API)";

#undef JE_VERSION_WRAP

                config.m_enable_resize = true;
                config.m_fps = 0;            // 使用垂直同步
                config.m_msaa = 4;           // 使用MSAAx4抗锯齿
                config.m_userdata = nullptr; // 用户自定义数据，默认留空，用户可以根据自己需要，
                // 使用 jegl_reboot_graphic_thread 传入设置并应用
            }
            else
            {
                config = *_config;
            }

            glthread = jegl_start_graphic_thread(
                config,
                universe.handle(),
                jegl_get_host_graphic_api(),
                [](jegl_context *glthread, void *ptr, jegl_update_action action)
                {
                    std::launder(reinterpret_cast<graphic_uhost *>(ptr))->_frame_rend_impl(action);
                },
                this);

            if (glthread != nullptr)
                je_ecs_universe_register_after_call_once_job(
                    universe.handle(),
                    _update_frame_universe_job,
                    this,
                    nullptr);
        }

        ~graphic_uhost()
        {
            if (glthread)
                jegl_terminate_graphic_thread(glthread);

            // All branches should be freed before graphic-uhost closed.
            assert(m_rendchain_branchs.empty());

            je_ecs_universe_unregister_after_call_once_job(
                universe.handle(), _update_frame_universe_job);
        }

        inline static std::shared_mutex _m_instance_universe_host_mx;
        inline static std::unordered_map<void *, graphic_uhost *> _m_instance_universe_host;

        static graphic_uhost *get_default_graphic_pipeline_instance(
            game_universe universe, const jegl_interface_config *config)
        {
            assert(universe);
            do
            {
                std::shared_lock sg1(_m_instance_universe_host_mx);
                auto fnd = _m_instance_universe_host.find(universe.handle());

                if (fnd != _m_instance_universe_host.end())
                    return fnd->second;

            } while (0);

            std::lock_guard g1(_m_instance_universe_host_mx);
            auto fnd = _m_instance_universe_host.find(universe.handle());

            if (fnd != _m_instance_universe_host.end())
            {
                if (config != nullptr)
                {
                    if (fnd->second->glthread == nullptr)
                        jeecs::debug::logerr("Unable to re-config graphic setting, bad graphic-thread.");
                    else
                        jegl_reboot_graphic_thread(fnd->second->glthread, config);
                }
                return fnd->second;
            }

            auto *instance = new graphic_uhost(universe, config);
            _m_instance_universe_host[universe.handle()] = instance;
            je_ecs_universe_register_exit_callback(universe.handle(), [](void *instance)
                                                   {
                    auto* host = (graphic_uhost*)instance;
                    std::lock_guard g1(_m_instance_universe_host_mx);
                    _m_instance_universe_host.erase(host->universe.handle());
                    delete host; }, instance);

            return instance;
        }
    };
}

jeecs::graphic_uhost *jegl_uhost_get_or_create_for_universe(
    void *universe, const jegl_interface_config *config)
{
    return jeecs::graphic_uhost::get_default_graphic_pipeline_instance(universe, config);
}
void jegl_uhost_set_skip_behavior(jeecs::graphic_uhost *host, bool skip_all_draw)
{
    host->m_skip_all_draw = skip_all_draw;
}
jegl_context *jegl_uhost_get_context(jeecs::graphic_uhost *host)
{
    return host->glthread;
}
jeecs::rendchain_branch *jegl_uhost_alloc_branch(jeecs::graphic_uhost *host)
{
    return host->alloc_pipeline();
}
void jegl_uhost_free_branch(jeecs::graphic_uhost *host, jeecs::rendchain_branch *free_branch)
{
    return host->free_pipeline(free_branch);
}
jegl_rendchain *jegl_branch_new_chain(jeecs::rendchain_branch *branch, jegl_resource *framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    return branch->allocate_new_chain(framebuffer, x, y, w, h);
}
void jegl_branch_new_frame(jeecs::rendchain_branch *branch, int priority)
{
    branch->new_frame(priority);
}
