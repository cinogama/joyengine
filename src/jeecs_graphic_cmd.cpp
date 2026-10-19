#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_graphic_render_internal.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

// Render API (v2): backend-agnostic command recording.
//
// Recording happens on any thread and only touches CPU memory. Commit runs on
// the owning graphic thread, prepares resources lazily and dispatches a compact
// command stream to the backend render vtable. All state validation is done
// with asserts; invalid usage is a programming error, not a recoverable case.

namespace
{
    enum class jegl_cmd_op : uint32_t
    {
        BEGIN_PASS,
        CLEAR_COLOR,
        CLEAR_DEPTH,
        END_PASS,

        BIND_PIPELINE,
        BIND_VERTEX,
        BIND_UNIFORM_BUFFER,
        BIND_TEXTURE_2D,
        BIND_TEXTURE_3D,
        BIND_TEXTURE_ARRAY,
        BIND_TEXTURE_CUBE,
        BIND_SAMPLER,

        DRAW_INDEXED,
        DRAW,
    };

    struct jegl_cmd
    {
        jegl_cmd_op m_op;
        uint32_t m_a, m_b, m_c, m_d;
        uint32_t m_uniform_offset;
        uint32_t m_uniform_size;
        uintptr_t m_ptr0;
        float m_f[4];
    };

    struct jegl_resource_entry
    {
        void* m_ptr;
        void (*m_release)(void*);
    };

    template<typename T>
    void _jegl_release_shared_resource(void* ptr)
    {
        T* resource = static_cast<T*>(ptr);

        if constexpr (std::is_same_v<T, jegl_shader>)
            jegl_close_shader(resource);
        else if constexpr (std::is_same_v<T, jegl_texture_2d>)
            jegl_close_texture_2d(resource);
        else if constexpr (std::is_same_v<T, jegl_texture_3d>)
            jegl_close_texture_3d(resource);
        else if constexpr (std::is_same_v<T, jegl_texture_array>)
            jegl_close_texture_array(resource);
        else if constexpr (std::is_same_v<T, jegl_texture_cube>)
            jegl_close_texture_cube(resource);
        else if constexpr (std::is_same_v<T, jegl_texture_msaa>)
            jegl_close_texture_msaa(resource);
        else if constexpr (std::is_same_v<T, jegl_vertex>)
            jegl_close_vertex(resource);
        else if constexpr (std::is_same_v<T, jegl_frame_buffer>)
            jegl_close_framebuf(resource);
        else if constexpr (std::is_same_v<T, jegl_uniform_buffer>)
            jegl_close_uniformbuf(resource);
        else if constexpr (std::is_same_v<T, jegl_pipeline>)
            jegl_close_pipeline(resource);
        else if constexpr (std::is_same_v<T, jegl_sampler>)
            jegl_close_sampler(resource);
        else
            static_assert(!sizeof(T*), "Unsupported graphic resource type.");
    }

    inline size_t _jegl_resource_hash(void* ptr)
    {
        return (reinterpret_cast<uintptr_t>(ptr) >> 4);
    }

    inline uint32_t _jegl_align_up(uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }

    bool _jegl_topology_of_vertex(
        const jegl_vertex* vertex, jegl_topology* out_topology)
    {
        switch (vertex->m_type)
        {
        case jegl_vertex::type::LINESTRIP:
            *out_topology = jegl_topology::LINE_STRIP;
            return true;
        case jegl_vertex::type::TRIANGLES:
            *out_topology = jegl_topology::TRIANGLES;
            return true;
        case jegl_vertex::type::TRIANGLESTRIP:
            *out_topology = jegl_topology::TRIANGLE_STRIP;
            return true;
        default:
            return false;
        }
    }

    bool _jegl_vertex_layout_matches(
        const jegl_vertex* vertex, const jegl_shader* shader)
    {
        // A shader consumes a prefix of the vertex attributes. Models may
        // carry extra attributes (normal/tangent/...) that the shader ignores;
        // the pipeline input layout is built from the shader's vin, so only
        // that prefix has to match.
        if (shader->m_vertex_in_count > vertex->m_format_count)
            return false;

        for (size_t i = 0; i < shader->m_vertex_in_count; ++i)
        {
            const auto type = shader->m_vertex_in[i];
            size_t count = 0;
            jegl_vertex::data_type base_type;

            switch (type)
            {
            case jegl_shader::uniform_type::FLOAT:
                count = 1;
                base_type = jegl_vertex::data_type::FLOAT32;
                break;
            case jegl_shader::uniform_type::FLOAT2:
                count = 2;
                base_type = jegl_vertex::data_type::FLOAT32;
                break;
            case jegl_shader::uniform_type::FLOAT3:
                count = 3;
                base_type = jegl_vertex::data_type::FLOAT32;
                break;
            case jegl_shader::uniform_type::FLOAT4:
                count = 4;
                base_type = jegl_vertex::data_type::FLOAT32;
                break;
            case jegl_shader::uniform_type::INT:
                count = 1;
                base_type = jegl_vertex::data_type::INT32;
                break;
            case jegl_shader::uniform_type::INT2:
                count = 2;
                base_type = jegl_vertex::data_type::INT32;
                break;
            case jegl_shader::uniform_type::INT3:
                count = 3;
                base_type = jegl_vertex::data_type::INT32;
                break;
            case jegl_shader::uniform_type::INT4:
                count = 4;
                base_type = jegl_vertex::data_type::INT32;
                break;
            default:
                return false;
            }

            if (vertex->m_formats[i].m_count != count
                || vertex->m_formats[i].m_type != base_type)
                return false;
        }
        return true;
    }
}

struct jegl_command_buffer
{
    JECS_DISABLE_MOVE_AND_COPY(jegl_command_buffer);

    struct texture_binding
    {
        uint32_t m_pass;
        jegl_texture_kind m_kind;
        void* m_texture;
    };

    jegl_context* m_context;

    std::vector<jegl_cmd> m_commands;
    std::vector<uint8_t> m_uniform_arena;

    // Open-addressing set of resources that must stay alive until reset/close.
    std::vector<jegl_resource_entry> m_resources;
    size_t m_resource_count;

    bool m_in_pass;
    bool m_pass_has_draw;

    bool m_pending_draw;
    jegl_cmd_op m_pending_draw_op;
    uint32_t m_pending_a, m_pending_b, m_pending_c;

    bool m_has_pipeline;
    bool m_has_vertex;
    jegl_pipeline* m_pipeline;
    jegl_vertex* m_vertex;

    jegl_frame_buffer* m_target;
    uint32_t m_target_attachment_count;

    std::vector<uint8_t> m_uniform_staging;
    uint32_t m_uniform_staging_size;
    bool m_uniform_staging_dirty;
    bool m_has_last_uniform_block;
    uint32_t m_last_uniform_offset;
    uint32_t m_last_uniform_size;

    texture_binding m_texture_bindings[32];
    size_t m_texture_binding_count;

    explicit jegl_command_buffer(jegl_context* context)
        : m_context(context)
        , m_resource_count(0)
        , m_in_pass(false)
        , m_pass_has_draw(false)
        , m_pending_draw(false)
        , m_pending_draw_op(jegl_cmd_op::DRAW_INDEXED)
        , m_pending_a(0)
        , m_pending_b(0)
        , m_pending_c(0)
        , m_has_pipeline(false)
        , m_has_vertex(false)
        , m_pipeline(nullptr)
        , m_vertex(nullptr)
        , m_target(nullptr)
        , m_target_attachment_count(1)
        , m_uniform_staging_size(0)
        , m_uniform_staging_dirty(false)
        , m_has_last_uniform_block(false)
        , m_last_uniform_offset(0)
        , m_last_uniform_size(0)
        , m_texture_binding_count(0)
    {
        assert(context != nullptr);
    }

    ~jegl_command_buffer()
    {
        release_all_resources();
    }

    void ensure_resource_capacity(size_t needed)
    {
        size_t capacity = m_resources.size();
        if (capacity >= needed)
            return;

        if (capacity == 0)
            capacity = 64;
        while (capacity < needed)
            capacity *= 2;

        std::vector<jegl_resource_entry> old =
            std::move(m_resources);
        m_resources.assign(capacity, jegl_resource_entry{ nullptr, nullptr });
        m_resource_count = 0;

        for (auto& entry : old)
        {
            if (entry.m_ptr != nullptr)
                insert_raw(entry);
        }
    }

    size_t find_slot(void* ptr) const
    {
        assert(!m_resources.empty());
        const size_t mask = m_resources.size() - 1;
        size_t index = _jegl_resource_hash(ptr) & mask;

        while (m_resources[index].m_ptr != nullptr)
        {
            if (m_resources[index].m_ptr == ptr)
                return index;
            index = (index + 1) & mask;
        }
        return index;
    }

    bool contains(void* ptr) const
    {
        if (m_resources.empty())
            return false;
        return m_resources[find_slot(ptr)].m_ptr == ptr;
    }

    void insert_raw(const jegl_resource_entry& entry)
    {
        const size_t mask = m_resources.size() - 1;
        size_t index = _jegl_resource_hash(entry.m_ptr) & mask;

        while (m_resources[index].m_ptr != nullptr)
            index = (index + 1) & mask;

        m_resources[index] = entry;
        ++m_resource_count;
    }

    void retain_raw(void* ptr, void (*release)(void*))
    {
        if (m_resources.empty() || (m_resource_count + 1) * 10 >= m_resources.size() * 7)
            ensure_resource_capacity(m_resource_count + 1);

        if (contains(ptr))
            return;

        insert_raw(jegl_resource_entry{ ptr, release });
    }

    template<typename T>
    void retain(T* resource)
    {
        if (resource == nullptr)
            return;
        if (contains(resource))
            return;

        jegl_share_resource(resource);
        retain_raw(resource, &_jegl_release_shared_resource<T>);
    }

    void release_all_resources()
    {
        for (auto& entry : m_resources)
        {
            if (entry.m_ptr != nullptr)
            {
                entry.m_release(entry.m_ptr);
                entry.m_ptr = nullptr;
                entry.m_release = nullptr;
            }
        }
        m_resource_count = 0;
    }

    void reset()
    {
        assert(!m_in_pass);
        assert(!m_pending_draw);

        release_all_resources();
        m_commands.clear();
        m_uniform_arena.clear();

        m_pass_has_draw = false;
        m_pending_draw = false;
        m_pending_draw_op = jegl_cmd_op::DRAW_INDEXED;
        m_pending_a = m_pending_b = m_pending_c = 0;
        m_has_pipeline = false;
        m_has_vertex = false;
        m_pipeline = nullptr;
        m_vertex = nullptr;
        m_target = nullptr;
        m_target_attachment_count = 1;

        m_uniform_staging.clear();
        m_uniform_staging_size = 0;
        m_uniform_staging_dirty = false;
        m_has_last_uniform_block = false;
        m_last_uniform_offset = 0;
        m_last_uniform_size = 0;

        m_texture_binding_count = 0;
    }

    void push_command(const jegl_cmd& command)
    {
        m_commands.push_back(command);
    }

    void reset_uniform_staging(jegl_pipeline* pipeline)
    {
        const auto* shader = pipeline->m_shader;
        const uint32_t size = shader->m_uniform_block_size;

        m_uniform_staging.resize(size);
        m_uniform_staging_size = size;

        if (size != 0)
        {
            uint8_t* staging = m_uniform_staging.data();
            memset(staging, 0, size);

            for (auto* uniform = shader->m_custom_uniforms;
                uniform != nullptr;
                uniform = uniform->m_next)
            {
                if (uniform->m_size == 0)
                    continue;

                if (uniform->m_uniform_type == jegl_shader::uniform_type::FLOAT3X3)
                {
                    for (size_t row = 0; row < 3; ++row)
                    {
                        memcpy(
                            staging + uniform->m_offset + row * 16,
                            uniform->m_value.m_float3x3[row],
                            sizeof(float) * 3);
                    }
                }
                else
                {
                    memcpy(
                        staging + uniform->m_offset,
                        &uniform->m_value,
                        uniform->m_size);
                }
            }
        }

        m_uniform_staging_dirty = true;
        m_has_last_uniform_block = false;
    }

    void validate_draw_state() const
    {
        assert(m_has_pipeline);
        assert(m_has_vertex);

        const auto* shader = m_pipeline->m_shader;
        jegl_topology topology;
        assert(_jegl_topology_of_vertex(m_vertex, &topology));
        assert(topology == m_pipeline->m_state.m_topology);
        assert(_jegl_vertex_layout_matches(m_vertex, shader));

        for (size_t i = 0; i < m_texture_binding_count; ++i)
        {
            const auto& binding = m_texture_bindings[i];
            for (size_t j = 0; j < shader->m_texture_count; ++j)
            {
                if (shader->m_textures[j].m_pass == binding.m_pass
                    && shader->m_textures[j].m_kind != binding.m_kind)
                {
                    assert(!"Texture kind does not match the shader declaration.");
                }
            }
        }
    }

    void emit_draw(jegl_cmd_op op, uint32_t a, uint32_t b, uint32_t c)
    {
        validate_draw_state();

        uint32_t uniform_offset = 0;
        uint32_t uniform_size = m_uniform_staging_size;

        if (uniform_size != 0)
        {
            if (!m_uniform_staging_dirty && m_has_last_uniform_block)
            {
                uniform_offset = m_last_uniform_offset;
                uniform_size = m_last_uniform_size;
            }
            else
            {
                const size_t aligned = (m_uniform_arena.size() + 15) & ~size_t(15);
                assert(aligned + uniform_size <= 0xFFFFFFFFu);

                m_uniform_arena.resize(aligned + uniform_size);
                memcpy(
                    m_uniform_arena.data() + aligned,
                    m_uniform_staging.data(),
                    uniform_size);

                uniform_offset = (uint32_t)aligned;
                m_last_uniform_offset = uniform_offset;
                m_last_uniform_size = uniform_size;
                m_has_last_uniform_block = true;
                m_uniform_staging_dirty = false;
            }
        }

        jegl_cmd command{};
        command.m_op = op;
        command.m_a = a;
        command.m_b = b;
        command.m_c = c;
        command.m_uniform_offset = uniform_offset;
        command.m_uniform_size = uniform_size;
        push_command(command);

        m_pass_has_draw = true;
    }

    void flush_pending_draw()
    {
        if (!m_pending_draw)
            return;

        assert(m_in_pass);
        m_pending_draw = false;
        emit_draw(
            m_pending_draw_op,
            m_pending_a,
            m_pending_b,
            m_pending_c);
    }
};

/////////////////////////////////////////////////////////////////////////////
// Pipeline / uniform reflection
/////////////////////////////////////////////////////////////////////////////

jegl_pipeline_state jegl_pipeline_state_from_shader(jegl_shader* shader)
{
    assert(shader != nullptr);

    jegl_pipeline_state state{};
    state.m_topology = jegl_topology::TRIANGLES;

    state.m_depth_test = true;
    state.m_depth_func = shader->m_depth_test;
    state.m_depth_write =
        shader->m_depth_mask == jegl_shader::depth_mask_method::ENABLE;

    state.m_blend =
        shader->m_blend_equation != jegl_shader::blend_equation::DISABLED;
    state.m_blend_equation = shader->m_blend_equation;
    state.m_blend_src_color = shader->m_blend_src_mode;
    state.m_blend_dst_color = shader->m_blend_dst_mode;
    state.m_blend_src_alpha = shader->m_blend_src_mode;
    state.m_blend_dst_alpha = shader->m_blend_dst_mode;

    state.m_cull = shader->m_cull_mode;
    state.m_front_face = jegl_front_face::COUNTER_CLOCKWISE;
    state.m_color_write_mask = 0x0F;

    if (state.m_depth_func == jegl_shader::depth_test_method::INVALID)
    {
        state.m_depth_test = false;
        state.m_depth_func = jegl_shader::depth_test_method::LESS;
    }
    if (state.m_blend_equation == jegl_shader::blend_equation::INVALID)
    {
        state.m_blend = false;
        state.m_blend_equation = jegl_shader::blend_equation::DISABLED;
    }
    if (state.m_blend_src_color == jegl_shader::blend_method::INVALID)
        state.m_blend_src_color = jegl_shader::blend_method::ONE;
    if (state.m_blend_dst_color == jegl_shader::blend_method::INVALID)
        state.m_blend_dst_color = jegl_shader::blend_method::ZERO;
    if (state.m_blend_src_alpha == jegl_shader::blend_method::INVALID)
        state.m_blend_src_alpha = jegl_shader::blend_method::ONE;
    if (state.m_blend_dst_alpha == jegl_shader::blend_method::INVALID)
        state.m_blend_dst_alpha = jegl_shader::blend_method::ZERO;
    if (state.m_cull == jegl_shader::cull_mode::INVALID)
        state.m_cull = jegl_shader::cull_mode::NONE;

    return state;
}

jegl_uniform_slot jegl_pipeline_uniform_slot(
    jegl_pipeline* pipeline, const char* name)
{
    assert(pipeline != nullptr);
    assert(name != nullptr);

    for (auto* uniform = pipeline->m_shader->m_custom_uniforms;
        uniform != nullptr;
        uniform = uniform->m_next)
    {
        if (uniform->m_uniform_type != jegl_shader::uniform_type::TEXTURE
            && std::strcmp(uniform->m_name, name) == 0)
            return jegl_uniform_slot{ uniform->m_offset, uniform->m_size };
    }
    return jegl_uniform_slot{ JEGL_UNIFORM_SLOT_NONE, 0 };
}

const jegl_builtin_uniform_slots* jegl_pipeline_builtin_uniform_slots(
    jegl_pipeline* pipeline)
{
    assert(pipeline != nullptr);
    return &pipeline->m_builtin_slots;
}

/////////////////////////////////////////////////////////////////////////////
// Command buffer
/////////////////////////////////////////////////////////////////////////////

jegl_command_buffer* jegl_cmd_create(jegl_context* context)
{
    assert(context != nullptr);
    return new jegl_command_buffer(context);
}

void jegl_cmd_close(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    delete cmd;
}

void jegl_cmd_reset(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    cmd->reset();
}

jegl_context* jegl_cmd_get_context(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    return cmd->m_context;
}

void jegl_cmd_begin_pass(
    jegl_command_buffer* cmd,
    /* OPTIONAL: nullptr means screen */ jegl_frame_buffer* target,
    const jegl_viewport* viewport)
{
    assert(cmd != nullptr);
    assert(viewport != nullptr);
    assert(viewport->width > 0);
    assert(viewport->height > 0);
    assert(!cmd->m_in_pass);
    assert(!cmd->m_pending_draw);

    cmd->retain(target);

    cmd->m_target = target;
    cmd->m_target_attachment_count =
        target != nullptr ? (uint32_t)target->m_attachment_count : 1;

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BEGIN_PASS;
    command.m_a = (uint32_t)viewport->x;
    command.m_b = (uint32_t)viewport->y;
    command.m_c = viewport->width;
    command.m_d = viewport->height;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(target);

    cmd->m_in_pass = true;
    cmd->m_pass_has_draw = false;
    cmd->m_has_pipeline = false;
    cmd->m_has_vertex = false;
    cmd->m_pipeline = nullptr;
    cmd->m_vertex = nullptr;
    cmd->m_texture_binding_count = 0;
    cmd->m_uniform_staging_dirty = false;
    cmd->m_has_last_uniform_block = false;

    cmd->push_command(command);
}

void jegl_cmd_clear_color(
    jegl_command_buffer* cmd, uint32_t attachment, const float rgba[4])
{
    assert(cmd != nullptr);
    assert(rgba != nullptr);
    assert(cmd->m_in_pass);
    cmd->flush_pending_draw();
    assert(!cmd->m_pass_has_draw);
    assert(attachment < cmd->m_target_attachment_count);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::CLEAR_COLOR;
    command.m_a = attachment;
    command.m_f[0] = rgba[0];
    command.m_f[1] = rgba[1];
    command.m_f[2] = rgba[2];
    command.m_f[3] = rgba[3];
    cmd->push_command(command);
}

void jegl_cmd_clear_depth(jegl_command_buffer* cmd, float depth)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    cmd->flush_pending_draw();
    assert(!cmd->m_pass_has_draw);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::CLEAR_DEPTH;
    command.m_f[0] = depth;
    cmd->push_command(command);
}

void jegl_cmd_end_pass(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    cmd->flush_pending_draw();

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::END_PASS;
    cmd->push_command(command);

    cmd->m_in_pass = false;
    cmd->m_target = nullptr;
}

void jegl_cmd_bind_pipeline(
    jegl_command_buffer* cmd, jegl_pipeline* pipeline)
{
    assert(cmd != nullptr);
    assert(pipeline != nullptr);
    assert(cmd->m_in_pass);
    assert(pipeline->m_context == cmd->m_context);
    cmd->flush_pending_draw();

    cmd->retain(pipeline);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_PIPELINE;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(pipeline);
    cmd->push_command(command);

    cmd->m_has_pipeline = true;
    cmd->m_pipeline = pipeline;
    cmd->reset_uniform_staging(pipeline);
}

void jegl_cmd_bind_vertex(
    jegl_command_buffer* cmd, jegl_vertex* vertex)
{
    assert(cmd != nullptr);
    assert(vertex != nullptr);
    assert(cmd->m_in_pass);
    cmd->flush_pending_draw();

    cmd->retain(vertex);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_VERTEX;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(vertex);
    cmd->push_command(command);

    cmd->m_has_vertex = true;
    cmd->m_vertex = vertex;
}

void jegl_cmd_bind_uniform_buffer(
    jegl_command_buffer* cmd, jegl_uniform_buffer* buffer)
{
    assert(cmd != nullptr);
    assert(buffer != nullptr);
    assert(cmd->m_in_pass);
    cmd->flush_pending_draw();

    cmd->retain(buffer);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_UNIFORM_BUFFER;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(buffer);
    cmd->push_command(command);
}

template<typename TexT>
static void _jegl_cmd_bind_texture_impl(
    jegl_command_buffer* cmd,
    jegl_cmd_op op,
    jegl_texture_kind kind,
    uint32_t pass,
    TexT* texture)
{
    assert(cmd != nullptr);
    assert(texture != nullptr);
    assert(cmd->m_in_pass);
    assert(pass < 32);
    cmd->flush_pending_draw();

    cmd->retain(texture);

    jegl_cmd command{};
    command.m_op = op;
    command.m_a = pass;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(texture);
    cmd->push_command(command);

    bool updated = false;
    for (size_t i = 0; i < cmd->m_texture_binding_count; ++i)
    {
        if (cmd->m_texture_bindings[i].m_pass == pass)
        {
            cmd->m_texture_bindings[i].m_kind = kind;
            cmd->m_texture_bindings[i].m_texture = texture;
            updated = true;
            break;
        }
    }
    if (!updated)
    {
        auto& binding = cmd->m_texture_bindings[cmd->m_texture_binding_count++];
        binding.m_pass = pass;
        binding.m_kind = kind;
        binding.m_texture = texture;
    }
}

void jegl_cmd_bind_texture_2d(
    jegl_command_buffer* cmd, uint32_t pass, jegl_texture_2d* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_2D, jegl_texture_kind::TEXTURE_2D, pass, texture);
}

void jegl_cmd_bind_texture_3d(
    jegl_command_buffer* cmd, uint32_t pass, jegl_texture_3d* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_3D, jegl_texture_kind::TEXTURE_3D, pass, texture);
}

void jegl_cmd_bind_texture_array(
    jegl_command_buffer* cmd, uint32_t pass, jegl_texture_array* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_ARRAY, jegl_texture_kind::TEXTURE_ARRAY, pass, texture);
}

void jegl_cmd_bind_texture_cube(
    jegl_command_buffer* cmd, uint32_t pass, jegl_texture_cube* texture)
{
    _jegl_cmd_bind_texture_impl(
        cmd, jegl_cmd_op::BIND_TEXTURE_CUBE, jegl_texture_kind::TEXTURE_CUBE, pass, texture);
}

void jegl_cmd_bind_sampler(
    jegl_command_buffer* cmd, uint32_t sampler_id, jegl_sampler* sampler)
{
    assert(cmd != nullptr);
    assert(sampler != nullptr);
    assert(cmd->m_in_pass);
    assert(cmd->m_has_pipeline);
    assert(sampler_id < 16);
    cmd->flush_pending_draw();

    cmd->retain(sampler);

    jegl_cmd command{};
    command.m_op = jegl_cmd_op::BIND_SAMPLER;
    command.m_a = sampler_id;
    command.m_ptr0 = reinterpret_cast<uintptr_t>(sampler);
    cmd->push_command(command);
}

void jegl_cmd_set_uniform(
    jegl_command_buffer* cmd,
    jegl_uniform_slot slot,
    const void* data,
    uint32_t size)
{
    assert(cmd != nullptr);
    assert(data != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JEGL_UNIFORM_SLOT_NONE);
    assert(slot.m_size > 0);
    assert(size == slot.m_size);
    assert(slot.m_offset + size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        data,
        size);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void jegl_cmd_set_uniform_float(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, float value)
{
    assert(slot.m_size == sizeof(float));
    jegl_cmd_set_uniform(cmd, slot, &value, sizeof(value));
}

void jegl_cmd_set_uniform_float2(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, float x, float y)
{
    const float value[2] = { x, y };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_float3(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, float x, float y, float z)
{
    const float value[3] = { x, y, z };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_float4(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, float x, float y, float z, float w)
{
    const float value[4] = { x, y, z, w };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_int(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, int value)
{
    assert(slot.m_size == sizeof(int));
    jegl_cmd_set_uniform(cmd, slot, &value, sizeof(value));
}

void jegl_cmd_set_uniform_int2(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, int x, int y)
{
    const int value[2] = { x, y };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_int3(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, int x, int y, int z)
{
    const int value[3] = { x, y, z };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_int4(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, int x, int y, int z, int w)
{
    const int value[4] = { x, y, z, w };
    assert(slot.m_size == sizeof(value));
    jegl_cmd_set_uniform(cmd, slot, value, sizeof(value));
}

void jegl_cmd_set_uniform_float2x2(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, const float (*mat)[2])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JEGL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == sizeof(float) * 2 * 2);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        mat,
        sizeof(float) * 2 * 2);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void jegl_cmd_set_uniform_float3x3(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, const float (*mat)[3])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JEGL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == 48);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    uint8_t* staging = cmd->m_uniform_staging.data() + slot.m_offset;
    for (size_t row = 0; row < 3; ++row)
    {
        memcpy(staging + row * 16, mat[row], sizeof(float) * 3);
        memset(staging + row * 16 + 12, 0, sizeof(float));
    }

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void jegl_cmd_set_uniform_float4x4(
    jegl_command_buffer* cmd, jegl_uniform_slot slot, const float (*mat)[4])
{
    assert(cmd != nullptr);
    assert(mat != nullptr);
    assert(cmd->m_has_pipeline);
    assert(slot.m_offset != JEGL_UNIFORM_SLOT_NONE);
    assert(slot.m_size == sizeof(float) * 4 * 4);
    assert(slot.m_offset + slot.m_size <= cmd->m_uniform_staging_size);

    memcpy(
        cmd->m_uniform_staging.data() + slot.m_offset,
        mat,
        sizeof(float) * 4 * 4);

    cmd->m_uniform_staging_dirty = true;
    cmd->m_has_last_uniform_block = false;
}

void jegl_cmd_draw_indexed(
    jegl_command_buffer* cmd,
    uint32_t index_count, uint32_t first_index, uint32_t instance_count)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    assert(index_count > 0);
    assert(instance_count > 0);

    cmd->flush_pending_draw();
    cmd->emit_draw(
        jegl_cmd_op::DRAW_INDEXED,
        index_count,
        first_index,
        instance_count);
}

void jegl_cmd_draw(
    jegl_command_buffer* cmd,
    uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count)
{
    assert(cmd != nullptr);
    assert(cmd->m_in_pass);
    assert(vertex_count > 0);
    assert(instance_count > 0);

    cmd->flush_pending_draw();
    cmd->emit_draw(
        jegl_cmd_op::DRAW,
        vertex_count,
        first_vertex,
        instance_count);
}

void jegl_cmd_commit(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    assert(cmd->m_context != nullptr);
    assert(jeecs::graphic::_current_graphic_thread == cmd->m_context);

    if (cmd->m_pending_draw)
        cmd->flush_pending_draw();

    // A pass begun by jegl_branch_new_command_buffer may still be open; close
    // it here so callers can follow the "record, commit" flow.
    if (cmd->m_in_pass)
    {
        jegl_cmd command{};
        command.m_op = jegl_cmd_op::END_PASS;
        cmd->push_command(command);
        cmd->m_in_pass = false;
        cmd->m_target = nullptr;
    }

    const auto* render = cmd->m_context->m_apis->m_render_apis;
    assert(render != nullptr);
    assert(render->m_abi_version == JEGL_RENDER_API_ABI_VERSION);
    assert(render->m_struct_size == sizeof(jegl_graphic_api_render));

    jegl_context* const graphic_context = cmd->m_context;
    const auto impl_context = graphic_context->m_graphic_impl_context;
    render->commit_begin(impl_context);

    jegl_pipeline* last_pipeline = nullptr;
    jegl_vertex* last_vertex = nullptr;
    jegl_uniform_buffer* last_ubo = nullptr;
    void* last_texture[32] = {};
    uint32_t last_texture_kind[32] = {};
    jegl_sampler* last_sampler[16] = {};

    for (const auto& command : cmd->m_commands)
    {
        switch (command.m_op)
        {
        case jegl_cmd_op::BEGIN_PASS:
        {
            auto* target =
                reinterpret_cast<jegl_frame_buffer*>(command.m_ptr0);

            if (target != nullptr)
            {
                const bool ready = _jegl_prepare_framebuffer_resource(target);
                assert(ready);
                (void)ready;
            }

            const jegl_viewport viewport{
                (int32_t)command.m_a,
                (int32_t)command.m_b,
                command.m_c,
                command.m_d,
            };
            render->begin_pass(impl_context, target, &viewport);

            last_pipeline = nullptr;
            last_vertex = nullptr;
            last_ubo = nullptr;
            memset(last_texture, 0, sizeof(last_texture));
            memset(last_texture_kind, 0, sizeof(last_texture_kind));
            memset(last_sampler, 0, sizeof(last_sampler));
            break;
        }
        case jegl_cmd_op::CLEAR_COLOR:
            render->clear_color(impl_context, command.m_a, command.m_f);
            break;
        case jegl_cmd_op::CLEAR_DEPTH:
            render->clear_depth(impl_context, command.m_f[0]);
            break;
        case jegl_cmd_op::END_PASS:
            render->end_pass(impl_context);
            break;

        case jegl_cmd_op::BIND_PIPELINE:
        {
            auto* pipeline =
                reinterpret_cast<jegl_pipeline*>(command.m_ptr0);

            const auto prepare =
                _jegl_prepare_shader_resource(pipeline->m_shader);
            assert(prepare != jegl_resource_prepare_result::INVALID);

            const auto pipeline_prepare =
                _jegl_prepare_pipeline_resource(pipeline);
            assert(pipeline_prepare != jegl_resource_prepare_result::INVALID);

            if (prepare != jegl_resource_prepare_result::READY
                || pipeline_prepare != jegl_resource_prepare_result::READY)
            {
                render->pipeline_init(impl_context, pipeline);
                pipeline->m_backend_initialized = true;
            }

            if (last_pipeline != pipeline)
            {
                render->pipeline_bind(impl_context, pipeline);
                last_pipeline = pipeline;
            }
            break;
        }
        case jegl_cmd_op::BIND_VERTEX:
        {
            auto* vertex =
                reinterpret_cast<jegl_vertex*>(command.m_ptr0);

            const bool ready = _jegl_prepare_vertex_resource(vertex);
            assert(ready);
            (void)ready;

            if (last_vertex != vertex)
            {
                render->bind_vertex(impl_context, vertex);
                last_vertex = vertex;
            }
            break;
        }
        case jegl_cmd_op::BIND_UNIFORM_BUFFER:
        {
            auto* buffer =
                reinterpret_cast<jegl_uniform_buffer*>(command.m_ptr0);

            const bool ready = _jegl_prepare_uniform_buffer_resource(buffer);
            assert(ready);
            (void)ready;

            if (last_ubo != buffer)
            {
                render->bind_uniform_buffer(impl_context, buffer);
                last_ubo = buffer;
            }
            break;
        }
        case jegl_cmd_op::BIND_TEXTURE_2D:
        case jegl_cmd_op::BIND_TEXTURE_3D:
        case jegl_cmd_op::BIND_TEXTURE_ARRAY:
        case jegl_cmd_op::BIND_TEXTURE_CUBE:
        {
            const uint32_t pass = command.m_a;
            assert(pass < 32);

            const uint32_t kind =
                command.m_op == jegl_cmd_op::BIND_TEXTURE_2D ? 0u
                : command.m_op == jegl_cmd_op::BIND_TEXTURE_3D ? 1u
                : command.m_op == jegl_cmd_op::BIND_TEXTURE_ARRAY ? 2u
                : 3u;

            void* texture = reinterpret_cast<void*>(command.m_ptr0);
            bool ready = false;

            switch (command.m_op)
            {
            case jegl_cmd_op::BIND_TEXTURE_2D:
                ready = _jegl_prepare_texture_2d_resource(
                    static_cast<jegl_texture_2d*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_3D:
                ready = _jegl_prepare_texture_3d_resource(
                    static_cast<jegl_texture_3d*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_ARRAY:
                ready = _jegl_prepare_texture_array_resource(
                    static_cast<jegl_texture_array*>(texture));
                break;
            case jegl_cmd_op::BIND_TEXTURE_CUBE:
                ready = _jegl_prepare_texture_cube_resource(
                    static_cast<jegl_texture_cube*>(texture));
                break;
            default:
                break;
            }
            assert(ready);
            (void)ready;

            if (last_texture[pass] != texture
                || last_texture_kind[pass] != kind)
            {
                switch (command.m_op)
                {
                case jegl_cmd_op::BIND_TEXTURE_2D:
                    render->bind_texture_2d(
                        impl_context, pass, static_cast<jegl_texture_2d*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_3D:
                    render->bind_texture_3d(
                        impl_context, pass, static_cast<jegl_texture_3d*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_ARRAY:
                    render->bind_texture_array(
                        impl_context, pass, static_cast<jegl_texture_array*>(texture));
                    break;
                case jegl_cmd_op::BIND_TEXTURE_CUBE:
                    render->bind_texture_cube(
                        impl_context, pass, static_cast<jegl_texture_cube*>(texture));
                    break;
                default:
                    break;
                }

                last_texture[pass] = texture;
                last_texture_kind[pass] = kind;
            }
            break;
        }
        case jegl_cmd_op::BIND_SAMPLER:
        {
            const uint32_t sampler_id = command.m_a;
            auto* sampler =
                reinterpret_cast<jegl_sampler*>(command.m_ptr0);

            const auto sampler_prepare =
                _jegl_prepare_sampler_resource(sampler);
            assert(sampler_prepare != jegl_resource_prepare_result::INVALID);

            if (sampler_prepare != jegl_resource_prepare_result::READY)
            {
                render->sampler_init(impl_context, sampler);
                sampler->m_backend_initialized = true;
            }

            if (last_sampler[sampler_id] != sampler)
            {
                render->sampler_bind(impl_context, sampler_id, sampler);
                last_sampler[sampler_id] = sampler;
            }
            break;
        }

        case jegl_cmd_op::DRAW_INDEXED:
        {
            const void* uniform_data =
                command.m_uniform_size != 0
                ? cmd->m_uniform_arena.data() + command.m_uniform_offset
                : nullptr;

            render->draw_indexed(
                impl_context,
                command.m_a,
                command.m_b,
                command.m_c,
                uniform_data,
                command.m_uniform_size);
            break;
        }
        case jegl_cmd_op::DRAW:
        {
            const void* uniform_data =
                command.m_uniform_size != 0
                ? cmd->m_uniform_arena.data() + command.m_uniform_offset
                : nullptr;

            render->draw(
                impl_context,
                command.m_a,
                command.m_b,
                command.m_c,
                uniform_data,
                command.m_uniform_size);
            break;
        }
        default:
            assert(!"Unknown command.");
            break;
        }
    }

    render->commit_end(impl_context);
}

void _jegl_cmd_begin_deferred_draw(
    jegl_command_buffer* cmd,
    jegl_pipeline* pipeline,
    jegl_vertex* vertex,
    uint32_t index_count,
    uint32_t instance_count)
{
    assert(cmd != nullptr);
    assert(pipeline != nullptr);
    assert(vertex != nullptr);
    assert(cmd->m_in_pass);
    // Flush the previous deferred draw before starting a new one. Callers
    // such as the Woolang bindings may rely on "draw, set uniforms, draw"
    // without explicitly ending the previous action.
    cmd->flush_pending_draw();
    assert(index_count > 0);
    assert(instance_count > 0);

    jegl_cmd_bind_pipeline(cmd, pipeline);
    jegl_cmd_bind_vertex(cmd, vertex);

    cmd->m_pending_draw = true;
    cmd->m_pending_draw_op = jegl_cmd_op::DRAW_INDEXED;
    cmd->m_pending_a = index_count;
    cmd->m_pending_b = 0;
    cmd->m_pending_c = instance_count;
}

void _jegl_cmd_end_deferred_draw(jegl_command_buffer* cmd)
{
    assert(cmd != nullptr);
    cmd->flush_pending_draw();
}
