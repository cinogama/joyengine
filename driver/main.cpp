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
    config.m_width = 1024;
    config.m_height = 1024;
    config.m_fps = 0;
    config.m_title = "Demo";
    config.m_userdata = nullptr;

    jegl_set_host_graphic_api(jegl_using_vk130_apis);
    jeecs::graphic_uhost* uhost = jegl_uhost_get_or_create_for_universe(u.handle(), &config);
    jegl_context* gthread = jegl_uhost_get_context(uhost);

    assert(gthread != nullptr);

    // 创建一个模型，这里是一个方块
    const float default_shape_quad_data[] = {
        -0.5f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
    };
    basic::resource<graphic::vertex> debug_vertex = graphic::vertex::create(
        jegl_vertex::type::TRIANGLESTRIP,
        default_shape_quad_data,
        sizeof(default_shape_quad_data),
        {
            0, 3, 2, 1
        },
        {
            { jegl_vertex::data_type::FLOAT32, 3 }
        });

    basic::resource<graphic::vertex> vertex = graphic::vertex::load(
        gthread, "!/builtin/model/dancing_vampire.dae");

    // 创建一个纹理
    basic::resource<graphic::texture> texture = graphic::texture::load(
        gthread, "!/builtin/model/Vampire_diffuse.png");

    // 创建一个着色器
    basic::resource<graphic::shader> debug_shader = graphic::shader::load(
        gthread, "!/builtin/shader/UnlitBoneDebug.shader");
    basic::resource<graphic::shader> shader = graphic::shader::load(
        gthread, "!/builtin/shader/UnlitBoneTest.shader");

    auto* branch = jegl_uhost_alloc_branch(uhost);

    // 初始化图形接口
    jegl_sync_init(gthread, false);

    struct BonesData
    {
        float bones_mat4x4[1024][4][4];
    };
    struct BoneUbo
    {
        BonesData data;
    };

    BoneUbo bone_ubo = {};

    for (size_t bidx = 0; bidx < vertex->resouce()->m_raw_vertex_data->m_bone_count; ++bidx)
    {
        memcpy(
            bone_ubo.data.bones_mat4x4[bidx], 
            vertex->resouce()->m_raw_vertex_data->m_bones[bidx]->m_m2b_trans,
            4 * 16);
    }

    basic::resource<graphic::uniformbuffer> bone_ubo_buffer = 
        graphic::uniformbuffer::create(6, sizeof(bone_ubo));

    bone_ubo_buffer->update_buffer(0, sizeof(bone_ubo), &bone_ubo);

    // 循环图形更新，直到图形接口退出
    do
    {
        jegl_branch_new_frame(branch, 0);

        auto* rchain = jegl_branch_new_chain(branch, nullptr, 0, 0, 0, 0);

        const float clearcolor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        jegl_rchain_clear_color_buffer(rchain, clearcolor);
        jegl_rchain_clear_depth_buffer(rchain);

        auto texs = jegl_rchain_allocate_texture_group(rchain);
        jegl_rchain_bind_texture(rchain, texs, 0, texture->resouce());

        auto* main_act = jegl_rchain_draw(rchain, shader->resouce(), vertex->resouce(), texs);
        jegl_rchain_set_uniform_buffer(main_act, bone_ubo_buffer->resouce());
       
        for (size_t boneidx = 0; boneidx < vertex->resouce()->m_raw_vertex_data->m_bone_count; ++boneidx)
        {
            float offset[4];
            const float zero[4] = {0.0, 0.0, 0.0, 1.0};
            math::mat4xvec4(offset, bone_ubo.data.bones_mat4x4[boneidx], zero);

            auto *act = jegl_rchain_draw(rchain, debug_shader->resouce(), debug_vertex->resouce(), SIZE_MAX);
            auto offset_location = debug_shader->get_uniform_location("Offset");
            auto index_location = debug_shader->get_uniform_location("Index");
            if (offset_location != typing::INVALID_UINT32)
            {
                jegl_rchain_set_uniform_float3(
                    act,
                    offset_location,
                    -offset[0], -offset[1], -offset[2]);
            }
            if (index_location != typing::INVALID_UINT32)
            {
                jegl_rchain_set_uniform_int(
                    act,
                    index_location,
                    boneidx);
            }
        }

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
