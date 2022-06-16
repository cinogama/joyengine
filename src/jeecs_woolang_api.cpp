#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

// ECS UNIVERSE
WO_API wo_api wojeapi_get_edit_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = jedbg_get_editor_universe();
    if (!universe)
        wo_halt("failed to get editor universe.");
    return wo_ret_pointer(vm, universe);
}

WO_API wo_api wojeapi_create_world_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_pointer(vm,
        jeecs::game_universe(wo_pointer(args + 0)).create_world().handle());
}

WO_API wo_api wojeapi_get_shared_system_attached_world(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 1);
    return wo_ret_pointer(vm, jedbg_get_shared_system_attached_world(wo_pointer(args + 0), type));
}

WO_API wo_api wojeapi_get_all_worlds_in_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = wo_pointer(args + 0);
    wo_value out_array = args + 1;

    auto result = jedbg_get_all_worlds_in_universe(universe);
    {
        auto worldlist = result;
        while (worldlist)
            wo_set_pointer(wo_arr_add(out_array, nullptr), *(worldlist++));
    }
    je_mem_free(result);
    return wo_ret_nil(vm);
}

// ECS WORLD
WO_API wo_api wojeapi_close_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world(wo_pointer(args + 0)).close();
    return wo_ret_nil(vm);
}

WO_API wo_api wojeapi_get_world_name(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, jedbg_get_world_name(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_set_world_name(wo_vm vm, wo_value args, size_t argc)
{
    jedbg_set_world_name(wo_pointer(args + 0), wo_string(args + 1));
    return wo_ret_nil(vm);
}

WO_API wo_api wojeapi_attach_shared_system_to_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    gworld.attach_shared_system((const jeecs::typing::type_info*)wo_pointer(args + 1));
    return wo_ret_nil(vm);
}

WO_API wo_api wojeapi_get_all_entities_from_world(wo_vm vm, wo_value args, size_t argc)
{
    wo_value out_arr = args + 1;

    auto entities = jedbg_get_all_entities_in_world(wo_pointer(args + 0));
    auto entity_iter = entities;
    while (entity_iter)
    {
        wo_set_gchandle(wo_arr_add(out_arr, nullptr), *(entity_iter++), nullptr,
            [](void* entity_ptr) {
                jedbg_free_entity((jeecs::game_entity*)entity_ptr);
            });
    }
    je_mem_free(entities);

    return wo_ret_val(vm, out_arr);
}

// ECS ENTITY
WO_API wo_api wojeapi_get_entity_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_string(vm, entity->name().c_str());
}

WO_API wo_api wojeapi_set_entity_name(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_string(vm, entity->name(wo_string(args + 1)).c_str());
}

// ECS OTHER
WO_API wo_api wojeapi_exit(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_universe(jedbg_get_editor_universe()).stop();
    return wo_ret_nil(vm);
}

// ECS TYPEINFO
WO_API wo_api wojeapi_type_of(wo_vm vm, wo_value args, size_t argc)
{
    if (wo_valuetype(args + 0) == WO_INTEGER_TYPE)
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of((jeecs::typing::typeid_t)wo_int(args + 0)));
    else //if (wo_valuetype(args + 0) == WO_STRING_TYPE)
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of(wo_string(args + 0)));
}

WO_API wo_api wojeapi_type_id(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_int(vm, type->m_id);
}

WO_API wo_api wojeapi_type_name(wo_vm vm, wo_value args, size_t argc)
{
    const jeecs::typing::type_info* type = (const jeecs::typing::type_info*)wo_pointer(args + 0);
    return wo_ret_string(vm, type->m_typename);
}

const char* jeecs_woolang_api_path = "je.wo";
const char* jeecs_woolang_api_src = R"(
import woo.std;
namespace je
{
    extern("libjoyecs", "wojeapi_exit")
    func exit():void;

    using typeinfo = handle;
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_type_of")
        func create(var name: string):typeinfo;

        extern("libjoyecs", "wojeapi_type_of")
        func create(var id: int):typeinfo;

        extern("libjoyecs", "wojeapi_type_id")
        func id(var self: typeinfo) : int;

        extern("libjoyecs", "wojeapi_type_name")
        func name(var self: typeinfo) : string;
    }

    using universe = handle;
    namespace universe
    {
        extern("libjoyecs", "wojeapi_get_edit_universe")
        func current() : universe;

        extern("libjoyecs", "wojeapi_create_world_in_universe")
        func create_world(var self: universe) : world;

        namespace editor
        {
            func worlds_list(var self: universe)
            {
                extern("libjoyecs", "wojeapi_get_all_worlds_in_universe")
                func _get_all_worlds(var universe:universe, var out_arrs:array<world>) : void;

                var result = []:array<world>;
                _get_all_worlds(self, result);

                return result;
            }

            extern("libjoyecs", "wojeapi_get_shared_system_attached_world")
            func get_shared_system_attached_world(var self:universe, var systype:typeinfo): world;
        }
    }

    using world = handle;
    namespace world
    {
        extern("libjoyecs", "wojeapi_close_world")
        func close(var self: world) : void;

        extern("libjoyecs", "wojeapi_attach_shared_system_to_world")
        func attach_shared_system(var self: world, var systype: typeinfo): void;

        func rend(var self: world)
        {
            // ATTENTION: Built-in components or systems's typeinfo will not unregister
            //            so I can let them static here, but you should pay attention to
            //            the life-cycle of custom type / woolang vm.
            const static var graphic_typeinfo = typeinfo("jeecs::DefaultGraphicPipelineSystem");
            return self->attach_shared_system(graphic_typeinfo);
        } 

        func rend(): world
        {
            const static var graphic_typeinfo = typeinfo("jeecs::DefaultGraphicPipelineSystem");
            return universe::current()->editor::get_shared_system_attached_world(graphic_typeinfo);
        }
    
        extern("libjoyecs", "wojeapi_close_world")
        func close(var self: world): void;

        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_world_name")
            func name(var self: world): string;

            extern("libjoyecs", "wojeapi_set_world_name")
            func name(var self: world, var _name: string): void;

            func get_all_entities(var self: world): array<entity>
            {
                extern("libjoyecs", "wojeapi_get_all_entities_from_world")
                func _get_all_entities_from_world(var world: world, var out_result: array<entity>): array<entity>;

                var result = []: array<entity>;
                return _get_all_entities_from_world(self, result);
            }
        }
    }
    
    using entity = gchandle;
    namespace entity
    {
        namespace editor
        {
            extern("libjoyecs", "wojeapi_get_entity_name")
            func name(var self: entity): string;

            extern("libjoyecs", "wojeapi_set_entity_name")
            func name(var self: entity, var name: string): string;

            extern("libjoyecs", "wojeapi_get_entity_chunk_info")
            func chunk_info(var self: srting): string;
        }
    }
}
/*
namespace je
    namespace editor
{
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
                while (self.m_curretn_iter->next(0, ref out_entity))
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

        func get_all_components(var self : entity)
        {
            extern("libjoyecs", "je_editor_get_all_components_from_entity")
            func _get_all_components_from_entity(var e:entity, var out_result:array<string>):void;
            
            var result = []:array<string>;
            _get_all_components_from_entity(self, result);
            return result;
        }

        extern("libjoyecs", "je_editor_check_entity_is_valid")
        func valid(var self : entity):bool;
    }
}*/

)";

