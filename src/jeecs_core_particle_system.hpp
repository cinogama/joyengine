#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

#include <vector>
#include <unordered_map>

namespace jeecs::particle
{
    /*
    ParticleSystem [系统]
    粒子模拟系统：驱动 Particle::Emitter 组件的发射、积分与网格构建。

    工作方式：
      * 每个发射器实体在本系统内持有一个 CPU 粒子池（发射器粒子上限容量的
        SoA 数组），死亡粒子以交换删除方式紧凑排布；
      * 发射器实体复用常规渲染组件：系统把存活粒子构建为面向摄像机的公告板
        四边形网格（每粒子 4 顶点 6 索引），经 je_graphic_vertex_buffer_update 逐帧
        写入 Renderer::Shape，由常规图形管线（如 UnlitGraphicPipelineSystem）
        随实体一起绘制；粒子纹理绑定在 Renderer::Textures2D 的 pass 0；
      * 公告板朝向在粒子着色器中用视图矩阵展开（顶点属性携带中心
        位置/颜色/已自转缩放的角点/粒子 ID），CPU 只做模拟，因此多
        摄像机自动正确；
      * 模拟空间：世界空间下粒子位置存于世界坐标，构建网格时经发射器
        逆变换转回局部（发射器移动不携带已发射粒子）；局部空间下粒子
        直接存于发射器局部坐标（整体随发射器刚体运动），此时 direction
        与 gravity 按发射器局部坐标系解释。
    运行相位：Update（在物理与渲染之前完成数据构建）。
    */
    struct ParticleSystem : public game_system
    {
        // ===== 每发射器粒子池（SoA，存于模拟坐标系） =====
        struct particle_pool
        {
            std::vector<float> pos_x, pos_y, pos_z;
            std::vector<float> vel_x, vel_y, vel_z;
            std::vector<float> age, lifetime;
            std::vector<float> base_size, rot, rot_speed;
            std::vector<math::vec4> base_color;
            // 粒子唯一 ID：发射时按 next_particle_id 单调递增分配，
            // 交换删除时随粒子一起移动，供着色器区分不同粒子
            std::vector<uint32_t> particle_ids;
            uint32_t next_particle_id = 0;

            size_t alive_count = 0;
            size_t capacity = 0;

            double spawn_accumulator = 0.0;
            double burst_timer = 0.0;
            bool dirty = true; // 池新建/重置后至少完整上传一次

            basic::optional<basic::resource<graphic::vertex>> gpu_vertex;
            std::vector<float> staging; // capacity * 4 顶点 * FLOATS_PER_VERTEX

            void reset();
            void resize(size_t new_capacity);
        };

        std::unordered_map<uint32_t, particle_pool> _m_pools;
        std::vector<uint32_t> _m_alive_emitter_ids;

        ParticleSystem(game_world w);

        void GraphicUpdate();
    };
}
