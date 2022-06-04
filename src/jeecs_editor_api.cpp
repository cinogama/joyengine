#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_editor.hpp"

const char* jeecs_editor_api_path = "je/editor.rsn";
const char* jeecs_editor_api_src = R"(
// EDITOR API ONLY USED IN EDITOR
// P.S. I seem to be talking nonsense...

import rscene.std;

namespace je
    namespace editor
{
    using universe = handle;
    namespace universe
    {
        protected func create() : universe
        {
            extern("libjoyecs", "je_editor_get_editor_universe")
            func _universe():handle;

            var u = _universe();
            if (!u) std::panic("Failed to get edit-universe.");

            return u;
        }
    }

    using world = handle;
    namespace world
    {
        func all_worlds() : array<world>
        {
            extern("libjoyecs", "je_editor_get_alive_worlds")
            func _all_worlds(var universe:universe, var out_arrs:array<world>) : void;

            var result = []:array<world>;
            _all_worlds(universe(), result);
            return result;
        }

        func create()
        {
            extern("libjoyecs", "je_editor_create_world")
            func _create_world_in_universe(var _universe:universe):world;

            return _create_world_in_universe(universe());
        }
            
        // Member functions
        extern("libjoyecs", "je_editor_get_world_name")
        func name(var self:world):string;

        extern("libjoyecs", "je_editor_set_world_name")
        func name(var self:world, var newname:string):string;

        extern("libjoyecs", "je_editor_remove_world")
        func close(var self:world):void;

        extern("libjoyecs", "je_editor_attach_shared_system_to_world")
        func attach_shared_system(var self:world, var system_name:string):void;
    }
}

)";

jeecs::game_universe _universe_address = nullptr;

void jedbg_set_editor_universe(void* universe_handle)
{
    jeecs::debug::log_info("Editor will work at universe: %p", universe_handle);
    _universe_address = jeecs::game_universe(universe_handle);
}

RS_API rs_api je_editor_get_editor_universe(rs_vm vm, rs_value args, size_t argc)
{
    return rs_ret_pointer(vm, _universe_address.handle());
}