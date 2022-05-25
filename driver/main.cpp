/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>

int main(int argc, char** argv)
{
    je_init(argc, argv);
    at_quick_exit(je_finish);

    using namespace jeecs;
    using namespace std;

    rs_vm v = rs_create_vm();
    rs_load_source(v, "x.rsn", R"(
import rscene.std;
import je.shader;

//var module_trans_mat    = uniform:<float4x4>("je_modle_trans");
//var camera_trans_mat    = uniform_block:<float4x4>("je_camera_trans");
//var project_mat         = uniform_block:<float4x4>("je_project_trans");
//var mvp_mat             = uniform_block:<float4x4>("je_mvp_mat");

func vert(var vdata : vertex_in)
{
    var iposition   = vdata->in:<float3>(0);
    var ionormal    = vdata->in:<float3>(1);
    var iuv         = vdata->in:<float2>(2);

    var oposition = float4(iposition->yzx(), float(1));
    var onormal   = ionormal->zyx();
    var ouv       = iuv->yx();

    return vertex_out(oposition, onormal, ouv);
}
func frag(var fdata : fragment_in)
{
    return fragment_out(float4(0, 0, 0, 1));
}

vert(vertex_in());

std::panic("...");

)");
    std::cout << rs_get_compile_error(v, RS_NEED_COLOR) << std::endl;
    rs_run(v);
    rs_close_vm(v);


    jeecs::enrty::module_entry();
    at_quick_exit(jeecs::enrty::module_leave);

    if(0)
    {
        game_universe universe = game_universe::create_universe();
        universe.add_shared_system(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"));

        game_world world = universe.create_world();
        world.add_system(typing::type_info::of("jeecs::TranslationUpdatingSystem"));

        world.attach_shared_system(typing::type_info::of("jeecs::DefaultGraphicPipelineSystem"));

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
            Renderer::Material,
            Renderer::Shape>();

        universe.wait();
        game_universe::destroy_universe(universe);
    }

    je_clock_sleep_for(1);
    je_finish();
}
