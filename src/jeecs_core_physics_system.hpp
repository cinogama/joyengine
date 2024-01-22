#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <box2d/box2d.h>
#include <unordered_map>

namespace jeecs
{
    struct Physics2DUpdatingSystem :public game_system
    {
        struct Physics2DWorldContext
        {
            b2World m_physics_world;
            size_t  m_velocity_iterations;
            size_t  m_position_iterations;

            size_t m_simulate_round_count;
            std::unordered_map<b2Body*, size_t> m_all_alive_bodys;

            constexpr static size_t MAX_GROUP_COUNT = 16;
            struct group_config
            {
                struct filter_config
                {
                    const jeecs::typing::type_info* m_type;
                    requirement::type               m_requirement;
                };

                std::vector<filter_config> m_group_filter;
                uint16_t m_collision_mask;
            };
            group_config m_group_configs[MAX_GROUP_COUNT];

            JECS_DISABLE_MOVE_AND_COPY(Physics2DWorldContext);

            Physics2DWorldContext(
                const Physics2D::World* world,
                size_t simulate_round_count
            )
                : m_physics_world({})
                , m_velocity_iterations(8)
                , m_position_iterations(3)
                , m_simulate_round_count(0)
                , m_group_configs()
            {
                update_world_config(
                    world, simulate_round_count);

                if (world->group_config.has_resource())
                {
                    auto path = world->group_config.get_path();
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

                            // group_config_info_t: struct{types, collider_mask}
                            wo_value group_config_info = wo_push_empty(vmm);

                            // array<(const type_info*, Requirement)>
                            wo_value filter_types = wo_push_empty(vmm);

                            // Requirement
                            wo_value group_requirement = wo_push_empty(vmm);

                            // int/const type_info*
                            wo_value group_collide_with_mask = wo_push_empty(vmm);

                            if (result == nullptr)
                                jeecs::debug::logerr("Unable to resolve physics group config: '%s': %s.",
                                    path.c_str(), wo_get_runtime_error(vmm));
                            else if (wo_valuetype(result) != WO_ARRAY_TYPE)
                                jeecs::debug::logerr("Unable to resolve physics group config: '%s': Unexpected value type.",
                                    path.c_str());
                            else
                            {
                                // Resolve!

                                wo_integer_t configed_group_count = wo_lengthof(result);
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
                                        for (wo_integer_t ii = wo_lengthof(filter_types); ii > 0; --ii)
                                        {
                                            wo_arr_get(group_collide_with_mask, filter_types, ii - 1);
                                            wo_struct_get(group_requirement, group_collide_with_mask, 1);
                                            wo_struct_get(group_collide_with_mask, group_collide_with_mask, 0);
                                            group.m_group_filter.push_back(
                                                group_config::filter_config
                                                {
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

            void update_world_config(
                const Physics2D::World* world,
                size_t simulate_round_count)
            {
                bool updated = false;
                auto gravity_config_value = b2Vec2(world->gravity.x, world->gravity.y);
                if (m_physics_world.GetGravity() != gravity_config_value)
                {
                    updated = true;
                    m_physics_world.SetGravity(gravity_config_value);
                }
                if (m_physics_world.GetAllowSleeping() != world->sleepable)
                {
                    updated = true;
                    m_physics_world.SetAllowSleeping(world->sleepable);
                }
                if (m_physics_world.GetContinuousPhysics() != world->continuous)
                {
                    updated = true;
                    m_physics_world.SetContinuousPhysics(world->continuous);
                }

                m_velocity_iterations = world->velocity_step;
                m_position_iterations = world->position_step;

                if (updated)
                {
                    for (auto& [rbody, _] : m_all_alive_bodys)
                        rbody->SetAwake(true);
                }

                if (m_simulate_round_count == simulate_round_count)
                    jeecs::debug::logerr("Duplicate physical world layerid: %zu, please check and eliminate it.",
                        world->layerid);
                else
                    m_simulate_round_count = simulate_round_count;
            }
        };

        std::map<size_t, std::unique_ptr<Physics2DWorldContext>> _m_alive_physic_worlds;
        size_t m_simulate_round_count;

        Physics2DUpdatingSystem(game_world world)
            : game_system(world)
            , m_simulate_round_count(0)
        {

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

        void StateUpdate()
        {
            b2BodyDef default_rigidbody_config;

            ++m_simulate_round_count;

            std::map<size_t, std::unique_ptr<Physics2DWorldContext>>
                _m_this_frame_alive_worlds;

            auto& selector = select_begin();

            selector.exec([&](Physics2D::World& world)
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
                        fnd->second->update_world_config(&world, m_simulate_round_count);
                        _m_this_frame_alive_worlds[world.layerid] = std::move(fnd->second);
                    }
                });

            selector.anyof<Physics2D::BoxCollider, Physics2D::CircleCollider>();
            selector.exec([&](
                game_entity e,
                Transform::Translation& translation,
                Transform::LocalPosition& localposition,
                Transform::LocalRotation& localrotation,
                Physics2D::Rigidbody& rigidbody,
                Physics2D::Mass* mass,
                Physics2D::Kinematics* kinematics,
                Physics2D::Restitution* restitution,
                Physics2D::Friction* friction,
                Physics2D::Bullet* bullet,
                Physics2D::BoxCollider* boxcollider,
                Physics2D::CircleCollider* circlecollider,
                Renderer::Shape* rendshape)
                {
                    auto fnd = _m_this_frame_alive_worlds.find(rigidbody.layerid);
                    if (fnd == _m_this_frame_alive_worlds.end())
                    {
                        rigidbody.native_rigidbody = nullptr;
                        jeecs::debug::logerr("The rigidbody belongs to layer: %d, but the physical world of this layer cannot be found.",
                            rigidbody.layerid);
                        return;
                    }

                    auto& physics_world_instance = fnd->second->m_physics_world;
                    auto& all_rigidbody_list = fnd->second->m_all_alive_bodys;

                    if (rigidbody.rigidbody_just_created == true)
                    {
                        b2BodyDef default_rigidbody_config;
                        default_rigidbody_config.position = { translation.world_position.x, translation.world_position.y };
                        default_rigidbody_config.angle = localrotation.rot.euler_angle().z / math::RAD2DEG;
                        rigidbody.native_rigidbody = physics_world_instance.CreateBody(&default_rigidbody_config);
                    }

                    auto existing_alive_body_finding_result = all_rigidbody_list.find((b2Body*)rigidbody.native_rigidbody);
                    assert(existing_alive_body_finding_result == all_rigidbody_list.end() ||
                        existing_alive_body_finding_result->second + 1 == m_simulate_round_count);

                    if (rigidbody.rigidbody_just_created == false
                        && existing_alive_body_finding_result == all_rigidbody_list.end())
                    {
                        rigidbody.rigidbody_just_created = true;
                        rigidbody.native_rigidbody = nullptr;
                    }
                    else
                    {
                        rigidbody.rigidbody_just_created = false;

                        b2Body* rigidbody_instance = (b2Body*)rigidbody.native_rigidbody;
                        assert(rigidbody_instance != nullptr);

                        // Set address of rigidbody for storing collision result.
                        rigidbody_instance->GetUserData().pointer = (uintptr_t)&rigidbody;

                        all_rigidbody_list[rigidbody_instance] = m_simulate_round_count;

                        // 如果实体有 Physics2D::Bullet 组件，那么就适用高精度碰撞
                        rigidbody_instance->SetBullet(bullet != nullptr);

                        bool need_remove_old_fixture = false;
                        auto* old_rigidbody_fixture = rigidbody_instance->GetFixtureList();
                        // 开始创建碰撞体

                        // 获取实体的网格大小，如果没有，那么默认就是 1，1
                        // TODO: 考虑网格本身改变的情况，不过目前应该没人会去动网格
                        auto&& rendshape_mesh_size =
                            rendshape && rendshape->vertex.has_resource()
                            ? math::vec2(
                                rendshape->vertex.get_resource()->resouce()->m_raw_vertex_data->m_size_x,
                                rendshape->vertex.get_resource()->resouce()->m_raw_vertex_data->m_size_y)
                            : math::vec2(1.f, 1.f);

                        auto entity_scaled_size = rendshape_mesh_size
                            * math::vec2(translation.local_scale.x, translation.local_scale.y);

                        bool force_update_fixture = false;
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

                        if (boxcollider != nullptr)
                        {
                            if (old_rigidbody_fixture == nullptr
                                || old_rigidbody_fixture != boxcollider->native_fixture
                                || force_update_fixture)
                            {
                                // 引擎暂时不支持一个实体有多个fixture，这里标记一下，移除旧的。
                                need_remove_old_fixture = true;

                                // TODO: 此处保存此大小到组件中，如果发现大小发生变化，则重设Fixture，下同
                                auto collider_size = entity_scaled_size * boxcollider->scale;

                                b2PolygonShape box_shape;
                                box_shape.SetAsBox(abs(collider_size.x / 2.f), abs(collider_size.y / 2.f));

                                b2FixtureDef box_shape_fixture_define;
                                box_shape_fixture_define.shape = &box_shape;
                                box_shape_fixture_define.density = mass ? mass->density : 0.f;
                                box_shape_fixture_define.friction = friction ? friction->value : 0.f;
                                box_shape_fixture_define.restitution = restitution ? restitution->value : 0.f;
                                boxcollider->native_fixture =
                                    rigidbody_instance->CreateFixture(&box_shape_fixture_define);
                            }
                        }
                        else if (circlecollider != nullptr)
                        {
                            if (old_rigidbody_fixture == nullptr
                                || old_rigidbody_fixture != circlecollider->native_fixture
                                || force_update_fixture)
                            {
                                // 引擎暂时不支持一个实体有多个fixture，这里标记一下，移除旧的。
                                need_remove_old_fixture = true;

                                auto collider_size = entity_scaled_size * circlecollider->scale;

                                b2CircleShape circle_shape;
                                circle_shape.m_radius = std::max(abs(collider_size.x / 2.f), abs(collider_size.y / 2.f));

                                b2FixtureDef circle_shape_fixture_define;
                                circle_shape_fixture_define.shape = &circle_shape;
                                circle_shape_fixture_define.density = mass ? mass->density : 0.f;
                                circle_shape_fixture_define.friction = friction ? friction->value : 0.f;
                                circle_shape_fixture_define.restitution = restitution ? restitution->value : 0.f;
                                circlecollider->native_fixture =
                                    rigidbody_instance->CreateFixture(&circle_shape_fixture_define);
                            }
                        }

                        if (need_remove_old_fixture && old_rigidbody_fixture != nullptr)
                            rigidbody_instance->DestroyFixture(old_rigidbody_fixture);

                        if (kinematics == nullptr)
                            rigidbody_instance->SetType(b2_staticBody);
                        else
                        {
                            if (mass == nullptr)
                                rigidbody_instance->SetType(b2_kinematicBody);
                            else
                                rigidbody_instance->SetType(b2_dynamicBody);

                            // 检查刚体内的动力学和变换属性，从组件同步到物理引擎
                            if (check_if_need_update_vec2(rigidbody_instance->GetLinearVelocity(), kinematics->linear_velocity))
                                rigidbody_instance->SetLinearVelocity({ kinematics->linear_velocity.x, kinematics->linear_velocity.y });
                            if (check_if_need_update_float(rigidbody_instance->GetAngularVelocity(), kinematics->angular_velocity))
                                rigidbody_instance->SetAngularVelocity(kinematics->angular_velocity);

                            if (check_if_need_update_float(rigidbody_instance->GetLinearDamping(), kinematics->linear_damping))
                                rigidbody_instance->SetLinearDamping(kinematics->linear_damping);
                            if (check_if_need_update_float(rigidbody_instance->GetAngularDamping(), kinematics->angular_damping))
                                rigidbody_instance->SetAngularDamping(kinematics->angular_damping);
                            if (check_if_need_update_float(rigidbody_instance->GetGravityScale(), kinematics->gravity_scale))
                                rigidbody_instance->SetAngularDamping(kinematics->gravity_scale);

                            if (rigidbody_instance->IsFixedRotation() != kinematics->lock_rotation)
                            {
                                rigidbody_instance->SetFixedRotation(kinematics->lock_rotation);
                                rigidbody_instance->SetAwake(true);
                            }
                        }

                        if (check_if_need_update_vec2(rigidbody_instance->GetPosition(), math::vec2(translation.world_position.x, translation.world_position.y))
                            || check_if_need_update_float(rigidbody_instance->GetAngle() * math::RAD2DEG, translation.world_rotation.euler_angle().z))
                        {
                            rigidbody_instance->SetTransform(b2Vec2(translation.world_position.x, translation.world_position.y),
                                translation.world_rotation.euler_angle().z / math::RAD2DEG);
                            rigidbody_instance->SetAwake(true);
                        }

                        if (rigidbody._arch_updated_modify_hack != &rigidbody)
                        {
                            if (auto* fixture = rigidbody_instance->GetFixtureList())
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
                                        assert(filter_type.m_requirement == requirement::type::CONTAIN
                                            || filter_type.m_requirement == requirement::type::EXCEPT);

                                        bool has_component = je_ecs_world_entity_get_component(&e, filter_type.m_type);
                                        if ((filter_type.m_requirement == requirement::type::CONTAIN) != has_component)
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

                                b2Filter fixture_filter = fixture->GetFilterData();
                                fixture_filter.maskBits = self_mask_type;
                                fixture_filter.categoryBits = self_collide_group;
                                fixture->SetFilterData(fixture_filter);
                            }
                        }
                    }
                });

            // 物理引擎在此处进行物理解算
            for (auto& [_, physics_world] : _m_this_frame_alive_worlds)
            {
                physics_world->m_physics_world.Step(
                    deltatime(),
                    physics_world->m_velocity_iterations,
                    physics_world->m_position_iterations);

                std::vector<b2Body*> _dead_bodys; // 这个变量名真恐怖...

                for (auto [body, round] : physics_world->m_all_alive_bodys)
                {
                    if (round != m_simulate_round_count)
                        _dead_bodys.push_back(body);
                }
                for (auto dead_body : _dead_bodys)
                {
                    physics_world->m_all_alive_bodys.erase(dead_body);
                    physics_world->m_physics_world.DestroyBody(dead_body);
                }
            }

            _m_alive_physic_worlds.swap(_m_this_frame_alive_worlds);

            // 在此处将动力学数据更新到组件上
            selector.exec(
                [](
                    Transform::Translation& translation,
                    Transform::LocalPosition& localposition,
                    Transform::LocalRotation& localrotation,
                    Physics2D::Rigidbody& rigidbody,
                    Physics2D::Kinematics* kinematics,
                    Physics2D::CollisionResult* collisions
                    ) {
                        if (rigidbody.native_rigidbody != nullptr)
                        {
                            // 从刚体获取解算完成之后的坐标
                            b2Body* rigidbody_instance = (b2Body*)rigidbody.native_rigidbody;

                            if (collisions != nullptr)
                            {
                                collisions->results.clear();

                                auto* connect = rigidbody_instance->GetContactList();
                                while (connect != nullptr)
                                {
                                    if (connect->contact->GetManifold()->pointCount > 0)
                                    {
                                        auto* rigidbody = std::launder(reinterpret_cast<Physics2D::Rigidbody*>(
                                            connect->other->GetUserData().pointer));

                                        assert(rigidbody != nullptr);
                                        assert(collisions->results.find(rigidbody) == collisions->results.end());

                                        b2WorldManifold manifold;
                                        connect->contact->GetWorldManifold(&manifold);

                                        collisions->results[rigidbody] =
                                            Physics2D::CollisionResult::collide_result{
                                                math::vec2(manifold.points[0].x, manifold.points[0].y),
                                                math::vec2(manifold.normal.x,manifold.normal.y),
                                        };
                                    }
                                    connect = connect->next;
                                }
                            }
                            if (kinematics != nullptr && rigidbody.rigidbody_just_created == false)
                            {
                                auto& new_position = rigidbody_instance->GetPosition();

                                kinematics->linear_velocity = math::vec2{
                                    kinematics->lock_movement_x ? 0.0f : rigidbody_instance->GetLinearVelocity().x,
                                    kinematics->lock_movement_y ? 0.0f : rigidbody_instance->GetLinearVelocity().y
                                };
                                localposition.set_global_position(
                                    math::vec3(
                                        kinematics->lock_movement_x ? translation.world_position.x : new_position.x,
                                        kinematics->lock_movement_y ? translation.world_position.y : new_position.y,
                                        translation.world_position.z),
                                    translation, &localrotation);

                                kinematics->angular_velocity = rigidbody_instance->GetAngularVelocity();

                                auto&& world_angle = translation.world_rotation.euler_angle();
                                world_angle.z = rigidbody_instance->GetAngle() * math::RAD2DEG;
                                localrotation.set_global_rotation(math::quat::euler(world_angle), translation);
                            }
                        }
                }
            );
        }
    };
}

const char* jeecs_physics2d_config_path = "je/physics2d/config.wo";
const char* jeecs_physics2d_config_src = R"(
// (C)Cinogama. 
import woo::std;
import je;
import je::towoo;

namespace je::physics2d::config
{
    using CollideGroupInfo = struct{
        m_filter_components: vec<(je::typeinfo, CollideGroupInfo::Requirement)>,
        m_collide_mask: mut int,

        m_self_gid: int,
    }
    {
        public enum Requirement
        {
            CONTAIN,        // Must have spcify component
            MAYNOT,         // May have or not have
            ANYOF,          // Must have one of 'ANYOF' components
            EXCEPT,         // Must not contain spcify component
        }

        public let groups = []mut: vec<CollideGroupInfo>;

        public func create()
        {
            let self = CollideGroupInfo{
                m_filter_components = []mut, 
                m_collide_mask = mut 0,
                m_self_gid = groups->len,
            };

            groups->add(self);
            return self;
        }
        public func add_filter_components(self: CollideGroupInfo, t: je::typeinfo, r: Requirement)
        {
            self.m_filter_components->add((t, r));
        }
        public func collide_each_other(a: CollideGroupInfo, b: CollideGroupInfo)
        {
            using std;
            a.m_collide_mask = a.m_collide_mask->bitor(1->bitshl(b.m_self_gid));
            b.m_collide_mask = b.m_collide_mask->bitor(1->bitshl(a.m_self_gid));
        }
    }
}

#macro PHYSICS2D_GROUP
{
    /*
    PHYSICS2D_GROUP! GroupName
    {
        ComponentA,
        ComponentB,
    }
    */
    using std::token_type;

    let group_name = lexer->expecttoken(l_identifier)->valor("<Empty>");
    if (lexer->next != "{")
    {
        lexer->error("Unexpected token, should be '{'.");
        return "";
    }
    
    let types = []mut: vec<(string, string)>;

    parse_filter_components@
    for (;;)
    {
        let mut typename = "";
        let mut mode = option::none: option<string>;
        for (;;)
        {
            let token = lexer->next;
            if (token == ";")
            {
                if (typename == "")
                {
                    lexer->error("Unexpected '}', should be typename.");
                    return "";
                }

                types->add((typename, mode->valor("CONTAIN")));
                typename = "";
                break;
            }
            else if (token == "}")
            {
                if (typename != "")
                {
                    lexer->error("Unexpected '}', should be ';'.");
                    return "";
                }
                break parse_filter_components;
            }
            else if (token == "")
            {
                lexer->error("Unexpected EOF.");
                return "";
            }
            else if (token == "except")
            {
                if (mode->has)
                {
                    lexer->error(F"Duplicately marked attributes have been previously marked as `{mode->val}`");
                    return "";
                }
                mode = option::value("EXCEPT");
            }
            else if (token == "contain")
            {
                if (mode->has)
                {
                    lexer->error(F"Duplicately marked attributes have been previously marked as `{mode->val}`");
                    return "";
                }
                mode = option::value("CONTAIN");
            }
            else
                typename += token;
        }
    }

    let mut result = F"let {group_name} = je::physics2d::config::CollideGroupInfo::create();";
    for (let _, (typename, mode) : types)
    {
        result += F"{group_name}->add_filter_components({typename}::id, je::physics2d::config::CollideGroupInfo::Requirement::{mode});";
    }
    return result;
}

#macro PHYSICS2D_COLLISIONS
{
    if (lexer->next != "{")
    {
        lexer->error("Unexpected token, should be '{'.");
        return "";
    }

    let mut groups = []mut: vec<vec<string>>;
    let mut group = []mut: vec<string>;
    let mut typename = "";

    for (;;)
    {
        let token = lexer->next;
        if (token == "")
        {
            lexer->error("Unexpected EOF.");
            return "";
        }
        else if (token == "}")
            break;
        else if (token == ",")
        {
            if (typename != "")
            {
                group->add(typename);
                typename = "";
            }
        }
        else if (token == ";")
        {
            if (typename != "")
            {
                group->add(typename);
                typename = "";
            }
            if (group->empty)
            {
                lexer->error("Unexpected ';'.");
                return "";
            }
            groups->add(group);
            group = []mut;
        }
        else
            typename += token;
    }

    let mut result = "";
    for (let _, group_list : groups)
    {
        for (let i, group_a : group_list)
        {
            for (let j, group_b : group_list)
            {
                if (i == j) continue;
                result += F"{group_a}->collide_each_other({group_b});";
            }
        }
    }
    return result + "return je::physics2d::config::CollideGroupInfo::groups;";
}
)";