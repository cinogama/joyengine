/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#include "jeecs.hpp"

int main(int argc, char** argv)
{
    using namespace jeecs;
    je_init(argc, argv);

    // 创建全局上下文，并在当前上下文上以默认配置创建统一图形接口
    game_universe u = game_universe::create_universe();
    jegl_register_sync_thread_callback([](jegl_thread* gthread, void* pgthread) {}, nullptr);

    jegl_interface_config config;
    config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
    config.m_enable_resize = false;
    config.m_msaa = 0;
    config.m_width = 256;
    config.m_height = 256;
    config.m_fps = 60;
    config.m_title = "Demo";
    config.m_userdata = nullptr;

    jegl_set_host_graphic_api(jegl_using_vulkan110_apis);
    jegl_thread* gthread = jegl_uhost_get_gl_thread(jegl_uhost_get_or_create_for_universe(u.handle(), &config));

    assert(gthread != nullptr);

    // 创建一个模型，这里是一个方块
    basic::resource<graphic::vertex> vertex = graphic::vertex::create(
        jegl_vertex::type::TRIANGLESTRIP,
        {
            -0.5f, 0.5f, 0.0f,      0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, 0.0f,     0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
            0.5f, 0.5f, 0.0f,       1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, 0.0f,      1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        },
        { 3, 2, 3 });

    // 创建一个着色器，这里只是简单涂成红色
    basic::resource<graphic::shader> shader = graphic::shader::create(
        "!/builtin/demo.shader",
        { R"(
import je::shader;

ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex : float3,
};
using v2f = struct {
    pos : float4,
};
using fout = struct {
    color : float4
};

public func vert(v: vin)
{
    return v2f{ pos = float4::create(v.vertex + float3::new(0., 0., 0.), 1.) };
} 
public func frag(_: v2f)
{
    return fout{ color = float4::create(uniform("MainColor", float3::zero), 0.5) };
} 
)" }
    );
    basic::resource<graphic::shader> shader_b = graphic::shader::create(
        "!/builtin/demo2.shader",
        { R"(
import je::shader;

ZTEST   (LESS);
ZWRITE  (ENABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);
CULL    (BACK);

VAO_STRUCT! vin {
    vertex : float3,
    uv     : float2,
    normal : float3,
};
using v2f = struct {
    pos : float4,
    uv  : float2,
};
using fout = struct {
    color : float4
};

public func vert(v: vin)
{
    return v2f{ 
        pos = float4::create(v.vertex + float3::new(0.2, 0.2, -0.1), 1.),
        uv  = v.uv
    };
} 
public func frag(vf: v2f)
{
    let nearest_repeat = sampler2d::create(NEAREST, NEAREST, NEAREST, REPEAT, REPEAT);
    let Main = uniform_texture:<texture2d>("Main", nearest_repeat, 0);
    return fout{ color = uniform("MainColor", float4::zero) + texture(Main, vf.uv) };
} 
)" }
    );
    basic::resource<graphic::texture> texture = graphic::texture::load("!/builtin/icon/Joy.png");

    std::function<void(void)> ff = [&]()
    {
        // 画点东西，用指定的着色器渲染指定的模型
        
        float clear_color[4] = { 1.f,1.f,1.f,1.f };
        jegl_rend_to_framebuffer(nullptr, 0, 0, 0, 0);

        jegl_clear_framebuffer_color(clear_color);
        jegl_clear_framebuffer_depth();

        jegl_using_resource(shader->resouce());
        jegl_draw_vertex(vertex->resouce());

        // jegl_rend_to_framebuffer(nullptr, 0, 0, 0, 0);

        jegl_using_texture(texture->resouce(), 0);

        jegl_using_resource(shader_b->resouce());
        jegl_draw_vertex(vertex->resouce());

    };

    gthread->_m_frame_rend_work = [](jegl_thread* gthread, void* pgthread)
    {
        (*(std::function<void(void)>*)pgthread)();
    };
    gthread->_m_frame_rend_work_arg = &ff;

    // 初始化图形接口
    jegl_sync_init(gthread, false);

    // 循环图形更新，直到图形接口退出
    while (jegl_sync_update(gthread) == jegl_sync_state::JEGL_SYNC_COMPLETE)
    {
        
    }

    // 离开循环之后，关闭图形
    jegl_sync_shutdown(gthread, false);

    game_universe::destroy_universe(u);

    je_finish();
}
