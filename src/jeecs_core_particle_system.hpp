#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>

namespace jeecs
{
    using namespace Particle;
    using namespace Renderer;
    using namespace Transform;

    /*
    ParticleSystem [系统]
    粒子模拟系统：驱动 Particle::Emitter 组件的发射、积分与网格构建。

    工作方式：
      * 每个发射器实体在本系统内持有一个 CPU 粒子池（发射器粒子上限容量的
        SoA 数组），死亡粒子以交换删除方式紧凑排布；
      * 发射器实体复用常规渲染组件：系统把存活粒子构建为面向摄像机的公告板
        四边形网格（每粒子 4 顶点 6 索引），经 jegl_update_vertex_buffer 逐帧
        写入 Renderer::Shape，由常规图形管线（如 UnlitGraphicPipelineSystem）
        随实体一起绘制；粒子纹理绑定在 Renderer::Textures 的 pass 0；
      * 公告板朝向在粒子着色器中用视图矩阵展开（顶点属性携带中心
        位置/颜色/已自转缩放的角点），CPU 只做模拟，因此多摄像机自动正确；
      * 模拟空间：世界空间下粒子位置存于世界坐标，构建网格时经发射器
        逆变换转回局部（发射器移动不携带已发射粒子）；局部空间下粒子
        直接存于发射器局部坐标（整体随发射器刚体运动），此时 direction
        与 gravity 按发射器局部坐标系解释。
    运行相位：Update（在物理与渲染之前完成数据构建）。
    */
    struct ParticleSystem : public game_system
    {
        // 公告板四边形的角点与 UV（TRIANGLES，索引模式 0,1,2, 2,1,3）；
        // 角点在构建时按粒子自转旋转并乘以尺寸，着色器只需沿摄像机
        // 平面展开（见 builtin/shader/ParticleBlend.shader）
        inline static const float CORNER_UV[4][4] = {
            /* corner_x, corner_y, uv_x, uv_y */
            { -1.f, -1.f, 0.f, 0.f },
            { 1.f, -1.f, 1.f, 0.f },
            { -1.f, 1.f, 0.f, 1.f },
            { 1.f, 1.f, 1.f, 1.f },
        };
        // 顶点布局：中心位置(3) + 颜色(4) + 角点偏移(2) + UV(2)
        inline constexpr static size_t FLOATS_PER_VERTEX = 11;
        inline constexpr static size_t MAX_PARTICLE_LIMIT = 65536;

        // ===== 每发射器粒子池（SoA，存于模拟坐标系） =====
        struct particle_pool
        {
            std::vector<float> pos_x, pos_y, pos_z;
            std::vector<float> vel_x, vel_y, vel_z;
            std::vector<float> age, lifetime;
            std::vector<float> base_size, rot, rot_speed;
            std::vector<math::vec4> base_color;

            size_t alive_count = 0;
            size_t capacity = 0;

            double spawn_accumulator = 0.0;
            double burst_timer = 0.0;
            bool dirty = true; // 池新建/重置后至少完整上传一次

            basic::optional<basic::resource<graphic::vertex>> gpu_vertex;
            std::vector<float> staging; // capacity * 4 顶点 * FLOATS_PER_VERTEX

            void reset()
            {
                alive_count = 0;
                spawn_accumulator = 0.0;
                burst_timer = 0.0;
                dirty = true;
            }

            void resize(size_t new_capacity)
            {
                capacity = new_capacity;
                pos_x.resize(capacity); pos_y.resize(capacity); pos_z.resize(capacity);
                vel_x.resize(capacity); vel_y.resize(capacity); vel_z.resize(capacity);
                age.resize(capacity); lifetime.resize(capacity);
                base_size.resize(capacity); rot.resize(capacity); rot_speed.resize(capacity);
                base_color.resize(capacity);
                staging.assign(capacity * 4 * FLOATS_PER_VERTEX, 0.0f);
                reset();
            }
        };

        std::unordered_map<uint32_t, particle_pool> _m_pools;
        std::vector<uint32_t> _m_alive_emitter_ids;
        basic::optional<basic::resource<graphic::shader>> _m_default_particle_shader;

        ParticleSystem(game_world w)
            : game_system(w)
        {
        }

        const basic::resource<graphic::shader>& default_particle_shader()
        {
            if (!_m_default_particle_shader.has_value())
            {
                auto loaded = graphic::shader::load(
                    nullptr, "!/builtin/shader/ParticleBlend.shader");
                if (loaded.has_value())
                    _m_default_particle_shader.emplace(std::move(loaded.value()));
                else
                    jeecs::debug::logerr(
                        "ParticleSystem: failed to load builtin shader "
                        "'!/builtin/shader/ParticleBlend.shader'.");
            }
            return _m_default_particle_shader.value();
        }

        // 在 axis 周围半角 spread_rad 的圆锥内均匀取方向；spread 覆盖全球面时
        // 退化为均匀球面采样
        static math::vec3 random_direction_around(const math::vec3& axis, float spread_rad)
        {
            math::vec3 dir = axis.length() > math::EPSILON ? axis.unit()
                : math::vec3(0.f, 1.f, 0.f);

            if (spread_rad >= math::PI)
            {
                // 均匀球面：z ∈ [-1,1]，φ ∈ [0,2π)
                float z = math::random(-1.0f, 1.0f);
                float phi = math::random(0.0f, 2.f * math::PI);
                float r = std::sqrt(std::max(0.f, 1.f - z * z));
                return math::vec3(r * std::cos(phi), r * std::sin(phi), z);
            }

            // 圆锥内余弦均匀：cosθ ∈ [cos(spread), 1]
            float cos_theta = math::lerp(
                std::cos(spread_rad), 1.0f, math::random(0.0f, 1.0f));
            float sin_theta = std::sqrt(std::max(0.f, 1.f - cos_theta * cos_theta));
            float phi = math::random(0.0f, 2.f * math::PI);

            math::vec3 helper = std::abs(dir.y) < 0.9f
                ? math::vec3(0.f, 1.f, 0.f)
                : math::vec3(1.f, 0.f, 0.f);
            math::vec3 u = dir.cross(helper).unit();
            math::vec3 v = dir.cross(u);

            return (dir * cos_theta
                + (u * std::cos(phi) + v * std::sin(phi)) * sin_theta)
                .unit();
        }

        // 发射一个粒子（写在第 alive_count 槽位），返回是否成功
        static bool emit_one(
            particle_pool& pool,
            const Emitter& emitter,
            const Translation* translation,
            bool world_space)
        {
            if (pool.alive_count >= pool.capacity)
                return false;

            const size_t i = pool.alive_count++;

            // 发射形状偏移（发射器局部坐标）
            math::vec3 offset = {};
            switch (emitter.m_shape)
            {
            case 1: // 球体（体积内均匀）
            {
                math::vec3 dir = random_direction_around(
                    math::vec3(0.f, 1.f, 0.f), math::PI);
                offset = dir * (emitter.m_shape_radius
                    * std::cbrt(math::random(0.0f, 1.0f)));
                break;
            }
            case 2: // 圆（局部 XY 平面内）
            {
                float angle = math::random(0.0f, 2.f * math::PI);
                float radius = emitter.m_shape_radius
                    * std::sqrt(math::random(0.0f, 1.0f));
                offset = math::vec3(
                    std::cos(angle) * radius,
                    std::sin(angle) * radius,
                    0.f);
                break;
            }
            case 3: // 盒（半边长 m_shape_size）
                offset = math::vec3(
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.x,
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.y,
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.z);
                break;
            case 0: // 点
            default:
                break;
            }

            // 初速方向：散布角围绕主方向
            math::vec3 direction = random_direction_around(
                emitter.m_direction,
                math::clamp(emitter.m_spread_angle, 0.0f, 180.0f) * math::DEG2RAD);
            float speed = math::random(
                std::min(emitter.m_speed_min, emitter.m_speed_max),
                std::max(emitter.m_speed_min, emitter.m_speed_max));

            if (world_space && translation != nullptr)
            {
                // 世界空间模拟：偏移经发射器变换到世界，方向按世界坐标解释
                math::vec3 scaled_offset = math::vec3(
                    offset.x * translation->local_scale.x,
                    offset.y * translation->local_scale.y,
                    offset.z * translation->local_scale.z);
                math::vec3 world_offset = translation->world_rotation * scaled_offset;

                pool.pos_x[i] = translation->world_position.x + world_offset.x;
                pool.pos_y[i] = translation->world_position.y + world_offset.y;
                pool.pos_z[i] = translation->world_position.z + world_offset.z;

                pool.vel_x[i] = direction.x * speed;
                pool.vel_y[i] = direction.y * speed;
                pool.vel_z[i] = direction.z * speed;
            }
            else
            {
                // 局部空间模拟：偏移与方向都在发射器局部坐标系
                pool.pos_x[i] = offset.x;
                pool.pos_y[i] = offset.y;
                pool.pos_z[i] = offset.z;

                pool.vel_x[i] = direction.x * speed;
                pool.vel_y[i] = direction.y * speed;
                pool.vel_z[i] = direction.z * speed;
            }

            pool.age[i] = 0.0f;
            pool.lifetime[i] = std::max(math::EPSILON, math::random(
                std::min(emitter.m_lifetime_min, emitter.m_lifetime_max),
                std::max(emitter.m_lifetime_min, emitter.m_lifetime_max)));

            pool.base_size[i] = math::random(
                std::min(emitter.m_size_min, emitter.m_size_max),
                std::max(emitter.m_size_min, emitter.m_size_max));

            float color_k = math::random(0.0f, 1.0f);
            pool.base_color[i] = math::vec4(
                math::lerp(emitter.m_color_min.x, emitter.m_color_max.x, color_k),
                math::lerp(emitter.m_color_min.y, emitter.m_color_max.y, color_k),
                math::lerp(emitter.m_color_min.z, emitter.m_color_max.z, color_k),
                math::lerp(emitter.m_color_min.w, emitter.m_color_max.w, color_k));

            pool.rot[i] = math::random(
                std::min(emitter.m_rotation_min, emitter.m_rotation_max),
                std::max(emitter.m_rotation_min, emitter.m_rotation_max)) * math::DEG2RAD;
            pool.rot_speed[i] = math::random(
                std::min(emitter.m_angular_min, emitter.m_angular_max),
                std::max(emitter.m_angular_min, emitter.m_angular_max)) * math::DEG2RAD;

            return true;
        }

        static void emit_batch(
            particle_pool& pool,
            const Emitter& emitter,
            const Translation* translation,
            bool world_space,
            size_t count)
        {
            for (size_t c = 0; c < count && pool.alive_count < pool.capacity; ++c)
                if (!emit_one(pool, emitter, translation, world_space))
                    break;
        }

        void Update()
        {
            const float dt = deltatime();
            _m_alive_emitter_ids.clear();

            for (auto&& [
                e, emitter, translation, shape, shaders, textures, rendqueue
            ] : query_entity<
                view typesof(
                    Particle::Emitter&,
                    Translation*,
                    Shape*,
                    Shaders*,
                    Textures*,
                    Rendqueue*
                )
            >())
            {
                _m_alive_emitter_ids.push_back(e._m_raw._m_id);

                // 渲染三件套缺失时补齐（命令缓冲延迟生效，下一帧开始模拟）；
                // Shape 由本系统托管，Shaders 缺省挂内置粒子着色器
                bool renderer_ready = true;
                if (shape == nullptr)
                {
                    e.add_component<Shape>();
                    renderer_ready = false;
                }
                if (shaders == nullptr)
                {
                    if (auto* created = e.add_component<Shaders>())
                        created->shaders.push_back(default_particle_shader());
                    renderer_ready = false;
                }
                if (textures == nullptr)
                {
                    e.add_component<Textures>();
                    renderer_ready = false;
                }
                if (rendqueue == nullptr)
                {
                    // 半透明粒子默认在不透明物体之后绘制
                    if (auto* created = e.add_component<Rendqueue>())
                        created->rend_queue = 1000;
                    renderer_ready = false;
                }
                if (!renderer_ready)
                    continue;

                auto& pool = _m_pools[e._m_raw._m_id];

                const size_t desired_capacity = std::min(
                    MAX_PARTICLE_LIMIT,
                    (size_t)std::max(1, emitter.m_max_particles));
                if (pool.capacity != desired_capacity || !pool.gpu_vertex.has_value())
                {
                    pool.resize(desired_capacity);

                    // 顶点缓冲一次分配到容量上限：索引为静态角点模式，
                    // 之后每帧只重写顶点数据并收缩激活索引数量
                    std::vector<uint32_t> indices;
                    indices.reserve(pool.capacity * 6);
                    for (size_t q = 0; q < pool.capacity; ++q)
                    {
                        const uint32_t base = (uint32_t)(q * 4);
                        for (uint32_t idx : { base + 0, base + 1, base + 2,
                                              base + 2, base + 1, base + 3 })
                            indices.push_back(idx);
                    }

                    auto created = graphic::vertex::create(
                        jegl_vertex::TRIANGLES,
                        pool.staging.data(),
                        pool.staging.size() * sizeof(float),
                        indices,
                        {
                            { jegl_vertex::data_type::FLOAT32, 3 }, // 中心位置（局部）
                            { jegl_vertex::data_type::FLOAT32, 4 }, // 颜色 RGBA
                            { jegl_vertex::data_type::FLOAT32, 2 }, // 角点偏移（已自转缩放）
                            { jegl_vertex::data_type::FLOAT32, 2 }, // UV
                        });

                    if (created.has_value())
                    {
                        pool.gpu_vertex.emplace(std::move(created.value()));
                        shape->vertex = pool.gpu_vertex;
                    }
                    else
                    {
                        pool.gpu_vertex.reset();
                        pool.resize(0);
                        continue;
                    }
                }

                if (emitter.m_clear_particles)
                {
                    pool.reset();
                    emitter.m_clear_particles = false;
                }

                const bool world_space = emitter.m_simulation_space != 0;

                if (emitter.m_playing)
                {
                    // 连续发射
                    pool.spawn_accumulator += (double)emitter.m_spawn_rate * dt;
                    if (pool.spawn_accumulator >= 1.0)
                    {
                        size_t spawn_count = (size_t)pool.spawn_accumulator;
                        pool.spawn_accumulator -= (double)spawn_count;
                        emit_batch(pool, emitter, translation, world_space, spawn_count);
                    }
                    else if (pool.spawn_accumulator < 0.0)
                        pool.spawn_accumulator = 0.0;

                    // 周期爆发
                    if (emitter.m_burst_count > 0 && emitter.m_burst_interval > 0.f)
                    {
                        pool.burst_timer += dt;
                        while (pool.burst_timer >= (double)emitter.m_burst_interval)
                        {
                            pool.burst_timer -= (double)emitter.m_burst_interval;
                            emit_batch(pool, emitter, translation, world_space,
                                (size_t)emitter.m_burst_count);
                        }
                    }

                    // 积分（半隐式欧拉 + 指数阻力），死亡粒子交换删除
                    const float drag_k = emitter.m_drag > 0.f
                        ? std::exp(-emitter.m_drag * dt)
                        : 1.f;
                    // 世界空间下重力按世界坐标解释；局部空间下按发射器
                    // 局部坐标解释（模拟坐标系即存储坐标系，两种情况都直接用）
                    const math::vec3 gravity = emitter.m_gravity;

                    for (size_t i = 0; i < pool.alive_count;)
                    {
                        pool.age[i] += dt;
                        if (pool.age[i] >= pool.lifetime[i])
                        {
                            const size_t last = --pool.alive_count;
                            if (i != last)
                            {
                                pool.pos_x[i] = pool.pos_x[last];
                                pool.pos_y[i] = pool.pos_y[last];
                                pool.pos_z[i] = pool.pos_z[last];
                                pool.vel_x[i] = pool.vel_x[last];
                                pool.vel_y[i] = pool.vel_y[last];
                                pool.vel_z[i] = pool.vel_z[last];
                                pool.age[i] = pool.age[last];
                                pool.lifetime[i] = pool.lifetime[last];
                                pool.base_size[i] = pool.base_size[last];
                                pool.base_color[i] = pool.base_color[last];
                                pool.rot[i] = pool.rot[last];
                                pool.rot_speed[i] = pool.rot_speed[last];
                            }
                            continue; // 重试交换到 i 槽位的粒子
                        }

                        pool.vel_x[i] = pool.vel_x[i] * drag_k + gravity.x * dt;
                        pool.vel_y[i] = pool.vel_y[i] * drag_k + gravity.y * dt;
                        pool.vel_z[i] = pool.vel_z[i] * drag_k + gravity.z * dt;

                        pool.pos_x[i] += pool.vel_x[i] * dt;
                        pool.pos_y[i] += pool.vel_y[i] * dt;
                        pool.pos_z[i] += pool.vel_z[i] * dt;

                        pool.rot[i] += pool.rot_speed[i] * dt;

                        ++i;
                    }

                    pool.dirty = true;
                }

                if (!pool.dirty)
                    continue;

                // ===== 构建公告板网格（局部坐标顶点数据） =====
                jegl_vertex* raw_vertex = pool.gpu_vertex->get()->resource();

                // 世界空间模拟时，经发射器逆变换把世界坐标转回局部
                math::quat inv_rotation;
                math::vec3 inv_scale(1.f, 1.f, 1.f);
                math::vec3 emitter_pos(0.f, 0.f, 0.f);
                if (world_space && translation != nullptr)
                {
                    inv_rotation = translation->world_rotation.inverse();
                    emitter_pos = translation->world_position;
                    inv_scale = math::vec3(
                        1.f / std::max(translation->local_scale.x, math::EPSILON),
                        1.f / std::max(translation->local_scale.y, math::EPSILON),
                        1.f / std::max(translation->local_scale.z, math::EPSILON));
                }

                float min_x = 0.f, max_x = 0.f,
                    min_y = 0.f, max_y = 0.f,
                    min_z = 0.f, max_z = 0.f;
                float max_size = 0.f;
                bool has_bounds = false;

                const math::vec4 white(1.f, 1.f, 1.f, 1.f);
                const float unit_scale = 1.f;

                for (size_t i = 0; i < pool.alive_count; ++i)
                {
                    math::vec3 local(
                        pool.pos_x[i], pool.pos_y[i], pool.pos_z[i]);
                    if (world_space && translation != nullptr)
                    {
                        math::vec3 delta = local - emitter_pos;
                        math::vec3 rotated = inv_rotation * delta;
                        local = math::vec3(
                            rotated.x * inv_scale.x,
                            rotated.y * inv_scale.y,
                            rotated.z * inv_scale.z);
                    }

                    const float t = math::clamp(
                        pool.age[i] / pool.lifetime[i], 0.0f, 1.0f);

                    const math::vec4 grad
                        = emitter.m_color_over_lifetime.sample(t, white);
                    const math::vec4& base = pool.base_color[i];
                    const math::vec4 color(
                        base.x * grad.x, base.y * grad.y,
                        base.z * grad.z, base.w * grad.w);

                    const float size = pool.base_size[i]
                        * emitter.m_size_over_lifetime.sample(t, unit_scale);

                    // 角点按自转旋转并乘以尺寸：着色器只沿摄像机平面展开
                    const float cs = std::cos(pool.rot[i]) * size;
                    const float sn = std::sin(pool.rot[i]) * size;

                    float* quad = pool.staging.data()
                        + i * 4 * FLOATS_PER_VERTEX;
                    for (size_t c = 0; c < 4; ++c)
                    {
                        const float cx = CORNER_UV[c][0];
                        const float cy = CORNER_UV[c][1];

                        float* v = quad + c * FLOATS_PER_VERTEX;
                        v[0] = local.x;
                        v[1] = local.y;
                        v[2] = local.z;
                        v[3] = color.x;
                        v[4] = color.y;
                        v[5] = color.z;
                        v[6] = color.w;
                        v[7] = cs * cx - sn * cy;
                        v[8] = sn * cx + cs * cy;
                        v[9] = CORNER_UV[c][2];
                        v[10] = CORNER_UV[c][3];
                    }

                    if (!has_bounds)
                    {
                        min_x = max_x = local.x;
                        min_y = max_y = local.y;
                        min_z = max_z = local.z;
                        has_bounds = true;
                    }
                    else
                    {
                        min_x = std::min(min_x, local.x);
                        max_x = std::max(max_x, local.x);
                        min_y = std::min(min_y, local.y);
                        max_y = std::max(max_y, local.y);
                        min_z = std::min(min_z, local.z);
                        max_z = std::max(max_z, local.z);
                    }
                    max_size = std::max(max_size, std::abs(size));
                }

                if (has_bounds)
                {
                    // 包围盒外扩粒子尺寸，供视锥剔除使用
                    raw_vertex->m_x_min = min_x - max_size;
                    raw_vertex->m_x_max = max_x + max_size;
                    raw_vertex->m_y_min = min_y - max_size;
                    raw_vertex->m_y_max = max_y + max_size;
                    raw_vertex->m_z_min = min_z - max_size;
                    raw_vertex->m_z_max = max_z + max_size;
                }
                else
                {
                    raw_vertex->m_x_min = raw_vertex->m_y_min = raw_vertex->m_z_min = -0.01f;
                    raw_vertex->m_x_max = raw_vertex->m_y_max = raw_vertex->m_z_max = 0.01f;
                }

                pool.gpu_vertex->get()->update_buffer(
                    pool.staging.data(),
                    pool.staging.size() * sizeof(float),
                    pool.alive_count * 6);

                pool.dirty = false;
            }

            // 回收不再发射的实体对应的粒子池
            if (_m_pools.size() != _m_alive_emitter_ids.size())
            {
                for (auto iter = _m_pools.begin(); iter != _m_pools.end();)
                {
                    bool still_alive = false;
                    for (uint32_t id : _m_alive_emitter_ids)
                        if (id == iter->first)
                        {
                            still_alive = true;
                            break;
                        }

                    if (still_alive)
                        ++iter;
                    else
                        iter = _m_pools.erase(iter);
                }
            }
        }
    };
}
