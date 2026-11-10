// jeecs_graphic_host.cpp
// 图形线程中间管理器

#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs/ecs.hpp"
#include "jeecs/game.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/io.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs/version.hpp"

// 以下类型原位于 namespace jeecs 内，现作为 C ABI 不透明句柄 je_* 定义于全局作用域。

    struct je_render_branch_t
    {
        JECS_DISABLE_MOVE_AND_COPY(je_render_branch_t);
        
        /* 
        NOTE: 尽管 branch 本身支持双缓冲，但是 branch 中的 ubuffer 只有一份实例，
            在不同的 branch 中共享相同的 ubuffer 没啥意义
        */
        static constexpr uint8_t BRANCH_CHAIN_POOL_SIZE = 1;

        struct allocated_chain_t
        {
            JECS_DISABLE_MOVE_AND_COPY(allocated_chain_t);

            allocated_chain_t() = default;

            struct allocated_command_buffer_t
            {
                je_gl_command_buffer_t* m_command_buffer;
                je_gl_frame_buffer_t* m_target;
                // 不开启 pass 的资源更新命令缓冲：跳过帧也必须提交（只跳过绘制）
                bool m_is_update_command;
            };
            std::vector<allocated_command_buffer_t> m_allocated_command_buffers;
            size_t m_allocated_command_buffers_count;

            int m_priority;
        };

        je_gl_context_t* m_context;

        allocated_chain_t m_chain_buffers[BRANCH_CHAIN_POOL_SIZE];
        uint8_t m_writing_chain_buffer_index;
        allocated_chain_t* m_writing_chain_buffer_p;
        allocated_chain_t* m_rendering_chain_buffer_p;

        je_render_branch_t()
            : m_context(nullptr)
            , m_writing_chain_buffer_index(0)
            , m_writing_chain_buffer_p(&m_chain_buffers[0])
            , m_rendering_chain_buffer_p(
                &m_chain_buffers[(BRANCH_CHAIN_POOL_SIZE - 1) % BRANCH_CHAIN_POOL_SIZE])
        {
            for (auto& chain_buffer : m_chain_buffers)
            {
                chain_buffer.m_allocated_command_buffers_count = 0;
                chain_buffer.m_priority = 0;
            }
        }
        ~je_render_branch_t()
        {
            for (auto& chain_buffer : m_chain_buffers)
            {
                for (auto& command_buffer : chain_buffer.m_allocated_command_buffers)
                    je_graphic_cmd_close(command_buffer.m_command_buffer);
            }
        }

        void new_frame(int priority)
        {
            auto& chain_buffer = get_commiting_chain_buffer();
            chain_buffer.m_allocated_command_buffers_count = 0;
            chain_buffer.m_priority = priority;
        }
        je_gl_command_buffer_t* allocate_new_command_buffer(
            /* OPTIONAL */ je_gl_frame_buffer_t* framebuffer,
            const je_gl_viewport_t* viewport)
        {
            assert(m_context != nullptr);
            assert(viewport != nullptr);

            auto& chain_buffer = get_commiting_chain_buffer();

            if (chain_buffer.m_allocated_command_buffers_count
                >= chain_buffer.m_allocated_command_buffers.size())
            {
                assert(chain_buffer.m_allocated_command_buffers_count
                    == chain_buffer.m_allocated_command_buffers.size());

                chain_buffer.m_allocated_command_buffers.push_back(
                    allocated_chain_t::allocated_command_buffer_t{
                        je_graphic_cmd_create(m_context), nullptr, false });
            }

            auto& entry =
                chain_buffer.m_allocated_command_buffers[
                    chain_buffer.m_allocated_command_buffers_count++];

            je_graphic_cmd_reset(entry.m_command_buffer);
            je_graphic_cmd_begin_pass(entry.m_command_buffer, framebuffer, viewport);
            entry.m_target = framebuffer;
            entry.m_is_update_command = false;

            return entry.m_command_buffer;
        }

        // 申请一个不开启 pass 的资源更新命令缓冲；宿主在跳过帧时也会提交它
        je_gl_command_buffer_t* allocate_new_update_command()
        {
            assert(m_context != nullptr);

            auto& chain_buffer = get_commiting_chain_buffer();

            if (chain_buffer.m_allocated_command_buffers_count
                >= chain_buffer.m_allocated_command_buffers.size())
            {
                assert(chain_buffer.m_allocated_command_buffers_count
                    == chain_buffer.m_allocated_command_buffers.size());

                chain_buffer.m_allocated_command_buffers.push_back(
                    allocated_chain_t::allocated_command_buffer_t{
                        je_graphic_cmd_create(m_context), nullptr, true });
            }

            auto& entry =
                chain_buffer.m_allocated_command_buffers[
                    chain_buffer.m_allocated_command_buffers_count++];

            je_graphic_cmd_reset(entry.m_command_buffer);
            entry.m_target = nullptr;
            entry.m_is_update_command = true;

            return entry.m_command_buffer;
        }

        allocated_chain_t& get_commiting_chain_buffer()
        {
            return *m_writing_chain_buffer_p;
        }
        allocated_chain_t& get_rendering_chain_buffer()
        {
            return *m_rendering_chain_buffer_p;
        }
        void flip_chain_buffer()
        {
            m_rendering_chain_buffer_p = m_writing_chain_buffer_p;
            m_writing_chain_buffer_p = &m_chain_buffers[++m_writing_chain_buffer_index % BRANCH_CHAIN_POOL_SIZE];
        }

        void _commit_frame(
            je_gl_context_t* thread,
            je_gl_update_action_t action,
            bool skip_all_draw)
        {
            auto& chain_buffer = get_rendering_chain_buffer();

            const bool skip_draw =
                skip_all_draw
                || action != je_gl_update_action_t::JE_GL_UPDATE_CONTINUE;

            for (size_t i = 0; i < chain_buffer.m_allocated_command_buffers_count; ++i)
            {
                auto& entry = chain_buffer.m_allocated_command_buffers[i];

                // 资源更新与刷新无关：即使这一帧跳过绘制，也要落实（避免
                // 纹理上传/动态顶点内容在最小化期间丢失）
                if (!entry.m_is_update_command)
                {
                    if (skip_draw)
                        continue;

                    if (entry.m_target == nullptr
                        && action != je_gl_update_action_t::JE_GL_UPDATE_CONTINUE)
                        continue;
                }

                je_graphic_cmd_commit(entry.m_command_buffer);
            }
        }
    };
    struct je_graphic_uhost_t
    {
        JECS_DISABLE_MOVE_AND_COPY(je_graphic_uhost_t);

        je_gl_context_t* glthread;
        jeecs::game_universe universe;

        // 当图形实现请求跳过绘制时，是否跳过全部绘制流程
        // * 如果为true，则跳过全部的绘制流程，反之，则仍然绘制以非屏幕缓冲区的绘制操作
        bool m_skip_all_draw;
        bool m_graphic_frame_update;
        je_gl_command_buffer_t* m_screen_clear_command_buffer;

        std::mutex m_je_RenderBranchs_mx;
        std::vector<je_render_branch_t*> m_je_RenderBranchs;

        static void _update_frame_universe_job(void* host)
        {
            auto* graphic_host = static_cast<je_graphic_uhost_t*>(host);

            if (graphic_host->m_graphic_frame_update)
            {
                constexpr je_gl_update_sync_mode_t SYNC_MODE =
                    je_render_branch_t::BRANCH_CHAIN_POOL_SIZE > 1
                    ? je_gl_update_sync_mode_t::JE_GL_WAIT_LAST_FRAME_END
                    : je_gl_update_sync_mode_t::JE_GL_WAIT_THIS_FRAME_END;

                // ATTENTION: 注意，由于渲染线程在此处翻转，因此任何其他的绘制操作都不能与此同时发生
                //  由于 _update_frame_universe_job 是一个 universe_after_call_once_job，所以此阶段
                //  同时不能有其他绘制相关操作。
                if (!je_graphic_update(graphic_host->glthread, SYNC_MODE, [](void* p)
                    {
                        auto* graphic_host = static_cast<je_graphic_uhost_t*>(p);
                        for (auto& branch : graphic_host->m_je_RenderBranchs)
                            branch->flip_chain_buffer();
                    },
                    host))
                {
                    // 尝试请求退出 universe，在引擎上下文初始化期间，窗口可能在 world 初始化期间被关闭
                    // 此时上下文会额外获得一个生命周期，届时才会正式退出
                    graphic_host->m_graphic_frame_update = false;
                    graphic_host->universe.trim();
                }
            }
        }
        je_render_branch_t* alloc_pipeline()
        {
            je_render_branch_t* pipe = new je_render_branch_t();
            pipe->m_context = glthread;

            std::lock_guard g1(m_je_RenderBranchs_mx);
            m_je_RenderBranchs.push_back(pipe);

            return pipe;
        }
        void free_pipeline(je_render_branch_t* pipe)
        {
            std::lock_guard g1(m_je_RenderBranchs_mx);

            auto fnd = std::find(m_je_RenderBranchs.begin(), m_je_RenderBranchs.end(), pipe);
            assert(fnd != m_je_RenderBranchs.end());

            m_je_RenderBranchs.erase(fnd);

            delete pipe;
        }

        void _frame_rend_impl(je_gl_update_action_t action)
        {
            if (action == je_gl_update_action_t::JE_GL_UPDATE_CONTINUE)
            {
                int window_w = 0, window_h = 0;
                je_io_window_size_get(&window_w, &window_h);

                if (window_w > 0 && window_h > 0)
                {
                    if (m_screen_clear_command_buffer == nullptr)
                        m_screen_clear_command_buffer =
                            je_graphic_cmd_create(glthread);
                    else
                        je_graphic_cmd_reset(m_screen_clear_command_buffer);

                    const je_gl_viewport_t viewport{
                        0, 0,
                        (uint32_t)window_w, (uint32_t)window_h };
                    const float clear_color[4] = { 0.f, 0.f, 0.f, 0.f };

                    je_graphic_cmd_begin_pass(
                        m_screen_clear_command_buffer, nullptr, &viewport);
                    je_graphic_cmd_clear_color(
                        m_screen_clear_command_buffer, 0, clear_color);
                    je_graphic_cmd_clear_depth(
                        m_screen_clear_command_buffer, 1.0f);
                    je_graphic_cmd_commit(m_screen_clear_command_buffer);
                }
            }

            do
            {
                std::lock_guard g1(m_je_RenderBranchs_mx);

                std::stable_sort(
                    m_je_RenderBranchs.begin(),
                    m_je_RenderBranchs.end(),
                    [](je_render_branch_t* a, je_render_branch_t* b)
                    {
                        return a->get_rendering_chain_buffer().m_priority <
                            b->get_rendering_chain_buffer().m_priority;
                    });

                for (auto* gpipe : m_je_RenderBranchs)
                    gpipe->_commit_frame(glthread, action, m_skip_all_draw);

            } while (0);

        }

        je_graphic_uhost_t(jeecs::game_universe _universe, const je_gl_interface_config_t* _config)
            : glthread(nullptr)
            , universe(_universe)
            , m_skip_all_draw(true)
            , m_graphic_frame_update(true)
            , m_screen_clear_command_buffer(nullptr)
        {
            auto host_graphic_api = je_graphic_host_api_get();

            je_gl_interface_config_t config = {};
            if (_config == nullptr)
            {
                config.m_display_mode = JE_GL_DISPLAY_MODE_WINDOWED;
                config.m_width = 640;
                config.m_height = 480;

#define JE_VERSION_WRAP(A, B, C) #A "." #B "." #C

                if (host_graphic_api == je_graphic_using_dx11_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " DirectX 11)";
#if defined(JE_ENABLE_GL330_GAPI) || defined(JE_ENABLE_GLES300_GAPI) || defined(JE_ENABLE_WEBGL20_GAPI)
                else if (host_graphic_api == je_graphic_using_opengl3_apis)
#   ifdef JE_ENABLE_GL330_GAPI
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " OpenGL 3.3)";
#   elif defined(JE_ENABLE_GLES300_GAPI)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " OpenGLES 3.0)";
#   else
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " WebGL 2.0)";
#   endif
#endif
                else if (host_graphic_api == je_graphic_using_vk120_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Vulkan 1.2)";
                else if (host_graphic_api == je_graphic_using_metal_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Metal)";
                else if (host_graphic_api == je_graphic_using_none_apis)
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " None)";
                else
                    config.m_title = "JoyEngineECS(JoyEngine " JE_CORE_VERSION " Custom Graphic API)";

#undef JE_VERSION_WRAP

                config.m_enable_resize = true;
                config.m_fps = 0;            // 使用垂直同步
                config.m_msaa = 4;           // 使用MSAAx4抗锯齿
                config.m_userdata = nullptr; // 用户自定义数据，默认留空
            }
            else
            {
                config = *_config;
            }

            glthread = je_graphic_thread_start(
                config,
                universe.handle(),
                je_graphic_host_api_get(),
                [](je_gl_context_t* glthread, void* ptr, je_gl_update_action_t action)
                {
                    static_cast<je_graphic_uhost_t*>(ptr)->_frame_rend_impl(action);
                },
                this);

            if (glthread != nullptr)
                je_ecs_universe_after_call_once_job_register(
                    universe.handle(),
                    _update_frame_universe_job,
                    this,
                    nullptr);
        }

        ~je_graphic_uhost_t()
        {
            if (m_screen_clear_command_buffer != nullptr)
            {
                je_graphic_cmd_close(m_screen_clear_command_buffer);
                m_screen_clear_command_buffer = nullptr;
            }

            if (glthread)
                je_graphic_thread_terminate(glthread);

            // All branches should be freed before graphic-uhost closed.
            assert(m_je_RenderBranchs.empty());

            je_ecs_universe_after_call_once_job_unregister(
                universe.handle(), _update_frame_universe_job);
        }

        inline static std::shared_mutex _m_instance_universe_host_mx;
        inline static std::unordered_map<je_game_universe_t*, je_graphic_uhost_t*> _m_instance_universe_host;

        static je_graphic_uhost_t* get_default_graphic_pipeline_instance(
            jeecs::game_universe universe, const je_gl_interface_config_t* config)
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
                        je_graphic_thread_reboot(fnd->second->glthread, config);
                }
                return fnd->second;
            }

            auto* instance = new je_graphic_uhost_t(universe, config);
            _m_instance_universe_host[universe.handle()] = instance;
            je_ecs_universe_exit_callback_register(
                universe.handle(),
                [](void* instance)
                {
                    auto* host = (je_graphic_uhost_t*)instance;
                    std::lock_guard g1(_m_instance_universe_host_mx);
                    _m_instance_universe_host.erase(host->universe.handle());
                    delete host;
                }, instance);

            return instance;
        }
    };


je_graphic_uhost_t* je_graphic_uhost_get_or_create_for_universe(
    je_game_universe_t* universe, /* OPTIONAL: nullptr to use default config */ const je_gl_interface_config_t* config)
{
    return je_graphic_uhost_t::get_default_graphic_pipeline_instance(universe, config);
}
void je_graphic_uhost_set_skip_behavior(je_graphic_uhost_t* host, bool skip_all_draw)
{
    host->m_skip_all_draw = skip_all_draw;
}
/* OPTIONAL: If graphic thread failed to start up */ je_gl_context_t* je_graphic_uhost_get_context(je_graphic_uhost_t* host)
{
    return host->glthread;
}
je_render_branch_t* je_graphic_uhost_alloc_branch(je_graphic_uhost_t* host)
{
    return host->alloc_pipeline();
}
void je_graphic_uhost_free_branch(je_graphic_uhost_t* host, je_render_branch_t* free_branch)
{
    return host->free_pipeline(free_branch);
}
je_gl_command_buffer_t* je_graphic_branch_new_command_buffer(
    je_render_branch_t* branch,
    /* OPTIONAL: nullptr means screen framebuffer */ je_gl_frame_buffer_t* framebuffer,
    const je_gl_viewport_t* viewport)
{
    return branch->allocate_new_command_buffer(framebuffer, viewport);
}
je_gl_command_buffer_t* je_graphic_branch_new_update_command(
    je_render_branch_t* branch)
{
    return branch->allocate_new_update_command();
}
void je_graphic_branch_new_frame(je_render_branch_t* branch, int priority)
{
    branch->new_frame(priority);
}
