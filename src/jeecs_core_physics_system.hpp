#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <box2d/box2d.h>

namespace jeecs
{
    struct Physics2DUpdatingSystem :public game_system
    {
        b2World m_physics_world;

        Physics2DUpdatingSystem(game_world world) :game_system(world)
            , m_physics_world(b2Vec2(0.f, -9.8f))
        {
            m_physics_world.SetAllowSleeping(true);
            m_physics_world.SetContinuousPhysics(true);
        }

        void PreUpdate()
        {
            b2BodyDef default_rigidbody_config;
            select_from(this->get_world()).exec(
                [this, &default_rigidbody_config](
                    Transform::Translation& translation,
                    Physics2D::Rigidbody& rigidbody,
                    Physics2D::Kinematics* kinematics,
                    Physics2D::Collider* collider,
                    Physics2D::Bullet* bullet
                    )
                {
                    if (rigidbody.native_rigidbody == nullptr)
                        rigidbody.native_rigidbody = m_physics_world.CreateBody(&default_rigidbody_config);

                    b2Body* rigidbody_instance = (b2Body*)rigidbody.native_rigidbody;

                    rigidbody_instance->SetBullet(bullet != nullptr);

                    if (kinematics == nullptr)
                    {
                        rigidbody_instance->SetType(b2_staticBody);
                        // rigidbody_instance->mass
                    }
                    else
                    {
                        if (kinematics->mass == 0.f)
                            rigidbody_instance->SetType(b2_kinematicBody);
                        else
                            rigidbody_instance->SetType(b2_dynamicBody);
                        // rigidbody_instance->mass
                    }
                });
        }

        void Update()
        {
        }

        void LateUpdate()
        {
        }
    };
}
