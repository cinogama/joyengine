#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include "wo.h"

#include <box2d/box2d.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <execution>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace jeecs
{
    using namespace slice_requirement;

    namespace physics2d_detail
    {
        // ============================================================
        // Strongly-typed Box2D id wrappers (replace former reinterpret macros).
        //
        // Box2D already provides official lossless encode/decode helpers
        // (`b2StoreBodyId` / `b2LoadBodyId` / `b2StoreWorldId`), so we just
        // wrap them for hashing & null checks. No type-punning UB.
        // ============================================================
        inline uint64_t b2_body_to_u64(b2BodyId v) noexcept { return b2StoreBodyId(v); }
        inline uint64_t b2_shape_to_u64(b2ShapeId v) noexcept
        {
            // b2ShapeId shares layout with b2BodyId (int32 + uint16 + uint16 = 8B).
            return b2StoreBodyId(*reinterpret_cast<const b2BodyId*>(&v));
        }
        inline b2BodyId  u64_to_b2_body(uint64_t v)  noexcept { return b2LoadBodyId(v); }
        inline b2ShapeId u64_to_b2_shape(uint64_t v) noexcept
        {
            b2BodyId tmp = b2LoadBodyId(v);
            b2ShapeId out;
            std::memcpy(&out, &tmp, sizeof(out));
            return out;
        }

        struct B2BodyKeyHash
        {
            inline size_t operator()(b2BodyId v) const noexcept
            {
                return std::hash<uint64_t>{}(b2_body_to_u64(v));
            }
        };
        struct B2ShapeKeyHash
        {
            inline size_t operator()(b2ShapeId v) const noexcept
            {
                return std::hash<uint64_t>{}(b2_shape_to_u64(v));
            }
        };
        inline bool b2_body_eq(b2BodyId a, b2BodyId b) noexcept
        {
            return b2_body_to_u64(a) == b2_body_to_u64(b);
        }
        inline bool b2_shape_eq(b2ShapeId a, b2ShapeId b) noexcept
        {
            return b2_shape_to_u64(a) == b2_shape_to_u64(b);
        }
        inline bool b2_body_is_null(b2BodyId v) noexcept
        {
            return b2_body_to_u64(v) == 0;
        }
        inline bool b2_world_is_null(b2WorldId v) noexcept
        {
            return b2StoreWorldId(v) == 0;
        }

        // ============================================================
        // Equality predicates for use as unordered_map KeyEqual.
        // (b2BodyId / b2ShapeId are C structs without operator==.)
        // ============================================================
        struct B2BodyKeyEqual
        {
            inline bool operator()(b2BodyId a, b2BodyId b) const noexcept { return b2_body_eq(a, b); }
        };
        struct B2ShapeKeyEqual
        {
            inline bool operator()(b2ShapeId a, b2ShapeId b) const noexcept { return b2_shape_eq(a, b); }
        };

        // ============================================================
        // Collision-group table (max 16 groups, fits in uint16 bitmask).
        // ============================================================
        constexpr size_t MAX_GROUP_COUNT = 16;

        struct group_filter_rule
        {
            const jeecs::typing::type_info* m_type;
            jeecs::requirement::type        m_requirement;
        };
        struct group_definition
        {
            std::vector<group_filter_rule> m_filters;
            uint16_t                       m_collide_mask = 0; // Bit i set => this group collides with group i.
        };
        struct collision_group_table
        {
            std::array<group_definition, MAX_GROUP_COUNT> m_groups;
            size_t                                         m_count = 0; // Number of valid groups (rest unused).
        };

        // ============================================================
        // RAII guard for a woolang VM session: swaps the new VM in on
        // construction, and on destruction restores the prior VM *then*
        // closes the session VM (swap-back-before-close order, matching
        // jeecs_graphic_api_basic.cpp). Owns the VM, so every return path
        // after construction is leak-free.
        // ============================================================
        struct woort_vm_session_guard
        {
            woort_vm* m_prev;
            woort_vm* m_vm;
            explicit woort_vm_session_guard(woort_vm* next)
                : m_prev(woort_vm_swap(next)), m_vm(next) {
            }
            ~woort_vm_session_guard()
            {
                (void)woort_vm_swap(m_prev); // restore the prior VM first
                if (m_vm != nullptr)
                    woort_vm_close(m_vm);     // then release the session VM
            }
            JECS_DISABLE_MOVE_AND_COPY(woort_vm_session_guard);
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
                size_t     layerid = 0;
                math::vec2 gravity = math::vec2(0.f, -9.8f);
                size_t     substeps = 4;
                bool       sleep = false;
                bool       continuous = false;
            };
            std::vector<layer_info> layers;
            collision_group_table   collide_groups; // shared across every layer
        };

        // ============================================================
        // load_scene_physics_config_from_path: pure function that loads
        // and parses a woolang scene-physics config, returning nullopt
        // and logging on any failure.
        //
        // The script must return a struct of shape:
        //   { layers: vec<LayerInfo>, collide_groups: vec<CollideGroupInfo> }
        // where LayerInfo = { layerid: int, gravity: (real, real),
        //                     solver_substeps: int, enable_sleeping: bool,
        //                     enable_continuous: bool }
        //   (field order is significant) and CollideGroupInfo keeps its
        //   original layout: [filter_components, collide_mask].
        // ============================================================
        static std::optional<scene_physics_config> load_scene_physics_config_from_path(
            const std::string& path)
        {
            jeecs_file* fp = jeecs_file_open(path.c_str());
            if (fp == nullptr)
            {
                jeecs::debug::logerr("Unable to open Physics2D scene config: '%s'.", path.c_str());
                return std::nullopt;
            }

            std::vector<char> buf(fp->m_file_length);
            jeecs_file_read(buf.data(), sizeof(char), buf.size(), fp);
            jeecs_file_close(fp);

            wo_CompileErrors* cerr = nullptr;
            woort_CodeEnv* const cenv = wo_load_binary(path.c_str(), buf.data(), buf.size(), &cerr);
            if (cenv == nullptr)
            {
                jeecs::debug::logerr(
                    "Unable to parse Physics2D scene config '%s':\n%s.",
                    path.c_str(),
                    wo_get_compile_error(cerr, WO_PLAIM));
                wo_compile_errors_free(cerr);
                return std::nullopt;
            }

            scene_physics_config result{};
            woort_vm* const vmm = woort_vm_create();
            if (vmm == nullptr)
            {
                jeecs::debug::logerr("Unable to resolve Physics2D scene config: failed to create VM.");
                woort_codeenv_drop(cenv);
                return std::nullopt;
            }

            // Run the entire parse under the session guard so the prior VM is
            // restored and the session VM is closed on every return path.
            woort_vm_session_guard vm_guard{ vmm };

            woort_value s;
            if (!woort_push_reserve(9, &s))
            {
                jeecs::debug::logerr("Unable to resolve Physics2D scene config: stack overflow.");
                woort_codeenv_drop(cenv);
                return std::nullopt;
            }

            const woort_value result_struct = s + 0; // { layers, collide_groups }
            const woort_value layers_vec = s + 1;
            const woort_value groups_vec = s + 2;
            const woort_value layer_slot = s + 3; // reused per layer
            const woort_value gravity_slot = s + 4; // reused (vec2 = nested struct)
            const woort_value group_info = s + 5; // reused per collide group
            const woort_value filter_vec = s + 6; // array<(typeinfo, Requirement)>
            const woort_value filter_value = s + 7; // reused slot for current element
            const woort_value typeinfo_slot = s + 8; // reused

            const woort_VmCallStatus status = woort_bootup_codeenv(result_struct, cenv);
            if (status != WOORT_VM_CALL_STATUS_NORMAL)
            {
                jeecs::debug::logerr(
                    "Unable to resolve Physics2D scene config '%s': %s.",
                    path.c_str(),
                    woort_vm_get_runtime_error(vmm));
                woort_codeenv_drop(cenv);
                return std::nullopt;
            }

            // Unpack the two top-level fields of the returned struct.
            woort_struct_get(layers_vec, result_struct, 0);
            woort_struct_get(groups_vec, result_struct, 1);

            // ---- Layers ----
            const size_t layer_count = woort_vec_len(layers_vec);
            result.layers.clear();
            result.layers.reserve(layer_count);
            for (size_t i = 0; i < layer_count; ++i)
            {
                (void)woort_vec_get(layer_slot, layers_vec, i);

                scene_physics_config::layer_info info{};
                info.layerid = static_cast<size_t>(woort_struct_get_int(layer_slot, 0));

                // Field 1: gravity (vec2 → nested struct, fields 0/1 = x/y).
                woort_struct_get(gravity_slot, layer_slot, 1);
                info.gravity = math::vec2(
                    woort_struct_get_float(gravity_slot, 0),
                    woort_struct_get_float(gravity_slot, 1));
                info.substeps = static_cast<size_t>(woort_struct_get_int(layer_slot, 2));
                info.sleep = woort_struct_get_bool(layer_slot, 3);
                info.continuous = woort_struct_get_bool(layer_slot, 4);
                result.layers.push_back(info);
            }

            if (result.layers.empty())
            {
                jeecs::debug::logwarn(
                    "Physics2D scene config '%s' declared no layers; at least one is required.",
                    path.c_str());
            }

            // ---- Collide groups (shared by every layer) ----
            size_t configed = woort_vec_len(groups_vec);
            if (configed > MAX_GROUP_COUNT)
            {
                jeecs::debug::logwarn(
                    "Physics2D collision groups are limited to %zu; %zu provided in '%s', extras ignored.",
                    MAX_GROUP_COUNT, configed, path.c_str());
                configed = MAX_GROUP_COUNT;
            }
            result.collide_groups.m_count = configed;

            for (size_t i = 0; i < configed; ++i)
            {
                auto& group = result.collide_groups.m_groups[i];

                (void)woort_vec_get(group_info, groups_vec, i);
                // First struct field: filter list of (typeinfo, Requirement)
                woort_struct_get(filter_vec, group_info, 0);

                const size_t filter_count = woort_vec_len(filter_vec);
                group.m_filters.clear();
                group.m_filters.reserve(filter_count);

                for (size_t j = filter_count; j > 0; --j)
                {
                    // Read in reverse so the reused slot has the expected value at the end.
                    (void)woort_vec_get(filter_value, filter_vec, j - 1);

                    woort_value requirement_slot = filter_value;
                    woort_struct_get(typeinfo_slot, filter_value, 0); // typeinfo
                    woort_struct_get(requirement_slot, filter_value, 1); // Requirement

                    group.m_filters.push_back(group_filter_rule{
                        static_cast<const typing::type_info*>(woort_pointer(typeinfo_slot)),
                        static_cast<requirement::type>(woort_int(requirement_slot)),
                        });
                }

                // Second struct field: collide mask.
                group.m_collide_mask = static_cast<uint16_t>(woort_struct_get_int(group_info, 1));
            }

            woort_codeenv_drop(cenv);
            return result;
        }

        // ============================================================
        // Compute self category/mask bits for an entity, given a group table.
        // categoryBits = OR of (1 << i) for each group i the entity matches.
        // maskBits      = OR of each matched group's collide_mask.
        // ============================================================
        inline void compute_collision_filter(
            const collision_group_table& table,
            const game_entity& e,
            uint64_t& out_category,
            uint64_t& out_mask)
        {
            uint64_t cat = 0;
            uint64_t msk = 0;
            for (size_t i = 0; i < table.m_count; ++i)
            {
                const auto& g = table.m_groups[i];
                bool matches = true;
                for (const auto& f : g.m_filters)
                {
                    assert(f.m_requirement == requirement::type::CONTAINS
                        || f.m_requirement == requirement::type::EXCEPT);

                    const bool has = je_ecs_world_entity_get_component(&e, f.m_type->m_id) != nullptr;
                    if ((f.m_requirement == requirement::type::CONTAINS) != has)
                    {
                        matches = false;
                        break;
                    }
                }
                if (matches)
                {
                    cat |= (uint64_t(1) << i);
                    msk |= uint64_t(g.m_collide_mask);
                }
            }
            out_category = cat;
            out_mask = msk;
        }
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
            b2WorldId                                world{};
            size_t                                   layerid = 0;

            // Snapshot of last-applied world config (for diff'ing).
            math::vec2                               gravity_snap = math::vec2(0.f, -9.8f);
            size_t                                   substeps_snap = 4;
            bool                                     sleep_snap = false;
            bool                                     continuous_snap = false;

            physics2d_detail::collision_group_table  groups{};

            // Per-body runtime record. Stable pointer lifetime = life of the entry.
            struct BodyRecord
            {
                game_entity     entity;          // Refreshed every frame in phase_sync_bodies_in.
                b2BodyId        body{};
                b2ShapeId       shape{};

                // Diff cache: decides whether the fixture must be rebuilt.
                enum class ShapeKind : uint8_t { None, Box, Circle, Capsule };
                ShapeKind       cached_kind = ShapeKind::None;
                math::vec2      cached_box_size = {};
                float           cached_circle_r = 0.f;
                float           cached_capsule_r = 0.f;
                float           cached_capsule_h = 0.f;
                float           cached_density = -1.f;
                float           cached_friction = -1.f;
                float           cached_restitution = -1.f;
                bool            cached_trigger = false;

                uint64_t        last_seen_frame = 0;
            };
            std::unordered_map<b2BodyId, BodyRecord,
                physics2d_detail::B2BodyKeyHash,
                physics2d_detail::B2BodyKeyEqual> bodies;

            // Scratch for dead-body sweep.
            std::vector<b2BodyId> dead_scratch;
        };

        // Active worlds keyed by layerid.
        std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>> m_worlds;
        uint64_t                                                   m_frame = 0;

        // Reused scratch buffers (avoids per-frame allocation).
        std::vector<b2ContactData>                                 m_contact_data_scratch;

        // Scene-level config cache. The script is the single source of truth:
        // it defines the layer count, per-layer settings, and the shared
        // collision-group table. Reloaded only when Physics2D::Scene's
        // referenced path changes.
        std::string                                                m_loaded_config_path;
        physics2d_detail::scene_physics_config                     m_scene_config{};
        bool                                                       m_config_loaded = false;

        Physics2DUpdatingSystem(game_world w)
            : game_system(w)
        {
        }

        ~Physics2DUpdatingSystem()
        {
            for (auto& [_, pw] : m_worlds)
                if (pw && !physics2d_detail::b2_world_is_null(pw->world))
                    b2DestroyWorld(pw->world);
        }

        // ------------------------------------------------------------
        // Helpers
        // ------------------------------------------------------------
        inline static bool need_update_vec2(const b2Vec2& a, const math::vec2& b) noexcept
        {
            return !math::almost_equal(a.x, b.x) || !math::almost_equal(a.y, b.y);
        }
        inline static bool need_update_float(float a, float b) noexcept
        {
            return !math::almost_equal(a, b);
        }

        // Resolve final body pose (world-space) from Translation + Offset components.
        inline static void compute_body_pose(
            const Transform::Translation& trans,
            const Physics2D::Offset::Position* opos,
            const Physics2D::Offset::Rotation* orot,
            math::vec2& out_pos,
            float& out_rot_deg)
        {
            const math::vec2 offset_pos = opos ? opos->value : math::vec2(0.f);
            const float      extra_rot = orot ? orot->degree : 0.f;

            const float world_rot_deg =
                trans.world_rotation.euler_angle().z + extra_rot;

            // Rotate the local offset into world space, then add to world position.
            const math::vec3 rotated = math::quat::euler(0.f, 0.f, world_rot_deg)
                * math::vec3(offset_pos.x, offset_pos.y, 0.f);

            out_pos = math::vec2(trans.world_position.x + rotated.x,
                trans.world_position.y + rotated.y);
            out_rot_deg = world_rot_deg;
        }

        // ------------------------------------------------------------
        // Phase 1: synchronize worlds.
        // Returns the set of worlds alive this frame (ownership transferred
        // back into m_worlds at the end of PhysicsUpdate).
        // ------------------------------------------------------------
        std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>> phase_sync_worlds()
        {
            std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>> this_frame;

            // ---- Resolve the scene config from the (single) Physics2D::Scene entity. ----
            std::string current_path;
            size_t      scene_count = 0;
            for (auto&& [e, scene] :
                query_entity<view typesof(Physics2D::Scene&)>())
            {
                if (++scene_count == 1 && scene.physics_config.has_resource())
                {
                    auto path_opt = scene.physics_config.get_path();
                    if (path_opt.has_value())
                        current_path = path_opt.value();
                }
            }

            if (scene_count == 0)
            {
                // No scene config present: tear down everything and drop the cache.
                m_config_loaded = false;
                m_loaded_config_path.clear();
                m_scene_config = {};
                return this_frame;
            }
            if (scene_count > 1)
            {
                jeecs::debug::logerr(
                    "Multiple Physics2D::Scene entities found (%zu); only the first will be used.",
                    scene_count);
            }

            // Reload only when the referenced path changes (or on first use).
            if (current_path != m_loaded_config_path)
            {
                if (current_path.empty())
                {
                    m_config_loaded = false;
                    m_loaded_config_path.clear();
                    m_scene_config = {};
                }
                else
                {
                    auto loaded =
                        physics2d_detail::load_scene_physics_config_from_path(current_path);
                    if (loaded.has_value())
                    {
                        m_scene_config = std::move(loaded.value());
                        m_config_loaded = true;
                    }

                    // Remember the bad path so we don't retry every frame,
                    // but keep the previously loaded config (if any) alive.
                    m_loaded_config_path = current_path;
                }
            }

            if (!m_config_loaded || m_scene_config.layers.empty())
                return this_frame;

            // ---- Build / reuse one b2World per declared layer. ----
            // Each layer carries its own user-declared id (layerid), which is
            // what Rigidbody::layerid binds to — independent of declaration order.
            for (const auto& layer : m_scene_config.layers)
            {
                const size_t lid = layer.layerid;

                if (this_frame.find(lid) != this_frame.end())
                {
                    jeecs::debug::logerr(
                        "Duplicate Physics2D layer id: %zu, the second declaration is ignored.", lid);
                    continue;
                }

                auto prev_it = m_worlds.find(lid);
                std::unique_ptr<PhysicsWorld> pw;
                if (prev_it == m_worlds.end())
                {
                    pw = std::make_unique<PhysicsWorld>();
                    b2WorldDef wdef = b2DefaultWorldDef();
                    pw->world = b2CreateWorld(&wdef);
                    pw->layerid = lid;

                    pw->gravity_snap = layer.gravity;
                    pw->substeps_snap = layer.substeps;
                    pw->sleep_snap = layer.sleep;
                    pw->continuous_snap = layer.continuous;

                    b2World_SetGravity(pw->world, b2Vec2{ layer.gravity.x, layer.gravity.y });
                    b2World_EnableSleeping(pw->world, layer.sleep);
                    b2World_EnableContinuous(pw->world, layer.continuous);
                }
                else
                {
                    pw = std::move(prev_it->second);
                    // (prev_it entry is left empty; m_worlds is replaced at end of frame.)
                }

                // The collision-group table is shared across all layers.
                pw->groups = m_scene_config.collide_groups;

                // Diff config & apply.
                if (need_update_vec2(b2World_GetGravity(pw->world), layer.gravity))
                {
                    b2World_SetGravity(pw->world, b2Vec2{ layer.gravity.x, layer.gravity.y });
                    // Awake every body so the new gravity takes effect immediately.
                    for (auto& [_, rec] : pw->bodies)
                        b2Body_SetAwake(rec.body, true);
                }
                if (pw->sleep_snap != layer.sleep)
                {
                    b2World_EnableSleeping(pw->world, layer.sleep);
                    pw->sleep_snap = layer.sleep;
                }
                if (pw->continuous_snap != layer.continuous)
                {
                    b2World_EnableContinuous(pw->world, layer.continuous);
                    pw->continuous_snap = layer.continuous;
                }
                pw->substeps_snap = layer.substeps;

                this_frame.emplace(lid, std::move(pw));
            }

            return this_frame;
        }

        // ------------------------------------------------------------
        // Phase 2: write component state into box2d.
        // ------------------------------------------------------------
        void phase_sync_bodies_in(
            std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>>& this_frame)
        {
            for (auto&& [
                e, trans, rb, dyn, kinem, bullet,
                lockx, locky, lockr,
                lvel, avel, ldamp, adamp, gscale,
                box, circle, capsule,
                density, friction, restitution, trigger,
                opos, orot, oscale
            ] : query_entity <
                view typesof(
                    Transform::Translation&,
                    Physics2D::Rigidbody&,
                    Physics2D::DynamicBody*,
                    Physics2D::KinematicBody*,
                    Physics2D::Bullet*,
                    Physics2D::LockTranslationX*,
                    Physics2D::LockTranslationY*,
                    Physics2D::LockRotation*,
                    Physics2D::LinearVelocity*,
                    Physics2D::AngularVelocity*,
                    Physics2D::LinearDamping*,
                    Physics2D::AngularDamping*,
                    Physics2D::GravityScale*,
                    Physics2D::Collider::Box*,
                    Physics2D::Collider::Circle*,
                    Physics2D::Collider::Capsule*,
                    Physics2D::Density*,
                    Physics2D::Friction*,
                    Physics2D::Restitution*,
                    Physics2D::IsTrigger*,
                    Physics2D::Offset::Position*,
                    Physics2D::Offset::Rotation*,
                    Physics2D::Offset::Scale*
                ),
                anyof typesof(
                    Physics2D::Collider::Box,
                    Physics2D::Collider::Circle,
                    Physics2D::Collider::Capsule
                )
            > ())
            {
                auto world_it = this_frame.find(rb.layerid);
                if (world_it == this_frame.end() || world_it->second == nullptr)
                {
                    jeecs::debug::logerr(
                        "Rigidbody belongs to layer %zu, but the Physics2D scene config does not "
                        "define a layer with that index (declared layer count: %zu).",
                        rb.layerid, m_scene_config.layers.size());
                    continue;
                }
                PhysicsWorld* pw = world_it->second.get();

                // Resolve final pose.
                math::vec2 want_pos;
                float       want_rot_deg;
                compute_body_pose(trans, opos, orot, want_pos, want_rot_deg);

                // BodyType resolution.
                b2BodyType want_type = b2_staticBody;
                if (dyn && kinem)
                {
                    jeecs::debug::logerr(
                        "Entity has both DynamicBody and KinematicBody; treating as Dynamic.");
                    want_type = b2_dynamicBody;
                }
                else if (dyn)
                    want_type = b2_dynamicBody;
                else if (kinem)
                    want_type = b2_kinematicBody;

                // Locate-or-create the BodyRecord by entity identity.
                // We linear-scan the per-world body map (worlds usually have
                // bounded body counts; for very large worlds a secondary
                // entity->record index can be added later).
                PhysicsWorld::BodyRecord* rec = nullptr;
                for (auto& [bid, r] : pw->bodies)
                {
                    if (r.entity == e)
                    {
                        rec = &r;
                        break;
                    }
                }

                if (rec == nullptr)
                {
                    // First time we see this entity: create the b2Body.
                    b2BodyDef bdef = b2DefaultBodyDef();
                    bdef.type = want_type;
                    bdef.position = b2Vec2{ want_pos.x, want_pos.y };
                    bdef.rotation = b2MakeRot(want_rot_deg * math::DEG2RAD);

                    b2BodyId new_body = b2CreateBody(pw->world, &bdef);
                    if (physics2d_detail::b2_body_is_null(new_body))
                    {
                        jeecs::debug::logerr("b2CreateBody failed for entity on layer %zu.", rb.layerid);
                        continue;
                    }

                    PhysicsWorld::BodyRecord nr{};
                    nr.entity = e;
                    nr.body = new_body;
                    nr.shape = b2_nullShapeId;
                    auto inserted = pw->bodies.emplace(new_body, std::move(nr));
                    rec = &inserted.first->second;
                }
                rec->last_seen_frame = m_frame;
                rec->entity = e; // Refresh handle every frame.

                // Publish the stable body_id so any system phase (including
                // ones running before the next PhysicsUpdate) can use it as a
                // cross-frame identifier when looking up CollisionResult.
                rb.body_id = physics2d_detail::b2_body_to_u64(rec->body);

                const b2BodyId body = rec->body;

                // BodyType.
                b2Body_SetType(body, want_type);
                b2Body_SetBullet(body, bullet != nullptr);
                b2Body_SetFixedRotation(body, lockr != nullptr);
                if (lockr != nullptr)
                    b2Body_SetAwake(body, true);

                // Velocity / damping / gravity scale.
                if (lvel)
                {
                    const b2Vec2 want_lv{ lvel->value.x, lvel->value.y };
                    if (need_update_vec2(b2Body_GetLinearVelocity(body), lvel->value))
                        b2Body_SetLinearVelocity(body, want_lv);
                }
                if (avel)
                {
                    if (need_update_float(b2Body_GetAngularVelocity(body), avel->value))
                        b2Body_SetAngularVelocity(body, avel->value);
                }
                if (ldamp)
                {
                    if (need_update_float(b2Body_GetLinearDamping(body), ldamp->value))
                        b2Body_SetLinearDamping(body, ldamp->value);
                }
                if (adamp)
                {
                    if (need_update_float(b2Body_GetAngularDamping(body), adamp->value))
                        b2Body_SetAngularDamping(body, adamp->value);
                }
                if (gscale)
                {
                    if (need_update_float(b2Body_GetGravityScale(body), gscale->value))
                        b2Body_SetGravityScale(body, gscale->value);
                }

                // Pose.
                const bool pos_changed = need_update_vec2(
                    b2Body_GetPosition(body), want_pos);
                const bool rot_changed = need_update_float(
                    b2Rot_GetAngle(b2Body_GetRotation(body)) * math::RAD2DEG, want_rot_deg);
                if (pos_changed || rot_changed)
                {
                    b2Body_SetTransform(
                        body,
                        b2Vec2{ want_pos.x, want_pos.y },
                        b2MakeRot(want_rot_deg * math::DEG2RAD));
                    b2Body_SetAwake(body, true);
                }

                // ---- Fixture rebuild decision ----
                const float want_density = density ? density->value : 0.f;
                const float want_friction = friction ? friction->value : 0.f;
                const float want_restitution = restitution ? restitution->value : 0.f;
                const bool  want_trigger = trigger != nullptr;

                PhysicsWorld::BodyRecord::ShapeKind want_kind =
                    PhysicsWorld::BodyRecord::ShapeKind::None;
                if (box)     want_kind = PhysicsWorld::BodyRecord::ShapeKind::Box;
                else if (circle)  want_kind = PhysicsWorld::BodyRecord::ShapeKind::Circle;
                else if (capsule) want_kind = PhysicsWorld::BodyRecord::ShapeKind::Capsule;

                bool force_rebuild = (rec->cached_kind != want_kind);
                if (!force_rebuild)
                {
                    // Diff geometry.
                    switch (want_kind)
                    {
                    case PhysicsWorld::BodyRecord::ShapeKind::Box:
                        if (rec->cached_box_size != box->size) force_rebuild = true;
                        break;
                    case PhysicsWorld::BodyRecord::ShapeKind::Circle:
                        if (rec->cached_circle_r != circle->radius) force_rebuild = true;
                        break;
                    case PhysicsWorld::BodyRecord::ShapeKind::Capsule:
                        if (rec->cached_capsule_r != capsule->radius
                            || rec->cached_capsule_h != capsule->height) force_rebuild = true;
                        break;
                    default: break;
                    }
                    if (!force_rebuild)
                    {
                        if (rec->cached_density != want_density
                            || rec->cached_friction != want_friction
                            || rec->cached_restitution != want_restitution
                            || rec->cached_trigger != want_trigger)
                            force_rebuild = true;
                    }
                }

                if (force_rebuild)
                {
                    // Destroy old shape if any.
                    if (!physics2d_detail::b2_shape_eq(rec->shape, b2_nullShapeId))
                        b2DestroyShape(rec->shape, true);

                    b2ShapeDef sdef = b2DefaultShapeDef();
                    sdef.density = want_density;
                    sdef.material.friction = want_friction;
                    sdef.material.restitution = want_restitution;
                    sdef.isSensor = want_trigger;

                    b2ShapeId new_shape = b2_nullShapeId;
                    switch (want_kind)
                    {
                    case PhysicsWorld::BodyRecord::ShapeKind::Box:
                    {
                        b2Polygon box_poly = b2MakeBox(
                            std::abs(box->size.x) * 0.5f,
                            std::abs(box->size.y) * 0.5f);
                        new_shape = b2CreatePolygonShape(body, &sdef, &box_poly);
                        rec->cached_box_size = box->size;
                        break;
                    }
                    case PhysicsWorld::BodyRecord::ShapeKind::Circle:
                    {
                        b2Circle c;
                        c.center = b2Vec2{ 0.f, 0.f };
                        c.radius = std::abs(circle->radius);
                        new_shape = b2CreateCircleShape(body, &sdef, &c);
                        rec->cached_circle_r = circle->radius;
                        break;
                    }
                    case PhysicsWorld::BodyRecord::ShapeKind::Capsule:
                    {
                        const float r = std::abs(capsule->radius);
                        const float h = std::abs(capsule->height) * 0.5f;
                        b2Capsule cap;
                        cap.radius = r;
                        // Two center points symmetric about origin; total height = 2h.
                        if (h > r)
                        {
                            cap.center1 = b2Vec2{ 0.f,  h - r };
                            cap.center2 = b2Vec2{ 0.f, -h + r };
                        }
                        else
                        {
                            // Capsule degenerates to a circle.
                            cap.center1 = b2Vec2{ 0.f, 0.f };
                            cap.center2 = b2Vec2{ 0.f, 0.f };
                        }
                        new_shape = b2CreateCapsuleShape(body, &sdef, &cap);
                        rec->cached_capsule_r = capsule->radius;
                        rec->cached_capsule_h = capsule->height;
                        break;
                    }
                    default:
                        assert(false && "Unreachable: want_kind must be set by anyof constraint.");
                        break;
                    }

                    rec->shape = new_shape;
                    rec->cached_kind = want_kind;
                    rec->cached_density = want_density;
                    rec->cached_friction = want_friction;
                    rec->cached_restitution = want_restitution;
                    rec->cached_trigger = want_trigger;

                    // Apply collision-group filter (categoryBits/maskBits).
                    uint64_t cat = 0, msk = 0;
                    physics2d_detail::compute_collision_filter(pw->groups, e, cat, msk);
                    b2Filter filter = b2Shape_GetFilter(new_shape);
                    filter.categoryBits = cat;
                    filter.maskBits = msk;
                    b2Shape_SetFilter(new_shape, filter);
                }
            }
        }

        // ------------------------------------------------------------
        // Phase 3: step every world. Worlds are independent per Box2D v3,
        // so we parallelize across them.
        // ------------------------------------------------------------
        void phase_step(
            std::unordered_map<size_t, std::unique_ptr<PhysicsWorld>>& this_frame)
        {
            std::vector<PhysicsWorld*> worlds;
            worlds.reserve(this_frame.size());
            for (auto& [_, pw] : this_frame) worlds.push_back(pw.get());

            const float    dt = deltatime();
            const uint64_t frame = m_frame;

            std::for_each(
                std::execution::par_unseq,
                worlds.begin(), worlds.end(),
                [dt, frame](PhysicsWorld* pw)
                {
                    const int substeps = static_cast<int>(
                        pw->substeps_snap == 0 ? 1 : pw->substeps_snap);
                    b2World_Step(pw->world, dt, substeps);

                    // Sweep dead bodies (records not refreshed this frame).
                    pw->dead_scratch.clear();
                    for (auto& [bid, rec] : pw->bodies)
                        if (rec.last_seen_frame != frame)
                            pw->dead_scratch.push_back(bid);
                    for (b2BodyId dead : pw->dead_scratch)
                    {
                        pw->bodies.erase(dead);
                        b2DestroyBody(dead);
                    }
                });
        }

        // ------------------------------------------------------------
        // Phase 4: write solver results back to components.
        // ------------------------------------------------------------
        void phase_sync_bodies_out()
        {
            for (auto&& [
                e, trans, localpos, localrot,
                rb, lvel, avel,
                lockx, locky, lockr,
                opos, orot
            ] : query_entity<view typesof(
                Transform::Translation&,
                Transform::LocalPosition&,
                Transform::LocalRotation&,
                Physics2D::Rigidbody&,
                Physics2D::LinearVelocity*,
                Physics2D::AngularVelocity*,
                Physics2D::LockTranslationX*,
                Physics2D::LockTranslationY*,
                Physics2D::LockRotation*,
                Physics2D::Offset::Position*,
                Physics2D::Offset::Rotation*
            )>())
            {
                auto world_it = m_worlds.find(rb.layerid);
                if (world_it == m_worlds.end())
                    continue;
                PhysicsWorld* pw = world_it->second.get();

                // Find this entity's BodyRecord.
                PhysicsWorld::BodyRecord* rec = nullptr;
                for (auto& [bid, r] : pw->bodies)
                    if (r.entity == e) { rec = &r; break; }
                if (rec == nullptr)
                    continue;

                const b2BodyId body = rec->body;
                const b2Vec2   new_pos = b2Body_GetPosition(body);
                const b2Rot    new_rot = b2Body_GetRotation(body);
                const float    new_rot_deg = b2Rot_GetAngle(new_rot) * math::RAD2DEG;

                // Recover the local offset rotation that was applied on the way in,
                // so the rotation we write back is the entity's own (without offset).
                const float extra_rot = orot ? orot->degree : 0.f;
                const math::vec2 offset_pos = opos ? opos->value : math::vec2(0.f);
                const float entity_rot_deg = new_rot_deg - extra_rot;

                // Rotate offset back into local space to subtract from world pos.
                const math::vec3 rotated = math::quat::euler(0.f, 0.f, entity_rot_deg)
                    * math::vec3(offset_pos.x, offset_pos.y, 0.f);

                const bool lock_x = lockx != nullptr;
                const bool lock_y = locky != nullptr;

                const float out_x = lock_x ? trans.world_position.x : (new_pos.x - rotated.x);
                const float out_y = lock_y ? trans.world_position.y : (new_pos.y - rotated.y);

                trans.set_global_position(
                    math::vec3(out_x, out_y, trans.world_position.z),
                    &localpos, &localrot);

                if (!lockr)
                {
                    auto euler = trans.world_rotation.euler_angle();
                    euler.z = entity_rot_deg;
                    trans.set_global_rotation(math::quat::euler(euler), &localrot);
                }

                if (lvel)
                {
                    const b2Vec2 lv = b2Body_GetLinearVelocity(body);
                    lvel->value = math::vec2(
                        lock_x ? 0.f : lv.x,
                        lock_y ? 0.f : lv.y);
                }
                if (avel && !lockr)
                    avel->value = b2Body_GetAngularVelocity(body);
                else if (avel && lockr)
                    avel->value = 0.f;
            }
        }

        // ------------------------------------------------------------
        // Phase 5: collect contacts into CollisionResult.
        // ------------------------------------------------------------
        void phase_collect_contacts()
        {
            for (auto&& [e, rb, cr] : query_entity<view typesof(
                Physics2D::Rigidbody&,
                Physics2D::CollisionResult&
            )>())
            {
                auto world_it = m_worlds.find(rb.layerid);
                if (world_it == m_worlds.end())
                {
                    cr.contacts.clear();
                    continue;
                }
                PhysicsWorld* pw = world_it->second.get();

                // Find this entity's BodyRecord.
                PhysicsWorld::BodyRecord* self_rec = nullptr;
                for (auto& [bid, r] : pw->bodies)
                    if (r.entity == e) { self_rec = &r; break; }
                if (self_rec == nullptr)
                {
                    cr.contacts.clear();
                    continue;
                }

                const b2BodyId self_body = self_rec->body;

                cr.contacts.clear();
                const int cap = b2Body_GetContactCapacity(self_body);
                if (cap <= 0)
                    continue;

                if (static_cast<size_t>(cap) > m_contact_data_scratch.size())
                    m_contact_data_scratch.resize(static_cast<size_t>(cap));

                const int n = b2Body_GetContactData(
                    self_body, m_contact_data_scratch.data(), cap);
                assert(n >= 0 && (size_t)n <= m_contact_data_scratch.size());

                for (int i = 0; i < n; ++i)
                {
                    const b2ContactData& cd = m_contact_data_scratch[i];
                    if (cd.manifold.pointCount <= 0)
                        continue;

                    // Determine which side is self.
                    const b2BodyId body_a = b2Shape_GetBody(cd.shapeIdA);
                    const bool     self_is_a = physics2d_detail::b2_body_eq(body_a, self_body);

                    const b2ShapeId other_shape = self_is_a ? cd.shapeIdB : cd.shapeIdA;
                    const b2BodyId  other_body = b2Shape_GetBody(other_shape);

                    // Stable cross-frame id for the other body. This is what
                    // CollisionResult consumers compare against Rigidbody::body_id.
                    const uint64_t other_body_id = physics2d_detail::b2_body_to_u64(other_body);

                    // Resolve whether the other side is a sensor (IsTrigger).
                    // We rely on the cached_trigger flag written in phase_sync_bodies_in
                    // to avoid depending on extra Box2D query APIs.
                    bool other_is_trigger = false;
                    auto other_it = pw->bodies.find(other_body);
                    if (other_it != pw->bodies.end())
                        other_is_trigger = other_it->second.cached_trigger;

                    // Normal points from A to B in Box2D convention.
                    // We want it to point from self to other.
                    const math::vec2 normal = self_is_a
                        ? math::vec2(cd.manifold.normal.x, cd.manifold.normal.y)
                        : math::vec2(-cd.manifold.normal.x, -cd.manifold.normal.y);

                    // Emit one Contact per manifold point (Box2D gives up to 2).
                    for (int p = 0; p < cd.manifold.pointCount; ++p)
                    {
                        const b2ManifoldPoint& mp = cd.manifold.points[p];
                        Physics2D::CollisionResult::Contact out{};
                        out.other_body_id = other_body_id;
                        out.point = math::vec2(mp.point.x, mp.point.y); // already world-space
                        out.normal = normal;
                        out.normal_impulse = mp.normalImpulse;
                        out.tangent_impulse = mp.tangentImpulse;
                        out.is_trigger = other_is_trigger;
                        cr.contacts.push_back(out);
                    }
                }
            }
        }

        // ------------------------------------------------------------
        // Top-level update.
        // ------------------------------------------------------------
        void PhysicsUpdate()
        {
            ++m_frame;

            auto this_frame = phase_sync_worlds();
            phase_sync_bodies_in(this_frame);
            phase_step(this_frame);

            // Promote this_frame into m_worlds (drops worlds not seen this frame).
            m_worlds = std::move(this_frame);

            // Now query against the promoted m_worlds for write-back & contact collection.
            phase_sync_bodies_out();
            phase_collect_contacts();
        }
    };
}
