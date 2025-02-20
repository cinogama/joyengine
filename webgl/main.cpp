#include <emscripten/html5.h>

/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#include "jeecs.hpp"

#include <cmath>
#include <optional>

using namespace jeecs;

std::optional<game_universe> u;
basic::resource<graphic::vertex> g_vertex;
basic::resource<graphic::shader> g_shader;
basic::resource<graphic::uniformbuffer> g_ubo;

graphic_uhost* g_uhost = nullptr;
jegl_context* g_gthread = nullptr;
rendchain_branch* g_branch = nullptr;
uint32_t* g_factor_uniform_location = nullptr;

bool g_interface = false;

void webgl_rend_job_callback()
{
    if (! g_interface)
    {
        g_interface = true;

        // 初始化图形接口
        jegl_sync_init(g_gthread, false);

        // 循环图形更新，直到图形接口退出
        g_factor_uniform_location = 
            g_shader->get_uniform_location_as_builtin("factor");
    }

    jegl_branch_new_frame(g_branch, 0);

    auto* rchain = jegl_branch_new_chain(g_branch, nullptr, 0, 0, 0, 0);
    jegl_rchain_bind_uniform_buffer(rchain, g_ubo->resouce());

    const float clearcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    jegl_rchain_clear_color_buffer(rchain, clearcolor);
    jegl_rchain_clear_depth_buffer(rchain);

    auto* act = jegl_rchain_draw(rchain, g_shader->resouce(), g_vertex->resouce(), SIZE_MAX);
    jegl_rchain_set_builtin_uniform_float(
        act,
        g_factor_uniform_location,
        (float)(sin(je_clock_time()) + 1.) / 2.f);

    if (jegl_sync_update(g_gthread) != jegl_sync_state::JEGL_SYNC_COMPLETE)
    {
        // 离开循环之后，关闭branch，然后再宣告图形接口工作结束
        // NOTE: Branch 被要求在 uhost 释放之前释放，而 uhost 释放会等待
        //       图形线程完成结束，因此此处为了保证正确释放资源，需要在
        //       jegl_sync_shutdown 之前调用 jegl_uhost_free_branch
        jegl_uhost_free_branch(g_uhost, g_branch);
        jegl_sync_shutdown(g_gthread, false);

        game_universe::destroy_universe(u.value());

        je_finish();
        exit(0);
    }
}

int main(int argc, char** argv)
{
    je_init(argc, argv);

    // 创建全局上下文，并在当前上下文上以默认配置创建统一图形接口
    u = game_universe::create_universe();
    jegl_register_sync_thread_callback([](jegl_context* gthread, void* pgthread) {}, nullptr);

    jegl_interface_config config;
    config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
    config.m_enable_resize = true;
    config.m_msaa = 0;
    config.m_width = 512;
    config.m_height = 512;
    config.m_fps = 0;
    config.m_title = "Demo";
    config.m_userdata = nullptr;

    jegl_set_host_graphic_api(jegl_using_opengl3_apis);
    g_uhost = jegl_uhost_get_or_create_for_universe(u.value().handle(), &config);
    g_gthread = jegl_uhost_get_context(g_uhost);

    assert(g_gthread != nullptr);

    // 创建一个模型，这里是一个方块
    const float vertex_data[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f
    };
    g_vertex = graphic::vertex::create(
        jegl_vertex::type::TRIANGLESTRIP,
        vertex_data,
        sizeof(vertex_data),
        { 0, 1, 2, 3 },
        { jegl_vertex::data_layout{jegl_vertex::data_type::FLOAT32, 3} });

    // 创建一个着色器
    g_shader = graphic::shader::create(
        "!/builtin/demo.shader",
        { R"(
import je::shader;

VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

public func vert(v: vin) {
    return v2f{ pos = float4::create(v.vertex, 1.) };
} 
public func frag(_: v2f) {
    let factor = uniform("factor", 1.);
    return fout{ color = float4::create(1., 1., factor, 1.) };
} 
)" }
    );
    g_ubo = graphic::uniformbuffer::create(0, 3 * 16 * 4 + 4 * 4);
    
    g_branch = jegl_uhost_alloc_branch(g_uhost);

    emscripten_set_main_loop(webgl_rend_job_callback, 0, 1);
    return 0;
}
