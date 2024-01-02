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

    // ����ȫ�������ģ����ڵ�ǰ����������Ĭ�����ô���ͳһͼ�νӿ�
    game_universe u = game_universe::create_universe();
    jegl_register_sync_thread_callback([](jegl_thread* gthread, void* pgthread) {}, nullptr);

    jegl_interface_config config;
    config.m_display_mode = jegl_interface_config::display_mode::WINDOWED;
    config.m_enable_resize = true;
    config.m_msaa = 0;
    config.m_width = 256;
    config.m_height = 256;
    config.m_fps = 60;
    config.m_title = "Demo";
    config.m_userdata = nullptr;

    jegl_set_host_graphic_api(jegl_using_vulkan110_apis);
    jegl_thread* gthread = jegl_uhost_get_gl_thread(jegl_uhost_get_or_create_for_universe(u.handle(), &config));

    assert(gthread != nullptr);

    // ����һ��ģ�ͣ�������һ������
    basic::resource<graphic::vertex> vertex = graphic::vertex::create(
        jegl_vertex::type::TRIANGLESTRIP,
        {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            -0.5f, 0.5f, 0.0f,
            0.5f, 0.5f, 0.0f
        },
        { 3 });

    // ����һ����ɫ��������ֻ�Ǽ�Ϳ�ɺ�ɫ
    basic::resource<graphic::shader> shader = graphic::shader::create(
        "!/builtin/demo.shader",
        { R"(
import je::shader;

ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);

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
    return v2f{ pos = float4::create(v.vertex, 1.) };
} 
public func frag(_: v2f)
{
    return fout{ color = float4::create(1., 0., 0., 0.5) };
} 
)" }
    );
    basic::resource<graphic::shader> shader_b = graphic::shader::create(
        "!/builtin/demo2.shader",
        { R"(
import je::shader;

ZTEST   (LESS);
ZWRITE  (DISABLE);
BLEND   (SRC_ALPHA, ONE_MINUS_SRC_ALPHA);

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
    return v2f{ pos = float4::create(v.vertex + float3::new(0.2, 0.2, 0.0), 1.) };
} 
public func frag(_: v2f)
{
    return fout{ color = float4::create(0., 0., 1., 0.5) };
} 
)" }
    );
    std::function<void(void)> ff = [&]()
    {
        // ���㶫������ָ������ɫ����Ⱦָ����ģ��
        jegl_rend_to_framebuffer(nullptr, 0, 0, 128, 128);

        float clear_color[4] = { 0.f,0.f,0.f,0.f };
        jegl_clear_framebuffer_color(clear_color);
        jegl_clear_framebuffer_depth();

        jegl_using_resource(shader->resouce());
        jegl_draw_vertex(vertex->resouce());

        jegl_using_resource(shader_b->resouce());
        jegl_draw_vertex(vertex->resouce());
    };

    gthread->_m_frame_rend_work = [](jegl_thread* gthread, void* pgthread)
    {
        (*(std::function<void(void)>*)pgthread)();
    };
    gthread->_m_frame_rend_work_arg = &ff;

    // ��ʼ��ͼ�νӿ�
    jegl_sync_init(gthread, false);

    // ѭ��ͼ�θ��£�ֱ��ͼ�νӿ��˳�
    while (jegl_sync_update(gthread) == jegl_sync_state::JEGL_SYNC_COMPLETE)
    {
        
    }

    // �뿪ѭ��֮�󣬹ر�ͼ��
    jegl_sync_shutdown(gthread, false);

    game_universe::destroy_universe(u);

    je_finish();
}
