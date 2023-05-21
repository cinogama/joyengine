#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <box2d/box2d.h>
#include <unordered_map>

namespace jeecs
{
    struct Physics2DUpdatingSystem :public game_system
    {
        b2World m_physics_world;
        size_t m_simulate_round_count;
        std::unordered_map<b2Body*, size_t> m_all_alive_bodys;

        Physics2DUpdatingSystem(game_world world) :game_system(world)
            , m_physics_world(b2Vec2(0.f, -9.8f))
            , m_simulate_round_count(0)
        {
            m_physics_world.SetAllowSleeping(true);
            m_physics_world.SetContinuousPhysics(true);
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

        void UpdateBeforeSimulate(
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
            auto existing_alive_body_finding_result = m_all_alive_bodys.find((b2Body*)rigidbody.native_rigidbody);
            assert(existing_alive_body_finding_result == m_all_alive_bodys.end() ||
                existing_alive_body_finding_result->second + 1 == m_simulate_round_count);

            if (existing_alive_body_finding_result == m_all_alive_bodys.end())
            {
                b2BodyDef default_rigidbody_config;
                default_rigidbody_config.position = { translation.world_position.x, translation.world_position.y };
                default_rigidbody_config.angle = localrotation.rot.euler_angle().z / math::RAD2DEG;
                rigidbody.native_rigidbody = m_physics_world.CreateBody(&default_rigidbody_config);
            }

            b2Body* rigidbody_instance = (b2Body*)rigidbody.native_rigidbody;
            assert(rigidbody_instance != nullptr);

            m_all_alive_bodys[rigidbody_instance] = m_simulate_round_count;

            // 检查刚体内的动力学和变换属性，从组件同步到物理引擎
            if (kinematics != nullptr)
            {
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
            }

            if (check_if_need_update_vec2(rigidbody_instance->GetPosition(), math::vec2(translation.world_position.x, translation.world_position.y))
                || check_if_need_update_float(rigidbody_instance->GetAngle() * math::RAD2DEG, translation.world_rotation.euler_angle().z))
            {
                rigidbody_instance->SetTransform(b2Vec2(translation.world_position.x, translation.world_position.y),
                    translation.world_rotation.euler_angle().z / math::RAD2DEG);
                rigidbody_instance->SetAwake(true);
            }

            // 如果实体有 Physics2D::Bullet 组件，那么就适用高精度碰撞
            rigidbody_instance->SetBullet(bullet != nullptr);

            bool need_remove_old_fixture = false;
            auto* old_rigidbody_fixture = rigidbody_instance->GetFixtureList();
            // 开始创建碰撞体

            // 获取实体的网格大小，如果没有，那么默认就是 1，1
            // TODO: 考虑网格本身改变的情况，不过应该没人会去动网格
            auto&& rendshape_mesh_size =
                rendshape && rendshape->vertex != nullptr
                ? math::vec2(
                    rendshape->vertex->resouce()->m_raw_vertex_data->m_size_x,
                    rendshape->vertex->resouce()->m_raw_vertex_data->m_size_y)
                : math::vec2(1.f, 1.f);

            auto entity_scaled_size = rendshape_mesh_size
                * math::vec2(translation.local_scale.x, translation.local_scale.y);

            if (boxcollider != nullptr)
            {
                if (old_rigidbody_fixture == nullptr
                    || old_rigidbody_fixture != boxcollider->native_fixture)
                {
                    // 引擎暂时不支持一个实体有多个fixture，这里标记一下，移除旧的。
                    need_remove_old_fixture = true;

                    // TODO: 此处保存此大小到组件中，如果发现大小发生变化，则重设Fixture，下同
                    auto collider_size = entity_scaled_size * boxcollider->scale;

                    b2PolygonShape box_shape;
                    box_shape.SetAsBox(collider_size.x / 2.f, collider_size.y / 2.f);

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
                    || old_rigidbody_fixture != circlecollider->native_fixture)
                {
                    // 引擎暂时不支持一个实体有多个fixture，这里标记一下，移除旧的。
                    need_remove_old_fixture = true;

                    auto collider_size = entity_scaled_size * circlecollider->scale;

                    b2CircleShape circle_shape;
                    circle_shape.m_radius = std::max(collider_size.x / 2.f, collider_size.y / 2.f);

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
            }
        }

        void PreUpdate()
        {
            b2BodyDef default_rigidbody_config;

            ++m_simulate_round_count;

            select_from(this->get_world())
                .exec(&Physics2DUpdatingSystem::UpdateBeforeSimulate)
                .anyof<Physics2D::BoxCollider, Physics2D::CircleCollider>();

            // 物理引擎在此处进行物理解算
            m_physics_world.Step(input::real_delta_timef(), 8, 3);

            // TODO: 在此处将动力学数据更新到组件上
            select().exec(
                [](
                    Transform::Translation& translation,
                    Transform::LocalPosition& localposition,
                    Transform::LocalRotation& localrotation,
                    Physics2D::Rigidbody& rigidbody,
                    Physics2D::Kinematics* kinematics
                    ) {
                        if (rigidbody.native_rigidbody != nullptr)
                        {
                            // 从刚体获取解算完成之后的坐标
                            b2Body* rigidbody_instance = (b2Body*)rigidbody.native_rigidbody;
                            auto& new_position = rigidbody_instance->GetPosition();

                            localposition.set_world_position(
                                math::vec3(new_position.x, new_position.y, translation.world_position.z),
                                translation, &localrotation);

                            auto&& world_angle = translation.world_rotation.euler_angle();
                            world_angle.z = rigidbody_instance->GetAngle() * math::RAD2DEG;

                            localrotation.set_world_rotation(math::quat::euler(world_angle), translation);

                            // TODO: 检查刚体内的动力学属性，从物理引擎同步到组件

                            if (kinematics != nullptr)
                            {
                                kinematics->linear_velocity = math::vec2{ rigidbody_instance->GetLinearVelocity().x, rigidbody_instance->GetLinearVelocity().y };
                                kinematics->angular_velocity = rigidbody_instance->GetAngularVelocity();
                            }
                        }
                }
            );

            std::vector<b2Body*> _dead_bodys;
            // 这个变量名真恐怖...
            
            for (auto [body, round]: m_all_alive_bodys)
            {
                if (round != m_simulate_round_count)
                    _dead_bodys.push_back(body);
            }
            for (auto dead_body : _dead_bodys)
            {
                m_all_alive_bodys.erase(dead_body);
                m_physics_world.DestroyBody(dead_body);
            }
        }
    };
}
