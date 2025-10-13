#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <box2d/box2d.h>
#include <unordered_map>

namespace jeecs
{
    // We will treat the b2BodyId as a unique id.
    static_assert(sizeof(Physics2D::Rigidbody::rigidbody_id_t) == sizeof(b2BodyId));
    static_assert(sizeof(Physics2D::Collider::shape_id_t) == sizeof(b2ShapeId));

#define JE_B2JBody(b) (*std::launder(                               \
    reinterpret_cast<const Physics2D::Rigidbody::rigidbody_id_t *>( \
        const_cast<const b2BodyId *>(&b))))
#define JE_J2BBody(b) (*std::launder(   \
    reinterpret_cast<const b2BodyId *>( \
        const_cast<const Physics2D::Rigidbody::rigidbody_id_t *>(&b))))

#define JE_B2JShape(b) (*std::launder(                         \
    reinterpret_cast<const Physics2D::Collider::shape_id_t *>( \
        const_cast<const b2ShapeId *>(&b))))
#define JE_J2BShape(b) (*std::launder(   \
    reinterpret_cast<const b2ShapeId *>( \
        const_cast<const Physics2D::Collider::shape_id_t *>(&b))))

    struct Physics2DUpdatingSystem : public game_system
    {
        struct Physics2DWorldContext
        {
            b2WorldId m_physics_world;
            size_t m_step_count;

            size_t m_simulate_round_count;
            std::unordered_map<Physics2D::Rigidbody::rigidbody_id_t, size_t> m_all_alive_bodys;

            constexpr static size_t MAX_GROUP_COUNT = 16;
            struct group_config
            {
                struct filter_config
                {
                    const jeecs::typing::type_info* m_type;
                    requirement::type m_requirement;
                };

                std::vector<filter_config> m_group_filter;
                uint16_t m_collision_mask;
            };
            group_config m_group_configs[MAX_GROUP_COUNT];

            JECS_DISABLE_MOVE_AND_COPY(Physics2DWorldContext);

            Physics2DWorldContext(
                const Physics2D::World* world,
                size_t simulate_round_count)
                : m_physics_world({}), m_step_count(4), m_simulate_round_count(0), m_group_configs()
            {
                b2WorldDef world_def = b2DefaultWorldDef();
                m_physics_world = b2CreateWorld(&world_def);

                update_world_config(
                    world, simulate_round_count);

                if (world->group_config.has_resource())
                {
                    auto path = world->group_config.get_path().value();
                    auto* physics_group_config = jeecs_file_open(path.c_str());

                    if (physics_group_config == nullptr)
                        jeecs::debug::logerr("Unable to open physics group config: '%s', please check.",
                            path.c_str());
                    else
                    {
                        std::vector<char> buf(physics_group_config->m_file_length);
                        jeecs_file_read(buf.data(), sizeof(char), buf.size(), physics_group_config);
                        jeecs_file_close(physics_group_config);

                        wo_vm vmm = wo_create_vm();
                        if (!wo_load_binary(vmm, path.c_str(), buf.data(), buf.size()))
                            jeecs::debug::logerr("Unable to resolve physics group config: '%s':\n%s.",
                                path.c_str(), wo_get_compile_error(vmm, WO_NOTHING));
                        else
                        {
                            // array<group_config_info_t>
                            wo_value result = wo_run(vmm);

                            wo_value vmm_s = wo_reserve_stack(vmm, 4, nullptr);

                            // group_config_info_t: struct{types, collider_mask}
                            wo_value group_config_info = vmm_s + 0;

                            // array<(const type_info*, Requirement)>
                            wo_value filter_types = vmm_s + 1;

                            // Requirement
                            wo_value group_requirement = vmm_s + 2;

                            // int/const type_info*
                            wo_value group_collide_with_mask = vmm_s + 3;

                            if (result == nullptr)
                                jeecs::debug::logerr("Unable to resolve physics group config: '%s': %s.",
                                    path.c_str(), wo_get_runtime_error(vmm));
                            else if (wo_valuetype(result) != WO_ARRAY_TYPE)
                                jeecs::debug::logerr("Unable to resolve physics group config: '%s': Unexpected value type.",
                                    path.c_str());
                            else
                            {
                                // Resolve!

                                wo_integer_t configed_group_count = wo_arr_len(result);
                                if (configed_group_count > (wo_integer_t)MAX_GROUP_COUNT)
                                {
                                    jeecs::debug::logwarn("The number of physics2d collision groups is limited to 16, "
                                        "but the %d group(s) are provided in this configuration: `%s`, excess groups will be ignored",
                                        (int)configed_group_count, path.c_str());
                                    configed_group_count = (wo_integer_t)MAX_GROUP_COUNT;
                                }

                                for (wo_integer_t i = 0; i < (wo_integer_t)MAX_GROUP_COUNT; ++i)
                                {
                                    auto& group = m_group_configs[i];
                                    if (i < configed_group_count)
                                    {
                                        wo_arr_get(group_config_info, result, i);
                                        wo_struct_get(filter_types, group_config_info, 0);
                                        for (wo_integer_t ii = wo_arr_len(filter_types); ii > 0; --ii)
                                        {
                                            wo_arr_get(group_collide_with_mask, filter_types, ii - 1);
                                            wo_struct_get(group_requirement, group_collide_with_mask, 1);
                                            wo_struct_get(group_collide_with_mask, group_collide_with_mask, 0);
                                            group.m_group_filter.push_back(
                                                group_config::filter_config{
                                                    (const typing::type_info*)wo_pointer(group_collide_with_mask),
                                                    (requirement::type)wo_int(group_requirement),
                                                });
                                        }

                                        wo_struct_get(filter_types, group_config_info, 1);
                                        group.m_collision_mask = (uint16_t)wo_int(filter_types);
                                    }
                                    else
                                        group.m_collision_mask = 0x0000;
                                }
                            }
                        }
                        wo_close_vm(vmm);
                    }
                }
            }

            bool update_world_config(
                const Physics2D::World* world,
                size_t simulate_round_count)
            {
                b2Vec2 gravity_config_value = b2Vec2{ world->gravity.x, world->gravity.y };

                if (b2World_GetGravity(m_physics_world) != gravity_config_value)
                {
                    b2World_SetGravity(m_physics_world, gravity_config_value);
                    for (auto& [rbody, _] : m_all_alive_bodys)
                    {
                        // Awake all rigidbody to make sure the gravity is updated.
                        b2Body_SetAwake(JE_J2BBody(rbody), true);
                    }
                }

                b2World_EnableContinuous(m_physics_world, world->continuous);
                b2World_EnableSleeping(m_physics_world, world->sleepable);

                m_step_count = world->step;

                if (m_simulate_round_count == simulate_round_count)
                {
                    jeecs::debug::logerr("Duplicate physical world layerid: %zu, please check and eliminate it.",
                        world->layerid);

                    return false;
                }
                else
                {
                    m_simulate_round_count = simulate_round_count;
                    return true;
                }
            }
        };

        std::map<size_t, std::unique_ptr<Physics2DWorldContext>> _m_alive_physic_worlds;
        size_t m_simulate_round_count;

        Physics2DUpdatingSystem(game_world world)
            : game_system(world), m_simulate_round_count(0)
        {
            assert(JE_B2JBody(b2_nullBodyId) == Physics2D::Rigidbody::null_rigidbody);
            assert(JE_B2JShape(b2_nullShapeId) == Physics2D::Collider::null_shape);
        }

        inline static bool check_if_need_update_vec2(const b2Vec2& a, const math::vec2& b) noexcept
        {
            if (a.x == b.x && a.y == b.y)
                return false;
            return true;
        }
        inline static bool check_if_need_update_float(float a, float b) noexcept
        {
            if (a == b)
                return false;
            return true;
        }

        void PhysicsUpdate()
        {
            ++m_simulate_round_count;

            std::map<size_t, std::unique_ptr<Physics2DWorldContext>>
                _m_this_frame_alive_worlds;

            for (auto& [e, world] : query_entity_view<Physics2D::World&>())
            {
                auto fnd = _m_alive_physic_worlds.find(world.layerid);
                if (fnd == _m_alive_physic_worlds.end())
                {
                    // No such a physic world layer, create context for it.
                    _m_this_frame_alive_worlds[world.layerid] =
                        std::make_unique<Physics2DWorldContext>(
                            &world, m_simulate_round_count);
                }
                else
                {
                    // Update configs
                    if (!fnd->second->update_world_config(&world, m_simulate_round_count))
                        // Invalid world config, remove this world.
                        e.remove_component<Physics2D::World>();

                    _m_this_frame_alive_worlds[world.layerid] = std::move(fnd->second);
                }
            }

            for (auto& [
                e,
                translation,
                rigidbody,
                mass,
                kinematics,
                restitution,
                friction,
                bullet,
                boxcollider,
                circlecollider,
                capsulecollider,
                scale,
                posoffset,
                rotoffset,
                rendshape]: query_entity<
                view<
                Transform::Translation&,
                Physics2D::Rigidbody&,
                Physics2D::Mass*,
                Physics2D::Kinematics*,
                Physics2D::Restitution*,
                Physics2D::Friction*,
                Physics2D::Bullet*,
                Physics2D::Collider::Box*,
                Physics2D::Collider::Circle*,
                Physics2D::Collider::Capsule*,
                Physics2D::Transform::Scale*,
                Physics2D::Transform::Position*,
                Physics2D::Transform::Rotation*,
                Renderer::Shape*>,
                anyof<
                Physics2D::Collider::Box,
                Physics2D::Collider::Circle,
                Physics2D::Collider::Capsule>>())
            {
                auto fnd = _m_this_frame_alive_worlds.find(rigidbody.layerid);
                if (fnd == _m_this_frame_alive_worlds.end())
                {
                    rigidbody.native_rigidbody = Physics2D::Rigidbody::null_rigidbody;
                    jeecs::debug::logerr(
                        "The rigidbody belongs to layer: %d, but the physical world of this layer cannot be found.",
                        rigidbody.layerid);
                    continue;
                }

                auto& physics_world_instance = fnd->second->m_physics_world;
                auto& all_rigidbody_list = fnd->second->m_all_alive_bodys;

                math::vec3 offset_position = posoffset != nullptr
                    ? math::vec3(posoffset->offset)
                    : math::vec3(0.f, 0.f, 0.f)
                    ;
                float final_offset_rotation =
                    translation.world_rotation.euler_angle().z +
                    (rotoffset != nullptr ? rotoffset->angle : 0.f);

                math::vec3 final_offset_position =
                    math::quat::euler(0.f, 0.f, final_offset_rotation) * offset_position;

                if (rigidbody.rigidbody_just_created == true)
                {
                    b2BodyDef default_rigidbody_config = b2DefaultBodyDef();
                    default_rigidbody_config.rotation = b2MakeRot(final_offset_rotation * math::DEG2RAD);
                    default_rigidbody_config.position = {
                        translation.world_position.x + final_offset_position.x,
                        translation.world_position.y + final_offset_position.y };

                    auto b2newbody = b2CreateBody(physics_world_instance, &default_rigidbody_config);
                    rigidbody.native_rigidbody = JE_B2JBody(b2newbody);
                }

                auto existing_alive_body_finding_result =
                    all_rigidbody_list.find(rigidbody.native_rigidbody);

                assert(existing_alive_body_finding_result == all_rigidbody_list.end() ||
                    existing_alive_body_finding_result->second + 1 == m_simulate_round_count);

                if (existing_alive_body_finding_result == all_rigidbody_list.end()
                    && rigidbody.rigidbody_just_created == false)
                {
                    rigidbody.rigidbody_just_created = true;
                    rigidbody.native_rigidbody = Physics2D::Rigidbody::null_rigidbody;
                }
                else
                {
                    rigidbody.rigidbody_just_created = false;

                    assert(rigidbody.native_rigidbody != Physics2D::Rigidbody::null_rigidbody);
                    b2BodyId rigidbody_instance = JE_J2BBody(rigidbody.native_rigidbody);
                    all_rigidbody_list[rigidbody.native_rigidbody] = m_simulate_round_count;

                    // 如果实体有 Physics2D::Bullet 组件，那么就适用高精度碰撞
                    b2Body_SetBullet(rigidbody_instance, bullet != nullptr);

                    b2ShapeId old_shape_id;
                    const auto old_shape_count =
                        b2Body_GetShapes(rigidbody_instance, &old_shape_id, 1);

                    // 开始创建碰撞体
                    bool force_update_fixture = old_shape_count == 0;

                    // NOTE: 此处不再考虑实体的网格大小，开发者应当自行调整
                    auto entity_scaled_size = math::vec2(
                        translation.local_scale.x,
                        translation.local_scale.y);

                    if (scale != nullptr)
                        entity_scaled_size *= scale->scale;

                    if (entity_scaled_size != rigidbody.record_body_scale)
                    {
                        rigidbody.record_body_scale = entity_scaled_size;
                        force_update_fixture = true;
                    }
                    if (mass != nullptr && mass->density != rigidbody.record_density)
                    {
                        rigidbody.record_density = mass->density;
                        force_update_fixture = true;
                    }
                    if (friction != nullptr && friction->value != rigidbody.record_friction)
                    {
                        rigidbody.record_friction = friction->value;
                        force_update_fixture = true;
                    }
                    if (restitution != nullptr && restitution->value != rigidbody.record_restitution)
                    {
                        rigidbody.record_restitution = restitution->value;
                        force_update_fixture = true;
                    }

                    // Check if the shape has been changed.
                    if (boxcollider != nullptr)
                    {
                        if (JE_B2JShape(old_shape_id) != boxcollider->native_shape)
                            force_update_fixture = true;
                    }
                    else if (circlecollider != nullptr)
                    {
                        if (JE_B2JShape(old_shape_id) != circlecollider->native_shape)
                            force_update_fixture = true;
                    }
                    else if (capsulecollider != nullptr)
                    {
                        if (JE_B2JShape(old_shape_id) != capsulecollider->native_shape)
                            force_update_fixture = true;
                    }

                    if (force_update_fixture || old_shape_count == 0)
                    {
                        b2ShapeDef shape_define = b2DefaultShapeDef();
                        shape_define.density = mass ? mass->density : 0.f;
                        shape_define.material.friction = friction ? friction->value : 0.f;
                        shape_define.material.restitution = restitution ? restitution->value : 0.f;

                        b2ShapeId created_shape_id;

                        if (boxcollider != nullptr)
                        {
                            auto box_polygon = b2MakeBox(
                                abs(entity_scaled_size.x / 2.f),
                                abs(entity_scaled_size.y / 2.f));

                            created_shape_id = b2CreatePolygonShape(
                                rigidbody_instance,
                                &shape_define,
                                &box_polygon);
                            boxcollider->native_shape = JE_B2JShape(created_shape_id);
                        }
                        else if (circlecollider != nullptr)
                        {
                            b2Circle circle;
                            circle.center = b2Vec2{ 0.f, 0.f };
                            circle.radius = std::max(abs(entity_scaled_size.x / 2.f), abs(entity_scaled_size.y / 2.f));

                            created_shape_id = b2CreateCircleShape(
                                rigidbody_instance,
                                &shape_define,
                                &circle);
                            circlecollider->native_shape = JE_B2JShape(created_shape_id);
                        }
                        else if (capsulecollider != nullptr)
                        {
                            b2Capsule capsule;

                            float height = abs(entity_scaled_size.y / 2.f);

                            capsule.radius = abs(entity_scaled_size.x / 2.f);
                            capsule.center1 = b2Vec2{
                                0.f, height < capsule.radius ? 0.f : height - capsule.radius };
                            capsule.center2 = b2Vec2{
                                0.f, height < capsule.radius ? 0.f : capsule.radius - height };

                            created_shape_id = b2CreateCapsuleShape(
                                rigidbody_instance,
                                &shape_define,
                                &capsule);
                            capsulecollider->native_shape = JE_B2JShape(created_shape_id);
                        }
                        else
                        {
                            // Cannot be here.
                            abort();
                        }

                        // Set address of rigidbody for storing collision result.
                        b2Shape_SetUserData(created_shape_id, &rigidbody);

                        if (old_shape_count != 0)
                            b2DestroyShape(old_shape_id, true);
                    }

                    if (kinematics == nullptr)
                        b2Body_SetType(rigidbody_instance, b2_staticBody);
                    else
                    {
                        if (mass == nullptr)
                            b2Body_SetType(rigidbody_instance, b2_kinematicBody);
                        else
                            b2Body_SetType(rigidbody_instance, b2_dynamicBody);

                        // 检查刚体内的动力学和变换属性，从组件同步到物理引擎
                        if (check_if_need_update_vec2(b2Body_GetLinearVelocity(rigidbody_instance), kinematics->linear_velocity))
                            b2Body_SetLinearVelocity(rigidbody_instance, b2Vec2{ kinematics->linear_velocity.x, kinematics->linear_velocity.y });
                        if (check_if_need_update_float(b2Body_GetAngularVelocity(rigidbody_instance), kinematics->angular_velocity))
                            b2Body_SetAngularVelocity(rigidbody_instance, kinematics->angular_velocity);

                        if (check_if_need_update_float(b2Body_GetLinearDamping(rigidbody_instance), kinematics->linear_damping))
                            b2Body_SetLinearDamping(rigidbody_instance, kinematics->linear_damping);
                        if (check_if_need_update_float(b2Body_GetAngularDamping(rigidbody_instance), kinematics->angular_damping))
                            b2Body_SetAngularDamping(rigidbody_instance, kinematics->angular_damping);
                        if (check_if_need_update_float(b2Body_GetGravityScale(rigidbody_instance), kinematics->gravity_scale))
                            b2Body_SetGravityScale(rigidbody_instance, kinematics->gravity_scale);

                        if (b2Body_IsFixedRotation(rigidbody_instance) != kinematics->lock_rotation)
                        {
                            b2Body_SetFixedRotation(rigidbody_instance, kinematics->lock_rotation);
                            b2Body_SetAwake(rigidbody_instance, true);
                        }
                    }

                    if (check_if_need_update_vec2(
                        b2Body_GetPosition(rigidbody_instance),
                        math::vec2(
                            translation.world_position.x + final_offset_position.x,
                            translation.world_position.y + final_offset_position.y))
                        || check_if_need_update_float(
                            b2Rot_GetAngle(b2Body_GetRotation(rigidbody_instance)) * math::RAD2DEG,
                            final_offset_rotation))
                    {
                        b2Body_SetTransform(
                            rigidbody_instance,
                            b2Vec2{
                                translation.world_position.x + final_offset_position.x,
                                translation.world_position.y + final_offset_position.y },
                                b2MakeRot(final_offset_rotation * math::DEG2RAD));
                        b2Body_SetAwake(rigidbody_instance, true);
                    }

                    if (force_update_fixture || rigidbody._arch_updated_modify_hack != &rigidbody)
                    {
                        b2ShapeId current_shape_id;
                        if (b2Body_GetShapes(rigidbody_instance, &current_shape_id, 1) != 0)
                        {
                            rigidbody._arch_updated_modify_hack = &rigidbody;
                            uint16_t self_mask_type = 0x0000;
                            uint16_t self_collide_group = 0x0000;

                            for (size_t i = 0; i < Physics2DWorldContext::MAX_GROUP_COUNT; ++i)
                            {
                                auto& group = fnd->second->m_group_configs[i];
                                bool is_this_group = true;

                                for (auto& filter_type : group.m_group_filter)
                                {
                                    assert(filter_type.m_requirement == requirement::type::CONTAINS
                                        || filter_type.m_requirement == requirement::type::EXCEPT);

                                    bool has_component = je_ecs_world_entity_get_component(&e, filter_type.m_type->m_id);
                                    if ((filter_type.m_requirement == requirement::type::CONTAINS) != has_component)
                                    {
                                        is_this_group = false;
                                        break;
                                    }
                                }

                                if (is_this_group)
                                {
                                    self_mask_type |= (uint16_t)(1 << i);
                                    self_collide_group |= group.m_collision_mask;
                                }
                            }

                            b2Filter filter = b2Shape_GetFilter(current_shape_id);
                            filter.maskBits = self_mask_type;
                            filter.categoryBits = self_collide_group;
                            b2Shape_SetFilter(current_shape_id, filter);
                        }
                    }
                }
            }

            // 物理引擎在此处进行物理解算
            for (auto& [_, physics_world] : _m_this_frame_alive_worlds)
            {
                b2World_Step(
                    physics_world->m_physics_world,
                    deltatime(),
                    physics_world->m_step_count);

                // NOTE: 这个变量名真恐怖...
                std::vector<Physics2D::Rigidbody::rigidbody_id_t> _dead_bodys;

                for (auto [body, round] : physics_world->m_all_alive_bodys)
                {
                    if (round != m_simulate_round_count)
                        _dead_bodys.push_back(body);
                }
                for (auto dead_body : _dead_bodys)
                {
                    physics_world->m_all_alive_bodys.erase(dead_body);
                    b2DestroyBody(JE_J2BBody(dead_body));
                }
            }

            _m_alive_physic_worlds.swap(_m_this_frame_alive_worlds);

            // 在此处将动力学数据更新到组件上
            for (auto& [
                translation,
                localposition,
                localrotation,
                rigidbody,
                kinematics,
                collisions,
                posoffset,
                rotoffset] : query<
                view<
                Transform::Translation&,
                Transform::LocalPosition&,
                Transform::LocalRotation&,
                Physics2D::Rigidbody&,
                Physics2D::Kinematics*,
                Physics2D::CollisionResult*,
                Physics2D::Transform::Position*,
                Physics2D::Transform::Rotation*>,
                anyof<
                Physics2D::Kinematics,
                Physics2D::CollisionResult>>())

            {
                if (rigidbody.native_rigidbody != Physics2D::Rigidbody::null_rigidbody)
                {
                    // 从刚体获取解算完成之后的坐标
                    b2BodyId rigidbody_instance = JE_J2BBody(rigidbody.native_rigidbody);

                    auto new_position = b2Body_GetPosition(rigidbody_instance);
                    if (kinematics != nullptr &&
                        rigidbody.rigidbody_just_created == false)
                    {
                        math::vec3 offset_position = posoffset != nullptr
                            ? math::vec3(posoffset->offset)
                            : math::vec3(0.f, 0.f, 0.f);
                        float final_offset_rotation =
                            b2Rot_GetAngle(b2Body_GetRotation(rigidbody_instance)) * math::RAD2DEG - (rotoffset != nullptr ? rotoffset->angle : 0.f);

                        math::vec3 final_offset_position =
                            math::quat::euler(0.f, 0.f, final_offset_rotation) * offset_position;

                        auto new_velocity = b2Body_GetLinearVelocity(rigidbody_instance);

                        kinematics->linear_velocity = math::vec2{
                            kinematics->lock_movement_x ? 0.0f : new_velocity.x,
                            kinematics->lock_movement_y ? 0.0f : new_velocity.y };
                        translation.set_global_position(
                            math::vec3(
                                kinematics->lock_movement_x ? translation.world_position.x : new_position.x - final_offset_position.x,
                                kinematics->lock_movement_y ? translation.world_position.y : new_position.y - final_offset_position.y,
                                translation.world_position.z),
                            &localposition,
                            &localrotation);

                        kinematics->angular_velocity = b2Body_GetAngularVelocity(rigidbody_instance);

                        auto world_angle = translation.world_rotation.euler_angle();
                        world_angle.z = final_offset_rotation;
                        translation.set_global_rotation(
                            math::quat::euler(world_angle),
                            &localrotation);
                    }
                    if (collisions != nullptr)
                    {
                        collisions->results.clear();

                        std::vector<b2ContactData> contacts(
                            (size_t)b2Body_GetContactCapacity(rigidbody_instance));

                        int contact_count = b2Body_GetContactData(
                            rigidbody_instance,
                            contacts.data(),
                            (int)contacts.size());

                        assert((size_t)contact_count <= contacts.size());
                        contacts.resize((size_t)contact_count);

                        for (const auto& contact : contacts)
                        {
                            if (contact.manifold.pointCount > 0)
                            {
                                auto* a = std::launder(reinterpret_cast<Physics2D::Rigidbody*>(
                                    b2Shape_GetUserData(contact.shapeIdA)));
                                auto* b = std::launder(reinterpret_cast<Physics2D::Rigidbody*>(
                                    b2Shape_GetUserData(contact.shapeIdB)));

                                Physics2D::Rigidbody* other_rigidbody;
                                math::vec2 contact_point;

                                if (a->native_rigidbody != rigidbody.native_rigidbody)
                                {
                                    other_rigidbody = a;
                                    contact_point = math::vec2(
                                        translation.world_position.x + contact.manifold.points[0].anchorB.x,
                                        translation.world_position.y + contact.manifold.points[0].anchorB.y);
                                }
                                else
                                {
                                    other_rigidbody = b;
                                    contact_point = math::vec2(
                                        translation.world_position.x + contact.manifold.points[0].anchorA.x,
                                        translation.world_position.y + contact.manifold.points[0].anchorA.y);
                                }

                                assert(other_rigidbody != nullptr);
                                assert(collisions->results.find(other_rigidbody) == collisions->results.end());

                                collisions->results[other_rigidbody] =
                                    Physics2D::CollisionResult::collide_result{
                                        contact_point,
                                        math::vec2(
                                            contact.manifold.normal.x,
                                            contact.manifold.normal.y),
                                };
                            }
                        }
                    }
                }
            }
        }
    };
}
