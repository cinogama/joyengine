#define JE_IMPL
#include "jeecs.hpp"

#include "jeecs_editor.hpp"

const char* jeecs_editor_api_path = "je/editor.wo";
const char* jeecs_editor_api_src = R"(
// EDITOR API ONLY USED IN EDITOR
// P.S. I seem to be talking nonsense...

import woo.std;

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

        func top_entitys(var self:world)
        {
            return entity::entity_iter(self->get_all_entity(), func(var e:entity){return e->is_top()});
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

        extern("libjoyecs", "je_editor_destroy_entity")
        func destroy(var self:entity) :void;

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

        extern("libjoyecs", "je_editor_entity_is_child")
        func is_child(var parent_self:entity, var child_or_not:entity):bool;

        extern("libjoyecs", "je_editor_entity_is_top")
        func is_top(var parent_self:entity):bool;

        func select_all_child(var self:entity, var childs:array<entity>)
        {
            var result = []:array<entity>;
            for (var entity : childs)
                if (self->is_child(entity))
                    result->add(entity);
            return result;
        }

        using entity_iter
        {
            var m_all_entity_list = []  : array<entity>;
            var m_curretn_iter = nil    : array::iterator<entity>;
            var m_check_func = nil      : bool(entity);
            var m_current_entity = nil  : entity;
        
            func create(var entities:array<entity>, var judge:bool(entity))
            {
                var self = new();
                self.m_all_entity_list = entities;
                self.m_curretn_iter = entities->iter();
                self.m_check_func = judge;
                return self;
            }

            func iter(var self:entity_iter)
            {
                return self;
            }
        
            func next(var self:entity_iter, ref out_iter:entity_iter, ref out_entity:entity):bool
            {
                while (self.m_curretn_iter->next(0 /*dont need index*/, ref out_entity))
                {
                    if (self.m_check_func(out_entity))
                    {
                        self.m_current_entity = out_entity;
                        out_iter = self;
                        return true;
                    }
                }
                return false;
            }

            func childs(var self:entity_iter)
            {
                var current_entity = self.m_current_entity;
                return entity_iter(self.m_all_entity_list, 
                                    func(var e:entity)
                                    {
                                        if (current_entity->is_child(e)) return true;
                                        return false;
                                    });
            }
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

WO_API wo_api je_editor_get_editor_universe(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm, _universe_address.handle());
}

WO_API wo_api je_editor_exit_enging(wo_vm vm, wo_value args, size_t argc)
{
    _universe_address.stop();
    return wo_ret_nil(vm);
}