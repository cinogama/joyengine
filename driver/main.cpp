/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    je_init(argc, argv);
    atexit(je_finish);
    at_quick_exit(je_finish);

    using namespace jeecs;
    using namespace std;

    jeecs::enrty::module_entry();
    atexit(jeecs::enrty::module_leave);
    at_quick_exit(jeecs::enrty::module_leave);

    if (1)
    {
        game_universe universe = game_universe::create_universe();
        jedbg_set_editor_universe(universe.handle());

        universe.add_shared_system(typing::type_info::of("Graphic::DefaultGraphicPipelineSystem"));

        game_world world = universe.create_world();
        world.add_system(typing::type_info::of("Translation::TranslationUpdatingSystem"));

        world.attach_shared_system(typing::type_info::of("Graphic::DefaultGraphicPipelineSystem"));

        auto entity = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::ChildAnchor,
            Transform::LocalToWorld,
            Transform::Translation>();

        auto entity2 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::LocalToParent,
            Transform::Translation,
            Renderer::Rendqueue,
            Renderer::Shape,
            Renderer::Shaders,
            Renderer::Textures>();

        entity.get_component<Transform::LocalRotation>()->rot = jeecs::math::quat(0, 0, 0);
        entity2.get_component<Transform::LocalPosition>()->pos = jeecs::math::vec3(0, 0, 0);
        entity2.get_component<Transform::LocalToParent>()->parent_uid =
            entity.get_component<Transform::ChildAnchor>()->anchor_uid;
        entity2.get_component<Renderer::Rendqueue>()->rend_queue = 3000;

        jeecs::basic::resource<jeecs::graphic::texture> tex = new graphic::texture(2, 2, jegl_texture::texture_format::RGBA);
        tex->pix(0, 0).set(math::vec4(0, 0, 0, 1));
        tex->pix(1, 0).set(math::vec4(1, 0, 0, 1));
        tex->pix(0, 1).set(math::vec4(0, 1, 0, 1));
        tex->pix(1, 1).set(math::vec4(0, 0, 1, 1));
        entity2.get_component<Renderer::Textures>()->bind_texture(0, tex);
        entity2.get_component<Renderer::Shaders>()->shaders.push_back(
            new graphic::shader("test.shader", R"(
// Default shader
import je.shader;

using VAO_STRUCT vin = struct {
    vertex : float3,
    uv     : float2,
};

using v2f = struct {
    pos : float4,
    uv  : float2,
};

using fout = struct {
    color : float4
};

let example_tex = uniform:<texture2d>("example_tex");

func vert(vdata: vin)
{
    return v2f{
        pos = je_vp * je_m * float4(vdata.vertex, 1.), 
        uv  = vdata.uv 
    };
}

func frag(fdata: v2f)
{
    return fout{ 
        color = alphatest(texture(example_tex, fdata.uv))
    };
}
)")
);
        auto entity2_1 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalScale,
            Transform::LocalToParent,
            Transform::Translation,
            Renderer::Rendqueue,
            Renderer::Shape,
            Renderer::Shaders,
            Renderer::Textures>();
        entity2_1.get_component<Transform::LocalToParent>()->parent_uid =
            entity.get_component<Transform::ChildAnchor>()->anchor_uid;
        entity2_1.get_component<Renderer::Rendqueue>()->rend_queue = 1000;
        entity2_1.get_component<Transform::LocalPosition>()->pos = jeecs::math::vec3(0.5, 0, 1);
        entity2_1.get_component<Renderer::Textures>()->bind_texture(0, tex);
        entity2_1.get_component<Renderer::Shaders>()->shaders.push_back(
            new graphic::shader("test2.shader", R"(
// Default shader
import je.shader;

using VAO_STRUCT vin = struct {
    vertex : float3,
    uv     : float2,
};

using v2f = struct {
    pos : float4,
    uv  : float2,
};

using fout = struct {
    color : float4
};

let example_tex = uniform:<texture2d>("example_tex");

func vert(vdata: vin)
{
    return v2f{
        pos = je_vp * je_m * float4(vdata.vertex, 1.), 
        uv  = vdata.uv 
    };
}

func frag(fdata: v2f)
{
    return fout{ 
        color = alphatest(texture(example_tex, fdata.uv))
    };
}
)")
);

        auto entity3 = world.add_entity<
            Transform::LocalPosition,
            Transform::LocalRotation,
            Transform::LocalToWorld,
            Transform::Translation,
            Camera::Clip,
            Camera::PerspectiveProjection,
            Camera::Projection,
            Camera::Viewport>();

        entity3.get_component<Transform::LocalRotation>()->rot = math::quat(0, 0, 15);
        entity3.get_component<Transform::LocalPosition>()->pos = math::vec3(0, 0, -10);

        universe.wait();
        game_universe::destroy_universe(universe);
    }

    je_clock_sleep_for(1);
}
