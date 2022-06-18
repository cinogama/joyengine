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
        while (*worldlist)
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
    while (*entity_iter)
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

WO_API wo_api wojeapi_get_entity_chunk_info(wo_vm vm, wo_value args, size_t argc)
{
    char buf[64];
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    sprintf(buf, "[%p:%zuv%zu]", entity->_m_in_chunk, entity->_m_id, entity->_m_version);
    return wo_ret_string(vm, buf);
}

WO_API wo_api wojeapi_is_entity_valid(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    return wo_ret_bool(vm, entity->valid());
}

WO_API wo_api wojeapi_get_all_components_types_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    wo_value out_arr = args + 1;

    auto types = jedbg_get_all_components_from_entity(entity);
    auto typeindex = types;
    while (*typeindex)
        wo_set_pointer(wo_arr_add(out_arr, nullptr), (void*)*(typeindex++));
    je_mem_free(types);

    return wo_ret_val(vm, out_arr);
}

WO_API wo_api wojeapi_get_component_from_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_pointer(vm, je_ecs_world_entity_get_component(entity,
        (const jeecs::typing::type_info*)wo_pointer(args + 1)));
}

WO_API wo_api wojeapi_is_top_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);

    return wo_ret_bool(vm, nullptr == entity->get_component<jeecs::Transform::LocalToParent>());
}

WO_API wo_api wojeapi_is_child_of_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    jeecs::game_entity* parent = (jeecs::game_entity*)wo_pointer(args + 1);

    jeecs::Transform::LocalToParent* l2p = entity->get_component<jeecs::Transform::LocalToParent>();
    jeecs::Transform::ChildAnchor* archor = parent->get_component<jeecs::Transform::ChildAnchor>();

    if (l2p && archor)
    {
        return wo_ret_bool(vm, l2p->parent_uid == archor->anchor_uid);
    }
    return wo_ret_bool(vm, false);
}

// ECS COMPONENT

struct component_member_iter
{
    void* component_addr;
    const jeecs::typing::member_info* iter;

};
WO_API wo_api wojeapi_iter_components_member(wo_vm vm, wo_value args, size_t argc)
{
    void* component_addr = wo_pointer(args + 0);
    const jeecs::typing::type_info* component_type = (const jeecs::typing::type_info*)wo_pointer(args + 1);

    component_member_iter* iter = new component_member_iter;
    iter->component_addr = component_addr;
    iter->iter = component_type->m_member_types;

    return wo_ret_gchandle(vm, iter, nullptr, [](void* ptr) {delete (component_member_iter*)ptr; });
}
WO_API wo_api wojeapi_member_iterator_next(wo_vm vm, wo_value args, size_t argc)
{
    //func next(var self: member_iterator, ref out_name: string, ref out_type: typeinfo, ref out_addr: native_value): bool;
    component_member_iter* iter = (component_member_iter*)wo_pointer(args + 0);
    wo_value out_name = args + 1;
    wo_value out_type = args + 2;
    wo_value out_addr = args + 3;

    if (nullptr == iter->iter)
        return wo_ret_bool(vm, false);

    wo_set_string(out_name, iter->iter->m_member_name);
    wo_set_pointer(out_type, (void*)iter->iter->m_member_type);
    wo_set_handle(out_addr, (wo_handle_t)(iter->iter->m_member_offset + (intptr_t)iter->component_addr));
    iter->iter = iter->iter->m_next_member;

    return wo_ret_bool(vm, true);
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
    func exit(): void;

    using typeinfo = handle;
    namespace typeinfo
    {
        extern("libjoyecs", "wojeapi_type_of")
        func create(var name: string): typeinfo;

        extern("libjoyecs", "wojeapi_type_of")
        func create(var id: int): typeinfo;

        extern("libjoyecs", "wojeapi_type_id")
        func id(var self: typeinfo): int;

        extern("libjoyecs", "wojeapi_type_name")
        func name(var self: typeinfo): string;
    }

    using universe = handle;
    namespace universe
    {
        extern("libjoyecs", "wojeapi_get_edit_universe")
        func current(): universe;

        extern("libjoyecs", "wojeapi_create_world_in_universe")
        func create_world(var self: universe): world;

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

            func top_entity_iter(var self: world): entity::editor::entity_iter
            {
                return entity::editor::entity_iter(self->get_all_entities());
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
            func chunk_info(var self: entity): string;

            extern("libjoyecs", "wojeapi_is_entity_valid")
            func valid(var self: entity): bool;

            func get_components_types(var self: entity): array<typeinfo>
            {
                extern("libjoyecs", "wojeapi_get_all_components_types_from_entity")
                func _get_components_types_from_entity(
                    var entity: entity, var out_result: array<typeinfo>): array<typeinfo>;

                return _get_components_types_from_entity(self, []: array<typeinfo>);
            }

            extern("libjoyecs", "wojeapi_get_component_from_entity")
            func get_component(var self: entity, var type: typeinfo): component;

            extern("libjoyecs", "wojeapi_is_top_entity")
            func is_top(var self: entity): bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            func is_child_of(var self: entity, var parent: entity): bool;

            using entity_iter
            {
                var m_cur_iter = nil: array::iterator<entity>;
                var m_judge_func = nil: bool(entity);
                var m_current_entity = nil: entity;

                var m_all_entity_list = nil: array<entity>;
                var m_not_top_entities = nil: array<entity>;
                var m_outed_entities = nil: array<entity>;

                private func create(var entitys: array<entity>)
                {
                    // walk throw all 'top' entity
                    var self = new();
                    self.m_all_entity_list = entitys;
                    self.m_cur_iter = entitys->iter();
                    self.m_not_top_entities = []: array<entity>;
                    self.m_outed_entities = []: array<entity>;
                    self.m_judge_func = func(var e: entity)
                                        {
                                            var result = e->editor::is_top();
                                            if (!result)
                                                self.m_not_top_entities->add(e);
                                            return result
                                        };
                    return self;
                }
                func childs(var self: entity_iter)
                {
                    var chiter = new();
                    chiter.m_all_entity_list = self.m_all_entity_list;
                    chiter.m_cur_iter = self.m_all_entity_list->iter();
                    chiter.m_outed_entities = self.m_outed_entities;

                    var parent_entity = self.m_current_entity;
                    chiter.m_judge_func = func(var e: entity)
                                          {
                                                return e->is_child_of(parent_entity);
                                          };
                    return chiter;
                }
                func iter(var self: entity_iter)
                {
                    return self;
                }
                func next(var self: entity_iter, ref out_iter: entity_iter,ref out_entity: entity): bool
                {
                    while (self.m_cur_iter->next(0, ref out_entity))
                    {
                        if (self.m_judge_func(out_entity))
                        {
                            self.m_current_entity = out_entity;
                            out_iter = self;
                            self.m_outed_entities->add(out_entity);
                            return true;
                        }
                    }
                    if (self.m_not_top_entities)
                    {
                    continue_find_not_displayed_entity@
                        while (self.m_not_top_entities->len())
                        {
                            var top = self.m_not_top_entities[0];
                            self.m_not_top_entities->remove(0);

                            if (self.m_outed_entities->find(top) == -1)
                            {
                                for (var entity: self.m_all_entity_list)
                                {
                                    if (top != entity && top->is_child_of(entity))
                                        // Parent finded, it's not a orphan entity.
                                        continue continue_find_not_displayed_entity;
                                }

                                // Current entity have jeecs::Transform::LocalToParent,
                                // but it's LocalToParent donot point to any other entity;
                                self.m_current_entity = top;
                                out_iter = self;
                                out_entity = top;
                                self.m_outed_entities->add(top);
                                return true;
                            }
                        }
                    }
                    return false;
                }
            }
        }
    }

    using native_value = handle;
    namespace native_value
    {
        //func int(var self: native_value): int;
        //func float(var self: native_value): real;
        //func double(var self: native_value): real;
    }

    using component = handle;
    namespace component
    {
        namespace editor
        {
            using member_iterator = gchandle;
            namespace member_iterator
            {
                func iter(var self: member_iterator)
                {
                    return ref self;
                }

                extern("libjoyecs", "wojeapi_member_iterator_next")
                func next(var self: member_iterator, ref out_name: string, ref out_type: typeinfo, ref out_addr: native_value): bool;
            }

            extern("libjoyecs", "wojeapi_iter_components_member")
            func iter_member(var self: component, var type: typeinfo) : member_iterator;
        }
    }
}

)";

