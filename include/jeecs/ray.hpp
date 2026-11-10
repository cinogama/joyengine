#pragma once

// ============================================================================
// jeecs/ray.hpp —— 射线（C++）：jeecs::math::ray，依赖内置组件与图形封装
// =============================================================================

#include "jeecs/components.hpp"
#include "jeecs/graphic.hpp"

#ifdef __cplusplus
namespace jeecs
{
    namespace math
    {
        struct ray
        {
        public:
            vec3 orgin;
            vec3 direction;

            ray() noexcept
                : orgin(0, 0, 0)
                , direction(0, 0, 1)
            {
            }

            ray(ray&&) = default;
            ray(const ray&) = default;

            ray& operator=(ray&&) = default;
            ray& operator=(const ray&) = default;

            ray(const vec3& _orgin, const vec3& _direction) : orgin(_orgin),
                direction(_direction)
            {
            }
            ray(const Transform::Translation& camera_trans, const Camera::Projection& camera_proj, const vec2& screen_pos, bool ortho)
            {
                // 根据摄像机和屏幕坐标创建射线
                float ray_eye[4] = { screen_pos.x, screen_pos.y, 1.0f, 1.0f };

                float ray_world[4];
                mat4xvec4(ray_world, camera_proj.inv_projection, ray_eye);

                if (ray_world[3] != 0.0f)
                {
                    // is perspective
                    ray_world[0] /= ray_world[3];
                    ray_world[1] /= ray_world[3];
                    ray_world[2] /= ray_world[3];
                }

                if (ortho)
                {
                    // not perspective
                    orgin = camera_trans.world_position + camera_trans.world_rotation * vec3{ ray_world[0], ray_world[1], 0 };
                    direction = vec3(0, 0, 1);
                }
                else
                {
                    vec3 ray_dir(ray_world[0], ray_world[1], ray_world[2]);
                    orgin = camera_trans.world_position;
                    direction = (camera_trans.world_rotation * ray_dir).unit();
                }
            }

            struct intersect_result
            {
                bool intersected;
                vec3 place;
                float distance;

                intersect_result() noexcept
                    : intersected(false)
                    , place(0, 0, 0)
                    , distance(INFINITY)
                {
                }
                intersect_result(bool rslt, float dist = INFINITY, const vec3& plce = vec3(0, 0, 0)) : intersected(rslt),
                    place(plce),
                    distance(dist)
                {
                }
            };

            intersect_result intersect_triangle(const vec3& v0, const vec3& v1, const vec3& v2) const
            {
                float _t, _u, _v;
                float* t = &_t, * u = &_u, * v = &_v;

                // E1
                vec3 E1 = v1 - v0;
                // E2
                vec3 E2 = v2 - v0;
                // P
                vec3 P = direction.cross(E2);
                // determinant
                float det = E1.dot(P);
                // keep det > 0, modify T accordingly
                vec3 T;
                if (det > 0)
                {
                    T = orgin - v0;
                }
                else
                {
                    T = v0 - orgin;
                    det = -det;
                }
                // If determinant is near zero, ray lies in plane of triangle
                if (det < 0.0001f)
                    return false;
                // Calculate u and make sure u <= 1

                *u = T.dot(P); // T.dot(P);
                if (*u < 0.0f || *u > det)
                    return false;

                // Q

                vec3 Q = T.cross(E1); // T.Cross(E1);

                // Calculate v and make sure u + v <= 1
                *v = direction.dot(Q); // direction.dot(Q);

                if (*v < 0.0f || *u + *v > det)
                    return false;

                // Calculate t, scale parameters, ray intersects triangle

                *t = E2.dot(Q); // .dot(Q);

                float fInvDet = 1.0f / det;
                *t *= fInvDet;
                *u *= fInvDet;
                *v *= fInvDet;

                auto&& clid = ((1.0f - *u - *v) * v0 + *u * v1 + *v * v2);
                auto&& delta = clid - orgin;

                if (delta.dot(direction) < 0)
                {
                    return false;
                }
                // if (vec3::dot(delta, direction) < 0.0f)
                //	return false;
                return intersect_result(true, delta.length(), clid);
            }
            intersect_result intersect_rectangle(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& v3) const
            {
                /*

                v0-----------v3
                |             |
                |             |
                |             |
                v1-----------v2

                */

                auto&& inresult = intersect_triangle(v0, v1, v2);
                if (inresult.intersected)
                    return inresult;
                return intersect_triangle(v0, v3, v2);
            }
            intersect_result intersect_box(const vec3& offset, const vec3& size, const quat& rotation) const
            {
                /*
                        4----------5
                    /   |      /   |
                0----------1       |
                |       |  |       |
                |       |  |       |
                |       |  |       |
                |       6--|-------7
                |   /     |	   /
                2----------3

                */
                vec3 finalBoxPos[8];
                intersect_result minResult = false;
                minResult.distance = INFINITY;

                // pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = size.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = size.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = size.z / 2.0f);

                // rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = (rotation * finalBoxPos[i]) + offset;

                // front
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // back
                {
                    auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // left
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // right
                {
                    auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // top
                {
                    auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }
                // bottom
                {
                    auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                    if (f.intersected && f.distance < minResult.distance)
                        minResult = f;
                }

                return minResult;
            }
            intersect_result intersect_mesh(
                const basic::resource<graphic::vertex>& mesh,
                const vec3& offset,
                const quat& rotation,
                const vec3& scale) const
            {
                je_gl_vertex_t* raw_vertex_data = mesh->resource();

                intersect_result minResult = false;
                minResult.distance = INFINITY;

                switch (raw_vertex_data->m_type)
                {
                case JE_GL_VERTEX_TYPE_TRIANGLES:
                {
                    // 带索引的网格（模型文件加载产物）必须按索引缓冲取三角形，
                    // 否则顶点顺序与索引数量不匹配，结果不可靠且可能越界读取。
                    // 无索引时按顶点数组顺序遍历，上界为顶点数（字节数/每顶点大小）。
                    const bool use_indices = raw_vertex_data->m_indices != nullptr;
                    const size_t ref_count = use_indices
                        ? raw_vertex_data->m_index_count
                        : raw_vertex_data->m_vertex_length / raw_vertex_data->m_data_size_per_point;

                    auto vertex_ref = [raw_vertex_data, use_indices](size_t i) -> size_t
                    {
                        return use_indices ? raw_vertex_data->m_indices[i] : i;
                    };

                    for (size_t i = 0; i + 2 < ref_count; i += 3)
                    {
                        const float* point_0 =
                            reinterpret_cast<const float*>(
                                static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(i));
                        const float* point_1 =
                            reinterpret_cast<const float*>(
                                static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(i + 1));
                        const float* point_2 =
                            reinterpret_cast<const float*>(
                                static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(i + 2));

                        vec3 triangle_point[3] = {
                            {point_0[0], point_0[1], point_0[2]},
                            {point_1[0], point_1[1], point_1[2]},
                            {point_2[0], point_2[1], point_2[2]} };

                        auto&& f = intersect_triangle(
                            offset + rotation * triangle_point[0] * scale,
                            offset + rotation * triangle_point[1] * scale,
                            offset + rotation * triangle_point[2] * scale);

                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    break;
                }
                case JE_GL_VERTEX_TYPE_TRIANGLESTRIP:
                {
                    const bool use_indices = raw_vertex_data->m_indices != nullptr;
                    const size_t ref_count = use_indices
                        ? raw_vertex_data->m_index_count
                        : raw_vertex_data->m_vertex_length / raw_vertex_data->m_data_size_per_point;

                    auto vertex_ref = [raw_vertex_data, use_indices](size_t i) -> size_t
                    {
                        return use_indices ? raw_vertex_data->m_indices[i] : i;
                    };

                    const float* point_0 =
                        reinterpret_cast<const float*>(
                            static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(0));
                    const float* point_1 =
                        reinterpret_cast<const float*>(
                            static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(1));

                    vec3 triangle_point[3] = {
                        {point_0[0], point_0[1], point_0[2]},
                        {point_1[0], point_1[1], point_1[2]},
                        {},
                    };

                    for (size_t i = 2; i < ref_count; ++i)
                    {
                        const float* point = reinterpret_cast<const float*>(
                            static_cast<const char*>(raw_vertex_data->m_vertexs) + raw_vertex_data->m_data_size_per_point * vertex_ref(i));

                        triangle_point[i % 3] = { point[0], point[1], point[2] };

                        auto&& f = intersect_triangle(
                            offset + rotation * triangle_point[0] * scale,
                            offset + rotation * triangle_point[1] * scale,
                            offset + rotation * triangle_point[2] * scale);

                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    break;
                }
                default:
                    // Support triangles only
                    break;
                }

                return minResult;
            }

            intersect_result intersect_entity(
                const Transform::Translation& translation,
                const Renderer::Shape* entity_shape_may_null,
                bool consider_mesh) const
            {
                vec3 entity_box_center, entity_box_size;
                if (entity_shape_may_null)
                {
                    if (entity_shape_may_null->vertex.has_value())
                    {
                        auto* vertex_dat = entity_shape_may_null->vertex.value()->resource();
                        entity_box_center = vec3(
                            (vertex_dat->m_x_max + vertex_dat->m_x_min) / 2.0f,
                            (vertex_dat->m_y_max + vertex_dat->m_y_min) / 2.0f,
                            (vertex_dat->m_z_max + vertex_dat->m_z_min) / 2.0f);
                        entity_box_size = vec3(
                            vertex_dat->m_x_max - vertex_dat->m_x_min,
                            vertex_dat->m_y_max - vertex_dat->m_y_min,
                            vertex_dat->m_z_max - vertex_dat->m_z_min);
                    }
                    else
                    {
                        // Default shape size
                        entity_box_center = vec3(0.f, 0.f, 0.f);
                        entity_box_size = vec3(1.f, 1.f, 0.f);
                    }
                }
                else
                {
                    // Treat as a box
                    entity_box_center = vec3(0.f, 0.f, 0.f);
                    entity_box_size = vec3(1.f, 1.f, 1.f);
                }

                intersect_result minResult =
                    intersect_box(
                        entity_box_center + translation.world_position,
                        translation.local_scale * entity_box_size,
                        translation.world_rotation);

                if (minResult.intersected
                    && consider_mesh
                    && entity_shape_may_null != nullptr
                    && entity_shape_may_null->vertex.has_value())
                {
                    return intersect_mesh(
                        entity_shape_may_null->vertex.value(),
                        translation.world_position,
                        translation.world_rotation,
                        translation.local_scale);
                }

                return minResult;
            }
        };
    }
}
#endif // __cplusplus
