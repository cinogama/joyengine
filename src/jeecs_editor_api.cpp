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
    extern("libjoyecs", "je_editor_exit_enging")
    func exit():void;

    using universe = handle;
    using world = handle;
    using entity = gchandle;

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
        
        func get_shared_system_attached_world(var sys:string):world
        {
            extern("libjoyecs", "je_editor_get_shared_system_attached_system")
            func _get_shared_system_attached_world(var _universe:universe, var sys:string):world;

            return _get_shared_system_attached_world(universe(), sys);
        }

        func rend():world
        {
            return get_shared_system_attached_world("jeecs::DefaultGraphicPipelineSystem");
        }
        func rend(var self:world):world
        {
            self->attach_shared_system("jeecs::DefaultGraphicPipelineSystem");
            return self;
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

        func get_all_entity(var self:world)
        {
            extern("libjoyecs", "je_editor_get_all_entity_from_world")
            func _get_all_entity(var self:world, var out_result:array<entity>):void;
        
            var result = []:array<entity>;
            _get_all_entity(self, result);
            return result;
        }
        
    }
    namespace entity
    {
        extern("libjoyecs", "je_editor_try_get_entity_name")
        func name(var self:entity) :string;

        extern("libjoyecs", "je_editor_set_entity_name")
        func name(var self:entity, var name:string) :string;

        extern("libjoyecs", "je_editor_get_entity_chunk")
        func get_chunk(var self:entity) :handle;

        extern("libjoyecs", "je_editor_get_entity_chunk_id")
        func get_id(var self:entity) :handle;

        extern("libjoyecs", "je_editor_get_entity_version")
        func get_version(var self:entity) :handle;

        func get_desc(var self:entity)
        {
            return [self->get_chunk(), self->get_id(), self->get_version()];
        }
        
        func get_desc_str(var self:entity)
        {
            var result = self->get_desc();
            return F"[{result[0]}:{result[1]}v{result[2]}]";
        }
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

RS_API rs_api je_editor_exit_enging(rs_vm vm, rs_value args, size_t argc)
{
    _universe_address.stop();
    return rs_ret_nil(vm);
}