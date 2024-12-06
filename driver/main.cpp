/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs.hpp"

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

int main(int argc, char** argv)
{
    using namespace jeecs;
    je_init(argc, argv);

    // 创建全局上下文，并在当前上下文上以默认配置创建统一图形接口
    game_universe u = game_universe::create_universe();
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

    jegl_set_host_graphic_api(jegl_using_vk130_apis);
    jeecs::graphic_uhost* uhost = jegl_uhost_get_or_create_for_universe(u.handle(), &config);
    jegl_context* gthread = jegl_uhost_get_context(uhost);

    assert(gthread != nullptr);

    // 创建一个模型，这里是一个方块
    basic::resource<graphic::vertex> vertex = graphic::vertex::load(
        gthread, "!/builtin/model/dancing_vampire.dae");

    // 创建一个纹理
    basic::resource<graphic::texture> texture = graphic::texture::load(
        gthread, "!/builtin/model/Vampire_diffuse.png");

    // 创建一个着色器
    basic::resource<graphic::shader> shader = graphic::shader::load(
        gthread, "!/builtin/shader/UnlitBoneTest.shader");

    auto* branch = jegl_uhost_alloc_branch(uhost);

    // 初始化图形接口
    jegl_sync_init(gthread, false);

    struct BonesData
    {
        float bones[1024][16];
    };
    struct BoneUbo
    {
        BonesData data;
    };

    BoneUbo bone_ubo = {};

    basic::resource<graphic::uniformbuffer> bone_ubo_buffer = 
        graphic::uniformbuffer::create(6, sizeof(bone_ubo));

    // 循环图形更新，直到图形接口退出
    do
    {
        jegl_branch_new_frame(branch, 0);

        auto* rchain = jegl_branch_new_chain(branch, nullptr, 0, 0, 0, 0);

        const float clearcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        jegl_rchain_clear_color_buffer(rchain, clearcolor);
        jegl_rchain_clear_depth_buffer(rchain);

        jegl_rchain_bind_uniform_buffer(rchain, bone_ubo_buffer->resouce());

        shader->set_uniform("factor", (float)(sin(je_clock_time()) + 1.) / 2.f);

        auto texs = jegl_rchain_allocate_texture_group(rchain);
        jegl_rchain_bind_texture(rchain, texs, 0, texture->resouce());

        auto* act = jegl_rchain_draw(rchain, shader->resouce(), vertex->resouce(), texs);
       

    } while (jegl_sync_update(gthread) == jegl_sync_state::JEGL_SYNC_COMPLETE);

    // 离开循环之后，关闭branch，然后再宣告图形接口工作结束
    // NOTE: Branch 被要求在 uhost 释放之前释放，而 uhost 释放会等待
    //       图形线程完成结束，因此此处为了保证正确释放资源，需要在
    //       jegl_sync_shutdown 之前调用 jegl_uhost_free_branch
    jegl_uhost_free_branch(uhost, branch);
    jegl_sync_shutdown(gthread, false);

    game_universe::destroy_universe(u);

    je_finish();

}
