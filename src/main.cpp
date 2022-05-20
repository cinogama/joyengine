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
    rs_init(argc, argv);
    using namespace jeecs;
    using namespace std;

    rs_vm v = rs_create_vm();

    rs_virtual_source("jeecs/shader.rsn", R"(

// JoyEngine Shader (C)Cinogama. 2022.

using float = gchandle;
using vec2 = gchandle;
using vec3 = gchandle;
using vec4 = gchandle;
using mat2x2 = gchandle;
using mat3x3 = gchandle;
using mat4x4 = gchandle;

using vertex_in = gchandle;
using vertex_out = gchandle;
using fragment_out = gchandle;

namespace float
{
    extern("jeecs_shader_float_create")
        func create(var val:real):float;
    extern("jeecs_shader_float_create")
        func create(var val:real):float;
    
    extern("jeecs_shader_float_create")
        func operator + (var a:float, var b:float):float;
}


#macro if{
    lexer->error(
        "The 'if' statement in rscene shader is deprecated.");
}
#macro while{
    lexer->error(
        "The 'while' statement in rscene shader is deprecated.");
}
#macro for{
    lexer->error(
        "The 'for' statement in rscene shader is deprecated.");
}

)", false);

    rs_load_source(v, "example.rsn", R"(
import jeecs.shader;


extern func vert(var in : vertex_in) : vertex_out
{
    return nil;
}

extern func frag(var in : vertex_out) : fragment_out
{
    return nil;
}

)");
    std::cout << rs_get_compile_error(v, RS_NEED_COLOR);
    std::cout << rs_get_compile_warning(v, RS_NEED_COLOR);
    rs_run(v);
    rs_close_vm(v);

    // When abort try clear rs-state and module
    at_quick_exit(rs_finish);
    at_quick_exit(jeecs::enrty::module_leave);

    std::unordered_set<typing::uid_t> x;

    jeecs::enrty::module_entry();

    // goto debug_endl;
    // while (true)
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

debug_endl:

    jeecs::enrty::module_leave();
    rs_finish();
}
