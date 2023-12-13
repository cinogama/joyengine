// jeecs_graphic_host.cpp
// 图形线程中间管理器

#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"

namespace jeecs
{
    struct rendchain_branch
    {
        JECS_DISABLE_MOVE_AND_COPY(rendchain_branch);

        std::vector<jegl_rendchain*> m_allocated_chains;
        size_t m_allocated_chains_count;
        int    m_priority;

        rendchain_branch()
            : m_allocated_chains_count(0)
            , m_priority(0)
        {
        }
        ~rendchain_branch()
        {
            for (auto* chain : m_allocated_chains)
                jegl_rchain_close(chain);
        }

        void new_frame(int priority)
        {
            m_priority = priority;
            m_allocated_chains_count = 0;
        }
        jegl_rendchain* allocate_new_chain(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
        {
            if (m_allocated_chains_count >= m_allocated_chains.size())
            {
                assert(m_allocated_chains_count == m_allocated_chains.size());
                m_allocated_chains.push_back(jegl_rchain_create());
            }
            auto* rchain = m_allocated_chains[m_allocated_chains_count];
            jegl_rchain_begin(rchain, framebuffer, x, y, w, h);
            ++m_allocated_chains_count;
            return rchain;
        }
        void _commit_frame(jegl_thread* thread)
        {
            for (size_t i = 0; i < m_allocated_chains_count; ++i)
                jegl_rchain_commit(m_allocated_chains[i], thread);
        }
    };
    struct graphic_uhost
    {
        JECS_DISABLE_MOVE_AND_COPY(graphic_uhost);

        jegl_thread* glthread = nullptr;
        jeecs::game_universe universe;

        static void _update_frame_universe_job(void* host)
        {
            auto* graphic_host = (graphic_uhost*)host;
            if (!jegl_update(graphic_host->glthread, jegl_update_sync_mode::JEGL_WAIT_THIS_FRAME_END))
            {
                graphic_host->universe.stop();
            }
        }

        std::mutex m_rendchain_branchs_mx;
        std::vector<rendchain_branch*> m_rendchain_branchs;

        rendchain_branch* alloc_pipeline()
        {
            rendchain_branch* pipe = new rendchain_branch();

            std::lock_guard g1(m_rendchain_branchs_mx);
            m_rendchain_branchs.push_back(pipe);

            return pipe;
        }

        void free_pipeline(rendchain_branch* pipe)
        {
            std::lock_guard g1(m_rendchain_branchs_mx);
            auto fnd = std::find(m_rendchain_branchs.begin(), m_rendchain_branchs.end(), pipe);
            assert(fnd != m_rendchain_branchs.end());
            m_rendchain_branchs.erase(fnd);

            delete pipe;
        }

        void _frame_rend_impl()
        {
            // Clear frame buffer
            float clearcolor[] = { 0.f, 0.f, 0.f, 0.f };
            jegl_clear_framebuffer_color(clearcolor);
            jegl_clear_framebuffer_depth();

            std::lock_guard g1(m_rendchain_branchs_mx);

            std::stable_sort(m_rendchain_branchs.begin(), m_rendchain_branchs.end(),
                [](rendchain_branch* a, rendchain_branch* b)
                {
                    return a->m_priority < b->m_priority;
                });

            for (auto* gpipe : m_rendchain_branchs)
                gpipe->_commit_frame(glthread);

            jegl_rend_to_framebuffer(nullptr, 0, 0, 0, 0);
        }

        graphic_uhost(jeecs::game_universe _universe, const jegl_interface_config* _config)
            : universe(_universe)
        {
            jegl_interface_config config = {};
            if (_config == nullptr)
            {
                config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
                config.m_resolution_mode = jegl_interface_config::resolution_mode::SCALE;
                config.m_width = 640;
                config.m_height = 480;

                config.m_reso_x = 1;
                config.m_reso_y = 1;

                config.m_title = "JoyEngineECS(JoyEngine 4.3)";
                
                config.m_enable_resize = true;
                config.m_fps = 0;               // 使用垂直同步
                config.m_msaa = 4;              // 使用MSAAx4抗锯齿
                config.m_userdata = nullptr;    // 用户自定义数据，默认留空，用户可以根据自己需要，
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
                [](jegl_thread* glthread, void* ptr)
                {
                    ((graphic_uhost*)ptr)->_frame_rend_impl();
                }, this);

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

            je_ecs_universe_unregister_after_call_once_job(
                universe.handle(), _update_frame_universe_job);
        }

        inline static std::shared_mutex _m_instance_universe_host_mx;
        inline static std::unordered_map<void*, graphic_uhost*> _m_instance_universe_host;

        static graphic_uhost* get_default_graphic_pipeline_instance(
            game_universe universe, const jegl_interface_config* config)
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

            auto* instance = new graphic_uhost(universe, config);
            _m_instance_universe_host[universe.handle()] = instance;
            je_ecs_universe_register_exit_callback(universe.handle(),
                [](void* instance)
                {
                    auto* host = (graphic_uhost*)instance;
                    std::lock_guard g1(_m_instance_universe_host_mx);
                    _m_instance_universe_host.erase(host->universe.handle());
                    delete host;
                },
                instance);

            return instance;
        }
    };
}

jeecs::graphic_uhost* jegl_uhost_get_or_create_for_universe(
    void* universe, const jegl_interface_config* config)
{
    return jeecs::graphic_uhost::get_default_graphic_pipeline_instance(universe, config);
}
jegl_thread* jegl_uhost_get_gl_thread(jeecs::graphic_uhost* host)
{
    return host->glthread;
}
jeecs::rendchain_branch* jegl_uhost_alloc_branch(jeecs::graphic_uhost* host)
{
    return host->alloc_pipeline();
}
void jegl_uhost_free_branch(jeecs::graphic_uhost* host, jeecs::rendchain_branch* free_branch)
{
    return host->free_pipeline(free_branch);
}
jegl_rendchain* jegl_branch_new_chain(jeecs::rendchain_branch* branch, jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h)
{
    return branch->allocate_new_chain(framebuffer, x, y, w, h);
}
void jegl_branch_new_frame(jeecs::rendchain_branch* branch, int priority)
{
    branch->new_frame(priority);
}
