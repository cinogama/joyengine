#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

// ECS UNIVERSE
WO_API wo_api wojeapi_get_edit_universe(wo_vm vm, wo_value args, size_t argc)
{
    void* universe = jedbg_get_editor_universe();
    if (!universe)
        return wo_ret_halt(vm, "failed to get editor universe.");
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
    return wo_ret_option_ptr(vm, jedbg_get_shared_system_attached_world(wo_pointer(args + 0), type));
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
    return wo_ret_void(vm);
}

// ECS WORLD
WO_API wo_api wojeapi_close_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world(wo_pointer(args + 0)).close();
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_get_world_name(wo_vm vm, wo_value args, size_t argc)
{
    return wo_ret_string(vm, jedbg_get_world_name(wo_pointer(args + 0)));
}

WO_API wo_api wojeapi_set_world_name(wo_vm vm, wo_value args, size_t argc)
{
    jedbg_set_world_name(wo_pointer(args + 0), wo_string(args + 1));
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_attach_shared_system_to_world(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_world gworld = wo_pointer(args + 0);
    gworld.attach_shared_system((const jeecs::typing::type_info*)wo_pointer(args + 1));
    return wo_ret_void(vm);
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
WO_API wo_api wojeapi_close_entity(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::game_entity* entity = (jeecs::game_entity*)wo_pointer(args + 0);
    entity->close();
    return wo_ret_void(vm);
}

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

    return wo_ret_option_ptr(vm, je_ecs_world_entity_get_component(entity,
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
    return wo_ret_void(vm);
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

WO_API wo_api wojeapi_type_basic_type(wo_vm vm, wo_value args, size_t argc)
{
    enum basic_type
    {
        INT, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT
    };
    basic_type type = (basic_type)wo_int(args + 0);

    switch (type)
    {
    case INT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<int>(nullptr));
    case FLOAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<float>(nullptr));
    case FLOAT2:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec2>(nullptr));
    case FLOAT3:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec3>(nullptr));
    case FLOAT4:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::vec4>(nullptr));
    case STRING:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::string>(nullptr));
    case QUAT:
        return wo_ret_pointer(vm, (void*)jeecs::typing::type_info::of<jeecs::math::quat>(nullptr));
    default:
        return wo_ret_panic(vm, "Unknown basic type.");
    }
}

// Native value
WO_API wo_api wojeapi_native_value_int(wo_vm vm, wo_value args, size_t argc)
{
    int* value = (int*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_int(args + 1, *value);
    else
        *value = wo_int(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float(wo_vm vm, wo_value args, size_t argc)
{
    float* value = (float*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, *value);
    else
        *value = wo_float(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float2(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec2* value = (jeecs::math::vec2*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec3* value = (jeecs::math::vec3*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, value->z);
    else
        value->z = wo_float(args + 3);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_float4(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::vec4* value = (jeecs::math::vec4*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, value->x);
    else
        value->x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, value->y);
    else
        value->y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, value->z);
    else
        value->z = wo_float(args + 3);

    if (wo_is_ref(args + 4))
        wo_set_float(args + 4, value->w);
    else
        value->w = wo_float(args + 4);

    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_je_string(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::string* value = (jeecs::string*)wo_pointer(args + 0);
    if (wo_is_ref(args + 1))
        wo_set_string(args + 1, value->c_str());
    else
        *value = wo_string(args + 1);
    return wo_ret_void(vm);
}

WO_API wo_api wojeapi_native_value_rot_euler3(wo_vm vm, wo_value args, size_t argc)
{
    jeecs::math::quat* value = (jeecs::math::quat*)wo_pointer(args + 0);
    auto&& euler_v3 = value->euler_angle();

    bool need_update = false;

    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, euler_v3.x);
    else
        euler_v3.x = wo_float(args + 1), need_update = true;

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, euler_v3.y);
    else
        euler_v3.y = wo_float(args + 2), need_update = true;

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, euler_v3.z);
    else
        euler_v3.z = wo_float(args + 3), need_update = true;

    if (need_update)
        value->set_euler_angle(euler_v3);

    return wo_ret_void(vm);
}
///////////////////////////////////////////////////////////////////////
WO_API wo_api wojeapi_texture_open(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = jeecs::basic::create_new<jeecs::graphic::texture>(wo_string(args + 0));
    if (loaded_texture->enabled())
    {
        return wo_ret_option_gchandle(vm, loaded_texture, nullptr,
            [](void* ptr) {
                jeecs::basic::destroy_free((jeecs::graphic::texture*)ptr);
            });
    }
    jeecs::basic::destroy_free(loaded_texture);
    return wo_ret_option_none(vm);
}
WO_API wo_api wojeapi_texture_create(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = jeecs::basic::create_new<jeecs::graphic::texture>(
        wo_int(args + 0), wo_int(args + 1), jegl_texture::texture_format::RGBA);

    return wo_ret_gchandle(vm, loaded_texture, nullptr,
        [](void* ptr) {
            jeecs::basic::destroy_free((jeecs::graphic::texture*)ptr);
        });
}

WO_API wo_api wojeapi_texture_get_pixel(wo_vm vm, wo_value args, size_t argc)
{
    auto* loaded_texture = (jeecs::graphic::texture*)wo_pointer(args + 0);
    auto* pix = jeecs::basic::create_new<jeecs::graphic::texture::pixel>(*loaded_texture, wo_int(args + 1), wo_int(args + 2));

    return wo_ret_gchandle(vm, pix, args + 0, [](void* ptr)
        {
            jeecs::basic::destroy_free((jeecs::graphic::texture::pixel*)ptr);
        });
}

WO_API wo_api wojeapi_texture_pixel_color(wo_vm vm, wo_value args, size_t argc)
{
    auto* pix = (jeecs::graphic::texture::pixel*)wo_pointer(args + 0);
    auto color = pix->get();

    if (wo_is_ref(args + 1))
        wo_set_float(args + 1, color.x);
    else
        color.x = wo_float(args + 1);

    if (wo_is_ref(args + 2))
        wo_set_float(args + 2, color.y);
    else
        color.y = wo_float(args + 2);

    if (wo_is_ref(args + 3))
        wo_set_float(args + 3, color.z);
    else
        color.z = wo_float(args + 3);

    if (wo_is_ref(args + 4))
        wo_set_float(args + 4, color.w);
    else
        color.w = wo_float(args + 4);

    pix->set(color);

    return wo_ret_void(vm);
}

const char* jeecs_woolang_api_path = "je.wo";
const char* jeecs_woolang_api_src = R"(
import woo.std;
namespace je
{
    extern("libjoyecs", "wojeapi_exit")
    func exit(): void;

    namespace graphic
    {
        using texture = gchandle;
        namespace texture
        {
            extern("libjoyecs", "wojeapi_texture_open")
            func create(var path: string): option<texture>;

            extern("libjoyecs", "wojeapi_texture_create")
            func create(var width: int, var height: int): texture;

            extern("libjoyecs", "wojeapi_texture_get_pixel")
            func pix(var self: texture, var x: int, var y: int): pixel;

            using pixel = gchandle;
            namespace pixel
            {
                extern("libjoyecs", "wojeapi_texture_pixel_color")
                func color(var self: pixel, ref r: real, ref g: real, ref b: real, ref a: real): void;
            }
        }
    }

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

        enum basic_type
        {
            INT, FLOAT, FLOAT2, FLOAT3, FLOAT4, STRING, QUAT
        }
        extern("libjoyecs", "wojeapi_type_basic_type")
        private func create(var tid: basic_type): typeinfo;

        const var int = typeinfo(basic_type::INT);
        const var float = typeinfo(basic_type::FLOAT);
        const var float2 = typeinfo(basic_type::FLOAT2);
        const var float3 = typeinfo(basic_type::FLOAT3);
        const var float4 = typeinfo(basic_type::FLOAT4);
        const var quat = typeinfo(basic_type::QUAT);
        const var string = typeinfo(basic_type::STRING);
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
            func get_shared_system_attached_world(var self:universe, var systype:typeinfo): option<world>;
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

        func rend(): option<world>
        {
            const static var graphic_typeinfo = typeinfo("jeecs::DefaultGraphicPipelineSystem");
            return universe::current()->editor::get_shared_system_attached_world(graphic_typeinfo);
        }

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
        func operator == (var a: entity, var b: entity)
        {
            return a->editor::chunk_info() == b->editor::chunk_info();
        }
        func operator != (var a: entity, var b: entity)
        {
            return a->editor::chunk_info() != b->editor::chunk_info();
        }

        func close(var self: entity)
        {
            extern("libjoyecs", "wojeapi_close_entity")
            func _close(var self: entity): void;

            _close(self);
        }

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
            func get_component(var self: entity, var type: typeinfo): option<component>;

            extern("libjoyecs", "wojeapi_is_top_entity")
            func is_top(var self: entity): bool;

            extern("libjoyecs", "wojeapi_is_child_of_entity")
            func is_child_of(var self: entity, var parent: entity): bool;

            using entity_iter = struct {
                m_cur_iter          : array::iterator<entity>,
                m_judge_func        : bool(entity),
                m_current_entity    : option<entity>,

                m_all_entity_list   : array<entity>,
                m_not_top_entities  : array<entity>,
                m_outed_entities    : array<entity>,
            };
            namespace entity_iter
            {
                private func create(var entitys: array<entity>)
                {
                    var not_top_entities = []: array<entity>;
                    return entity_iter{
                        m_cur_iter = entitys->iter(),
                        m_judge_func = func(var e: entity)
                                       {
                                            var result = e->editor::is_top();
                                            if (!result)
                                                not_top_entities->add(e);
                                            return result
                                       },
                        m_current_entity = option::none:<entity>,
                        m_all_entity_list = entitys,
                        m_not_top_entities = not_top_entities,
                        m_outed_entities = []: array<entity>,
                    };
                }

                func childs(var self: entity_iter)
                {
                    var parent_entity = self.m_current_entity->val();
                    return entity_iter{
                        m_cur_iter = self.m_all_entity_list->iter(),
                        m_judge_func = func(var e: entity)
                                       {
                                            return e->is_child_of(parent_entity);
                                       },
                        m_current_entity = option::none:<entity>,
                        m_all_entity_list = self.m_all_entity_list,
                        m_not_top_entities = []: array<entity>,
                        m_outed_entities = self.m_outed_entities,
                    };
                }
    
                func iter(var self: entity_iter)
                {
                    return self;
                }
                func next(var self: entity_iter, ref out_iter: entity_iter,ref out_entity: entity): bool
                {
                    var current_iter = self.m_cur_iter;
                    while (current_iter->next(0, ref out_entity))
                    {
                        if (self.m_judge_func(out_entity))
                        {
                            self.m_current_entity = option::value(out_entity);
                            out_iter = self;
                            self.m_outed_entities->add(out_entity);
                            return true;
                        }
                    }
                continue_find_not_displayed_entity@
                    while (self.m_not_top_entities->len() >= 0)
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
                            self.m_current_entity = option::value(top);
                            out_iter = self;
                            out_entity = top;
                            self.m_outed_entities->add(top);
                            return true;
                        }
                    }
                    return false;
                }
            } // end of namespace entity_iter
        }
    } // end of namespace entity

    using native_value = handle;
    namespace native_value
    {
        extern("libjoyecs", "wojeapi_native_value_int")
        func int(var self: native_value, ref value: int): void;

        extern("libjoyecs", "wojeapi_native_value_float")
        func float(var self: native_value, ref value: real): void;

        extern("libjoyecs", "wojeapi_native_value_float2")
        func float2(var self: native_value, ref x: real, ref y: real): void;

        extern("libjoyecs", "wojeapi_native_value_float3")
        func float3(var self: native_value, ref x: real, ref y: real, ref z: real): void;

        extern("libjoyecs", "wojeapi_native_value_float4")
        func float4(var self: native_value, ref x: real, ref y: real, ref z: real, ref w: real): void;

        extern("libjoyecs", "wojeapi_native_value_rot_euler3")
        func euler3(var self: native_value, ref x: real, ref y: real, ref z: real): void;

        extern("libjoyecs", "wojeapi_native_value_je_string")
        func string(var self: native_value, ref val: string): void;

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
                    return self;
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

