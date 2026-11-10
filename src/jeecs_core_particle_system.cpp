#define JE_IMPL
#include "jeecs/components.hpp"
#include "jeecs/game.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/math.hpp"
#include "jeecs/prefix.hpp"
#include "jeecs_core_particle_system.hpp"

#include <cmath>
#include <algorithm>
#include <bit>

namespace jeecs::particle
{
    using namespace slice_requirement;
    using namespace Particle;
    using namespace Renderer;
    using namespace Transform;

    namespace
    {
        // 公告板四边形的角点与 UV（TRIANGLES，索引模式 0,1,2, 2,1,3）；
        // 角点在构建时按粒子自转旋转并乘以尺寸，着色器只需沿摄像机
        // 平面展开（见 builtin/shader/ParticleBlend.shader）
        inline const float CORNER_UV[4][4] = {
            /* corner_x, corner_y, uv_x, uv_y */
            { -1.f, -1.f, 0.f, 0.f },
            { 1.f, -1.f, 1.f, 0.f },
            { -1.f, 1.f, 0.f, 1.f },
            { 1.f, 1.f, 1.f, 1.f },
        };
        // 顶点布局：中心位置(3) + 颜色(4) + 角点偏移(2) + UV(2) + 粒子ID(1, INT32)
        constexpr size_t FLOATS_PER_VERTEX = 12;
        constexpr size_t MAX_PARTICLE_LIMIT = 65536;

        // 在 axis 周围半角 spread_rad 的圆锥内均匀取方向；spread 覆盖全球面时
        // 退化为均匀球面采样
        math::vec3 random_direction_around(const math::vec3& axis, float spread_rad)
        {
            const math::vec3 dir = axis.length() > math::EPSILON ? axis.unit()
                : math::vec3(0.f, 1.f, 0.f);

            if (spread_rad >= math::PI)
            {
                // 均匀球面：z ∈ [-1,1]，φ ∈ [0,2π)
                const float z = math::random(-1.0f, 1.0f);
                const float phi = math::random(0.0f, 2.f * math::PI);
                const float r = std::sqrt(std::max(0.f, 1.f - z * z));
                return math::vec3(r * std::cos(phi), r * std::sin(phi), z);
            }

            // 圆锥内余弦均匀：cosθ ∈ [cos(spread), 1]
            const float cos_theta = math::lerp(
                std::cos(spread_rad), 1.0f, math::random(0.0f, 1.0f));
            const float sin_theta = std::sqrt(std::max(0.f, 1.f - cos_theta * cos_theta));
            const float phi = math::random(0.0f, 2.f * math::PI);

            const math::vec3 helper = std::abs(dir.y) < 0.9f
                ? math::vec3(0.f, 1.f, 0.f)
                : math::vec3(1.f, 0.f, 0.f);
            const math::vec3 u = dir.cross(helper).unit();
            const math::vec3 v = dir.cross(u);

            return (dir * cos_theta
                + (u * std::cos(phi) + v * std::sin(phi)) * sin_theta)
                .unit();
        }

        // 发射一个粒子（写在第 alive_count 槽位），返回是否成功
        bool emit_one(
            ParticleSystem::particle_pool& pool,
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
            case Emitter::shape_type::sphere: // 球体（体积内均匀）
            {
                const math::vec3 dir = random_direction_around(
                    math::vec3(0.f, 1.f, 0.f), math::PI);
                offset = dir * (emitter.m_shape_radius
                    * std::cbrt(math::random(0.0f, 1.0f)));
                break;
            }
            case Emitter::shape_type::circle: // 圆（局部 XY 平面内）
            {
                const float angle = math::random(0.0f, 2.f * math::PI);
                const float radius = emitter.m_shape_radius
                    * std::sqrt(math::random(0.0f, 1.0f));
                offset = math::vec3(
                    std::cos(angle) * radius,
                    std::sin(angle) * radius,
                    0.f);
                break;
            }
            case Emitter::shape_type::box: // 盒（半边长 m_shape_size）
                offset = math::vec3(
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.x,
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.y,
                    math::random(-1.0f, 1.0f) * emitter.m_shape_size.z);
                break;
            case Emitter::shape_type::point: // 点
            default:
                break;
            }

            // 初速方向：散布角围绕主方向
            const math::vec3 direction = random_direction_around(
                translation->world_rotation * emitter.m_direction,
                math::clamp(emitter.m_spread_angle, 0.0f, 180.0f) * math::DEG2RAD);
            const float speed = math::random(
                std::min(emitter.m_speed_min, emitter.m_speed_max),
                std::max(emitter.m_speed_min, emitter.m_speed_max));

            if (world_space && translation != nullptr)
            {
                // 世界空间模拟：偏移经发射器变换到世界，方向按世界坐标解释
                const math::vec3 scaled_offset = math::vec3(
                    offset.x * translation->local_scale.x,
                    offset.y * translation->local_scale.y,
                    offset.z * translation->local_scale.z);
                const math::vec3 world_offset = translation->world_rotation * scaled_offset;

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

            const float color_k = math::random(0.0f, 1.0f);
            const math::vec4 color_min = emitter.m_color_min.to_linear();
            const math::vec4 color_max = emitter.m_color_max.to_linear();
            pool.base_color[i] = math::vec4(
                math::lerp(color_min.x, color_max.x, color_k),
                math::lerp(color_min.y, color_max.y, color_k),
                math::lerp(color_min.z, color_max.z, color_k),
                math::lerp(color_min.w, color_max.w, color_k));

            pool.rot[i] = math::random(
                std::min(emitter.m_rotation_min, emitter.m_rotation_max),
                std::max(emitter.m_rotation_min, emitter.m_rotation_max)) * math::DEG2RAD;
            pool.rot_speed[i] = math::random(
                std::min(emitter.m_angular_min, emitter.m_angular_max),
                std::max(emitter.m_angular_min, emitter.m_angular_max)) * math::DEG2RAD;

            // ID 通道为 INT32，在 INT32_MAX 处回绕，
            // 保证着色器读到的粒子 ID 始终为非负整数
            pool.particle_ids[i] = pool.next_particle_id;
            pool.next_particle_id = pool.next_particle_id >= 0x7FFFFFFFu
                ? 0u
                : pool.next_particle_id + 1;

            return true;
        }

        void emit_batch(
            ParticleSystem::particle_pool& pool,
            const Emitter& emitter,
            const Translation* translation,
            bool world_space,
            size_t count)
        {
            for (size_t c = 0; c < count && pool.alive_count < pool.capacity; ++c)
                if (!emit_one(pool, emitter, translation, world_space))
                    break;
        }
    }

    ParticleSystem::ParticleSystem(game_world w)
        : game_system(w)
    {
    }

    void ParticleSystem::particle_pool::reset()
    {
        alive_count = 0;
        spawn_accumulator = 0.0;
        burst_timer = 0.0;
        dirty = true;
    }

    void ParticleSystem::particle_pool::resize(size_t new_capacity)
    {
        capacity = new_capacity;
        pos_x.resize(capacity); pos_y.resize(capacity); pos_z.resize(capacity);
        vel_x.resize(capacity); vel_y.resize(capacity); vel_z.resize(capacity);
        age.resize(capacity); lifetime.resize(capacity);
        base_size.resize(capacity); rot.resize(capacity); rot_speed.resize(capacity);
        base_color.resize(capacity);
        particle_ids.resize(capacity);
        staging.assign(capacity * 4 * FLOATS_PER_VERTEX, 0.0f);
        reset();
    }

    void ParticleSystem::GraphicUpdate()
    {
        const float dt = deltatime();
        _m_alive_emitter_ids.clear();

        for (auto&& [
            e, emitter, translation, shape
        ] : query_entity<
            view typesof(
                Particle::Emitter&,
                Translation*,
                Shape&
            )
        >())
        {
            _m_alive_emitter_ids.push_back(e._m_raw._m_id);

            bool renderer_ready = true;

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
                    JE_GL_VERTEX_TYPE_TRIANGLES,
                    pool.staging.data(),
                    pool.staging.size() * sizeof(float),
                    indices,
                    {
                        { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 3 }, // 中心位置（局部）
                        { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 4 }, // 颜色 RGBA
                        { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2 }, // 角点偏移（已自转缩放）
                        { JE_GL_VERTEX_DATA_TYPE_FLOAT32, 2 }, // UV
                        { JE_GL_VERTEX_DATA_TYPE_INT32, 1 },   // 粒子 ID
                    });

                if (created.has_value())
                {
                    pool.gpu_vertex.emplace(std::move(created.value()));
                    shape.vertex = pool.gpu_vertex;
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

            const bool world_space =
                emitter.m_simulation_space != Emitter::simulation_space_type::local;

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
                            pool.particle_ids[i] = pool.particle_ids[last];
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
            je_gl_vertex_t* raw_vertex = pool.gpu_vertex->get()->resource();

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

            const math::color white(1.f, 1.f, 1.f, 1.f);
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
                    = emitter.m_color_over_lifetime.sample(t, white).to_linear();
                const math::vec4& base = pool.base_color[i];
                const math::vec4 color(
                    base.x * grad.x, base.y * grad.y,
                    base.z * grad.z, base.w * grad.w);

                const float size = pool.base_size[i]
                    * emitter.m_size_over_lifetime.sample(t, unit_scale);

                // 角点按自转旋转并乘以尺寸：着色器只沿摄像机平面展开
                const float cs = std::cos(pool.rot[i]) * size;
                const float sn = std::sin(pool.rot[i]) * size;

                // 粒子 ID 通道声明为 INT32，GPU 按整型读取顶点字节，
                // 故此处以位模式暂存于 float 暂存缓冲
                const float pid_bits = std::bit_cast<float>(
                    pool.particle_ids[i]);

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
                    v[11] = pid_bits;
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
}
