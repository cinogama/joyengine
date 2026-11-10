#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/basic.hpp"
#include "jeecs/game.hpp"
#include "jeecs/math.hpp"
#include "jeecs/typing.hpp"

#include <box2d/box2d.h>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace jeecs::physics
{
    namespace physics2d_detail
    {
        // ============================================================
        // unordered_map Hash / KeyEqual for b2BodyId (a C struct without
        // operator==), keying PhysicsWorld::bodies. Defined in the cpp on
        // top of Box2D's lossless b2StoreBodyId encoding.
        // ============================================================
        struct B2BodyKeyHash
        {
            size_t operator()(b2BodyId v) const noexcept;
        };
        struct B2BodyKeyEqual
        {
            bool operator()(b2BodyId a, b2BodyId b) const noexcept;
        };

        // ============================================================
        // Collision-group table (max 16 groups, fits in uint16 bitmask).
        // ============================================================
        constexpr size_t MAX_GROUP_COUNT = 16;

        struct group_filter_rule
        {
            const je_type_info_t* m_type;
            je_component_requirement_kind_t     m_requirement;
        };
        struct group_definition
        {
            std::vector<group_filter_rule> m_filters;
            uint16_t                       m_collide_mask; // Bit i set => this group collides with group i.
        };
        struct collision_group_table
        {
            std::array<group_definition, MAX_GROUP_COUNT> m_groups;
            size_t                                         m_count; // Number of valid groups (rest unused).

            collision_group_table()
                : m_groups()
                , m_count(0)
            {
            }
        };

        // ============================================================
        // scene_physics_config: the parsed result of the scene-level
        // physics config script. Layers define the N physics worlds;
        // collide_groups is the collision-rule table shared by ALL layers.
        // ============================================================
        struct scene_physics_config
        {
            struct layer_info
            {
                size_t     layerid;
                math::vec2 gravity;
                size_t     substeps;
                bool       sleep;
                bool       continuous;
            };
            std::vector<layer_info> layers;
            collision_group_table   collide_groups; // shared across every layer
        };
    }

    // ================================================================
    // Physics2DUpdatingSystem
    //
    // Phases per frame:
    //   1. phase_sync_worlds       — create / update / destroy b2WorldId
    //   2. phase_sync_bodies_in    — write component state into box2d
    //   3. phase_step              — b2World_Step (parallel across worlds)
    //   4. phase_sync_bodies_out   — write box2d result back to components
    //   5. phase_collect_contacts  — populate CollisionResult
    // ================================================================
    struct Physics2DUpdatingSystem : public game_system
    {
        struct PhysicsWorld
        {
            b2WorldId                                world;
            size_t                                   layerid;

            // Snapshot of last-applied world config (for diff'ing).
            math::vec2                               gravity_snap;
            size_t                                   substeps_snap;
            bool                                     sleep_snap;
            bool                                     continuous_snap;

            physics2d_detail::collision_group_table  groups;

            // Per-body runtime record. Stable pointer lifetime = life of the entry.
            struct BodyRecord
            {
                game_entity     entity;          // Refreshed every frame in phase_sync_bodies_in.
                b2BodyId        body;
                b2ShapeId       shape;
                // Collider::Mesh decomposes into compound shapes (convex
                // polygon or ear-cut triangles); primitive kinds use `shape`.
                std::vector<b2ShapeId> mesh_shapes;

                // Diff cache: decides whether the fixture must be rebuilt.
                enum class ShapeKind : uint8_t { None, Box, Circle, Capsule, Mesh };
                ShapeKind       cached_kind;
                math::vec2      cached_box_size;
                float           cached_circle_r;
                float           cached_capsule_r;
                float           cached_capsule_h;
                basic::vector<math::vec2> cached_mesh_vertices;
                float           cached_density;
                float           cached_friction;
                float           cached_restitution;
                bool            cached_trigger;

                uint64_t        last_seen_frame;

                BodyRecord()
                    : body()
                    , shape()
                    , cached_kind(ShapeKind::None)
                    , cached_box_size()
                    , cached_circle_r(0.f)
                    , cached_capsule_r(0.f)
                    , cached_capsule_h(0.f)
                    , cached_density(-1.f)
                    , cached_friction(-1.f)
                    , cached_restitution(-1.f)
                    , cached_trigger(false)
                    , last_seen_frame(0)
                {
                }
            };
            std::unordered_map<b2BodyId, BodyRecord,
                physics2d_detail::B2BodyKeyHash,
                physics2d_detail::B2BodyKeyEqual> bodies;

            // Scratch for dead-body sweep.
            std::vector<b2BodyId> dead_scratch;

            PhysicsWorld()
                : world()
                , layerid(0)
                , gravity_snap(math::vec2(0.f, -9.8f))
                , substeps_snap(4)
                , sleep_snap(false)
                , continuous_snap(false)
                , groups()
            {
            }
        };

        // Active worlds keyed by layerid.
        std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>> m_worlds;
        uint64_t                                                   m_frame;

        // Reused scratch buffers (avoids per-frame allocation).
        std::vector<b2ContactData>                                 m_contact_data_scratch;

        // Scene-level config cache. The script is the single source of truth:
        // it defines the layer count, per-layer settings, and the shared
        // collision-group table. Reloaded only when Physics2D::Scene's
        // referenced path changes.
        std::string                                                m_loaded_config_path;
        physics2d_detail::scene_physics_config                     m_scene_config;
        bool                                                       m_config_loaded;

        Physics2DUpdatingSystem(game_world w);
        ~Physics2DUpdatingSystem();

        // ------------------------------------------------------------
        // Phase 1: synchronize worlds.
        // Returns the set of worlds alive this frame (ownership transferred
        // back into m_worlds at the end of PhysicsUpdate).
        // ------------------------------------------------------------
        std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>> phase_sync_worlds();

        // ------------------------------------------------------------
        // Phase 2: write component state into box2d.
        // ------------------------------------------------------------
        void phase_sync_bodies_in(
            std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>>& this_frame);

        // ------------------------------------------------------------
        // Phase 3: step every world. Worlds are independent per Box2D v3,
        // so we parallelize across them.
        // ------------------------------------------------------------
        void phase_step(
            std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>>& this_frame);

        // ------------------------------------------------------------
        // Phase 4: write solver results back to components.
        // ------------------------------------------------------------
        void phase_sync_bodies_out();

        // ------------------------------------------------------------
        // Phase 5: collect contacts into CollisionResult.
        // ------------------------------------------------------------
        void phase_collect_contacts();

        // ------------------------------------------------------------
        // Top-level update.
        // ------------------------------------------------------------
        void PhysicsUpdate();
    };
}
