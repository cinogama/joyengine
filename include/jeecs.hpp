#pragma once

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include "wo.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cfloat>

#include <typeinfo>

#include <atomic>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <cstddef>
#include <cmath>
#include <random>
#include <thread>
#include <type_traits>
#include <sstream>
#include <climits>
#include <initializer_list>
#ifdef __cpp_lib_execution
#include <execution>
#endif

#define WO_FORCE_CAPI extern "C"{
#define WO_FORCE_CAPI_END }

#ifdef _WIN32
#   define JE_IMPORT __declspec(dllimport)
#   define JE_EXPORT __declspec(dllexport)
#else
#   define JE_IMPORT extern
#   define JE_EXPORT extern
#endif

#ifdef JE_IMPL
#   define JE_IMPORT_OR_EXPORT JE_EXPORT
#else
#   define JE_IMPORT_OR_EXPORT JE_IMPORT
#endif

#define JE_API JE_IMPORT_OR_EXPORT

#ifdef _WIN32
#		define JE_OS_WINDOWS
#elif defined(__ANDROID__)
#		define JE_OS_ANDROID
#elif defined(__linux__)
#		define JE_OS_LINUX
#else
#		define JE_OS_UNKNOWN
#endif

#if defined(_X86_)||defined(__i386)||(defined(_WIN32)&&!defined(_WIN64))
#		define JE_PLATFORM_X86
#		define JE_PLATFORM_M32
#elif defined(__x86_64)||defined(_M_X64)
#		define JE_PLATFORM_X64
#		define JE_PLATFORM_M64
#elif defined(__arm)
#		define JE_PLATFORM_ARM
#		define JE_PLATFORM_M32
#elif defined(__aarch64__)
#		define JE_PLATFORM_ARM64
#		define JE_PLATFORM_M64
#else
#		define JE_PLATFORM_UNKNOWN
#endif

namespace jeecs
{
    namespace typing
    {
        using typehash_t = size_t;
        using typeid_t = size_t;

        constexpr typeid_t NOT_TYPEID_FLAG = ((typeid_t)1) << ((typeid_t)(8 * sizeof(NOT_TYPEID_FLAG)) - 1);
        constexpr typeid_t INVALID_TYPE_ID = SIZE_MAX;
        constexpr uint32_t INVALID_UINT32 = (uint32_t)-1;
        constexpr size_t ALLIGN_BASE = alignof(std::max_align_t);

        struct type_info;

        using module_entry_t = void(*)(void);
        using module_leave_t = void(*)(void);

        using construct_func_t = void(*)(void*, void*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
        using to_string_func_t = const char* (*)(const void*);
        using parse_func_t = void(*)(void*, const char*);

        using update_func_t = void(*)(void*);

        using entity_id_in_chunk_t = size_t;
        using version_t = size_t;

        struct uuid
        {
            union
            {
                struct
                {
                    uint64_t a;
                    uint64_t b;
                };
                struct
                {
                    uint32_t x;
                    uint16_t y; // Time stamp
                    uint16_t z;

                    uint16_t w; // Inc L16
                    uint16_t u; // Inc H16
                    uint32_t v; // Random
                };
            };

            inline bool operator == (const uuid& uid) const noexcept
            {
                return a == uid.a && b == uid.b;
            }

            inline std::string to_string() const
            {
                char buf[sizeof(a) * 2 + sizeof(b) * 2 + 2];
                snprintf(buf, sizeof(buf), "%016llX-%016llX", a, b);
                return buf;
            }
            inline void parse(const std::string& buf)
            {
                sscanf(buf.c_str(), "%llX-%llX", &a, &b);
            }
        };

        using uid_t = uuid;
        using ms_stamp_t = uint64_t;

        template<typename T>
        struct _origin_type
        {
            template<typename U>
            using _origin_t =
                typename std::remove_cv<
                typename std::remove_reference<
                typename std::remove_pointer<U>::type
                >::type
                >::type;

            static auto _type_selector() // -> T*
            {
                if constexpr (
                    std::is_reference<T>::value
                    || std::is_pointer<T>::value
                    || std::is_const<T>::value
                    || std::is_volatile<T>::value)
                    return _origin_type<_origin_t<T>>::_type_selector();
                else
                    return (T*) nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        template<typename T>
        using origin_t = typename _origin_type<T>::type;

        template<class F>
        struct function_traits
        {
        private:
            using call_type = function_traits<decltype(&F::operator())>;

        public:
            using return_type = typename call_type::return_type;
            using flat_func_t = typename call_type::flat_func_t;
            using this_t = void;

            static const std::size_t arity = call_type::arity;

            template <std::size_t N>
            struct argument
            {
                static_assert(N < arity, "error: invalid parameter index.");
                using type = typename call_type::template argument<N>::type;
            };
        };

        template<class R, class... Args>
        struct function_traits<R(*)(Args...)> : public function_traits < R(Args...) >
        {
        };

        template<class R, class... Args>
        struct function_traits < R(Args...) >
        {
            using return_type = R;
            using flat_func_t = R(Args...);
            using this_t = void;

            static const std::size_t arity = sizeof...(Args);

            template <std::size_t N>
            struct argument
            {
                static_assert(N < arity, "error: invalid parameter index.");
                using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
            };


        };

        // member function pointer
        template<class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...)> : public function_traits < R(Args...) >
        {
            using this_t = C;
        };

        // const member function pointer
        template<class C, class R, class... Args>
        struct function_traits<R(C::*)(Args...) const> : public function_traits < R(Args...) >
        {
            using this_t = C;
        };

        // member object pointer
        template<class C, class R>
        struct function_traits<R(C::*)> : public function_traits < R(void) >
        {
            using this_t = C;
        };

        template<class F>
        struct function_traits<F&> : public function_traits < F >
        {};

        template<class F>
        struct function_traits<F&&> : public function_traits < F >
        {};

        template<size_t n, typename T, typename ... Ts>
        struct _variadic_type_indexer
        {
            static auto _type_selector() // -> T*
            {
                if constexpr (n != 0)
                    return _variadic_type_indexer<n - 1, Ts...>::_type_selector();
                else
                    return (T*) nullptr;
            }

            using type = typename std::remove_pointer<decltype(_type_selector())>::type;
        };

        template<size_t n, typename ... Ts>
        using index_types_t = typename _variadic_type_indexer<n, Ts...>::type;
    }

    class game_system;
    class game_world;

    struct game_entity
    {
        enum class entity_stat
        {
            UNAVAILABLE,    // Entity is destroied or just not ready,
            READY,          // Entity is OK, and just work as normal.
        };

        void* _m_in_chunk;
        jeecs::typing::entity_id_in_chunk_t   _m_id;
        jeecs::typing::version_t              _m_version;

        inline game_entity& _set_arch_chunk_info(
            void* chunk,
            jeecs::typing::entity_id_in_chunk_t index,
            jeecs::typing::version_t ver) noexcept
        {
            _m_in_chunk = chunk;
            _m_id = index;
            _m_version = ver;

            return *this;
        }

        template<typename T>
        inline T* get_component() const noexcept;

        template<typename T>
        inline T* add_component() const noexcept;

        template<typename T>
        inline void remove_component() const noexcept;

        inline jeecs::game_world game_world() const noexcept;

        inline void close() const noexcept;

        inline bool valid() const noexcept;

        inline std::string name();

        inline std::string name(const std::string& _name);

        inline bool operator == (const game_entity& e) const noexcept
        {
            return _m_in_chunk == e._m_in_chunk && _m_id == e._m_id && _m_version == e._m_version;
        }
        inline bool operator != (const game_entity& e) const noexcept
        {
            return _m_in_chunk != e._m_in_chunk || _m_id != e._m_id || _m_version != e._m_version;
        }
    };

    struct dependence;

    namespace input
    {
        enum class keycode
        {
            A = 'A', B, C, D, E, F, G, H, I, J, K, L,
            M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
            _1 = '1', _2, _3, _4, _5, _6, _7, _8, _9,
            _0, _ = ' ',

            L_SHIFT = 256,
            L_CTRL,
            L_ALT,
            TAB, ENTER, ESC, BACKSPACE,

            MOUSE_L_BUTTION = 512,
            MOUSE_M_BUTTION,
            MOUSE_R_BUTTION,

            MAX_KEY_CODE = 1024,
        };
    }

    namespace graphic
    {
        struct character;
    }
}

namespace std
{
    template<>
    struct hash<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator() (const jeecs::typing::uid_t& uid) const noexcept
        {
            if constexpr (sizeof(size_t) == 8)
                return uid.b ^ uid.a;
            else
                return (size_t)(uid.b >> 32) ^ (size_t)uid.a;
        }
    };

    template<>
    struct equal_to<jeecs::typing::uid_t>
    {
        inline constexpr size_t operator() (const jeecs::typing::uid_t& a, const jeecs::typing::uid_t& b) const noexcept
        {
            return a.a == b.a && a.b == b.b;
        }
    };
}

WO_FORCE_CAPI
JE_API void* je_mem_alloc(size_t sz);
JE_API void* je_mem_realloc(void* mem, size_t sz);
JE_API void je_mem_free(void* ptr);

JE_API void je_init(int argc, char** argv);
JE_API void je_finish(void);

JE_API const char* je_build_version();

JE_API void je_log_strat(void);
JE_API void je_log_shutdown(void);

#define JE_LOG_NORMAL 0
#define JE_LOG_INFO 1
#define JE_LOG_WARNING 2
#define JE_LOG_ERROR 3
#define JE_LOG_FATAL 4

JE_API size_t je_log_register_callback(void(*func)(int level, const char* msg, void* custom), void* custom);
JE_API void* je_log_unregister_callback(size_t regid);
JE_API void je_log(int level, const char* format, ...);

typedef enum je_typing_class
{
    JE_BASIC_TYPE,
    JE_COMPONENT,
    JE_SYSTEM,
} je_typing_class;
// You should promise: different type should have different name.
JE_API bool je_typing_find_or_register(
    jeecs::typing::typeid_t* out_typeid,
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover,
    jeecs::typing::to_string_func_t _to_string,
    jeecs::typing::parse_func_t     _parse,
    jeecs::typing::update_func_t    _pre_update,
    jeecs::typing::update_func_t    _update,
    jeecs::typing::update_func_t    _late_update,
    je_typing_class                 _typecls);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_name(
    const char* type_name);

JE_API void je_typing_unregister(
    jeecs::typing::typeid_t _id);

JE_API void je_register_member(
    jeecs::typing::typeid_t         _classid,
    const jeecs::typing::type_info* _membertype,
    const char* _member_name,
    ptrdiff_t                       _member_offset);

////////////////////// ARCH //////////////////////

JE_API void* je_arch_get_chunk(void* archtype);
JE_API void* je_arch_next_chunk(void* chunk);
JE_API const void* je_arch_entity_meta_addr_in_chunk(void* chunk);
JE_API size_t je_arch_entity_meta_size(void);
JE_API size_t je_arch_entity_meta_state_offset(void);
JE_API size_t je_arch_entity_meta_version_offset(void);

////////////////////// ECS //////////////////////

JE_API void* je_ecs_universe_create(void);
JE_API void je_ecs_universe_loop(void* universe);
JE_API void je_ecs_universe_destroy(void* universe);
JE_API void je_ecs_universe_stop(void* universe);

JE_API void je_ecs_universe_register_exit_callback(void* universe, void(*callback)(void*), void* arg);

typedef double(*je_job_for_worlds_t)(void* world);
typedef double(*je_job_call_once_t)(void);

/*
Jobs in universe have 2*3 types:
2: For all worlds / Call once job;
3: Pre/Normal/After job;

For all worlds job will be execute with each world in universe.
Call once job only execute 1 time per frame.

Pre job used for timing sensitive tasks, such as frame-update
Normal job used for normal tasks.
After job used to update some data based on normal job.

For example, graphic update will be pre-callonce-job.
*/

JE_API void je_ecs_universe_register_pre_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_register_pre_call_once_job(void* universe, je_job_call_once_t job);
JE_API void je_ecs_universe_register_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_register_call_once_job(void* universe, je_job_call_once_t job);
JE_API void je_ecs_universe_register_after_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_register_after_call_once_job(void* universe, je_job_call_once_t job);

JE_API void je_ecs_universe_unregister_pre_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_unregister_pre_call_once_job(void* universe, je_job_call_once_t job);
JE_API void je_ecs_universe_unregister_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_unregister_call_once_job(void* universe, je_job_call_once_t job);
JE_API void je_ecs_universe_unregister_after_for_worlds_job(void* universe, je_job_for_worlds_t job);
JE_API void je_ecs_universe_unregister_after_call_once_job(void* universe, je_job_call_once_t job);

JE_API void* je_ecs_world_in_universe(void* world);
JE_API void* je_ecs_world_create(void* in_universe);
JE_API void je_ecs_world_destroy(void* world);

JE_API bool je_ecs_world_is_valid(void* world);

JE_API size_t je_ecs_world_archmgr_updated_version(void* world);
JE_API void je_ecs_world_update_dependences_archinfo(void* world, jeecs::dependence* dependence);
JE_API void je_ecs_clear_dependence_archinfos(jeecs::dependence* dependence);

JE_API jeecs::game_system* je_ecs_world_add_system_instance(void* world, const jeecs::typing::type_info* type);
JE_API jeecs::game_system* je_ecs_world_get_system_instance(void* world, const jeecs::typing::type_info* type);
JE_API void je_ecs_world_remove_system_instance(void* world, const jeecs::typing::type_info* type);

JE_API void je_ecs_world_create_entity_with_components(
    void* world,
    jeecs::game_entity* out_entity,
    jeecs::typing::typeid_t* component_ids);
JE_API void je_ecs_world_destroy_entity(
    void* world,
    const jeecs::game_entity* entity);
JE_API void* je_ecs_world_entity_add_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);
JE_API void je_ecs_world_entity_remove_component(
    void* world,
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);
JE_API void* je_ecs_world_entity_get_component(
    const jeecs::game_entity* entity,
    const jeecs::typing::type_info* component_info);
JE_API void* je_ecs_world_of_entity(const jeecs::game_entity* entity);
JE_API bool je_ecs_world_validate_entity(const jeecs::game_entity* entity);
JE_API const char* je_ecs_get_name_of_entity(const jeecs::game_entity* entity);
JE_API const char* je_ecs_set_name_of_entity(const jeecs::game_entity* entity, const char* name);
/////////////////////////// Time&Sleep /////////////////////////////////

JE_API double je_clock_time();
JE_API jeecs::typing::ms_stamp_t je_clock_time_stamp();
JE_API void je_clock_sleep_until(double time);
JE_API void je_clock_sleep_for(double time);
JE_API void je_clock_suppress_sleep(double sup_stax);

/////////////////////////// JUID /////////////////////////////////

JE_API jeecs::typing::uid_t je_uid_generate(void);

/////////////////////////// CORE /////////////////////////////////

JE_API void jeecs_entry_register_core_systems(void);

/////////////////////////// FILE /////////////////////////////////

struct jeecs_file
{
    FILE* m_native_file_handle;
    size_t m_file_length;
};

JE_API jeecs_file* jeecs_file_open(const char* path);
JE_API void        jeecs_file_close(jeecs_file* file);
JE_API size_t      jeecs_file_read(
    void* out_buffer,
    size_t elem_size,
    size_t count,
    jeecs_file* file);

// If ignore_crc64 == true, cache will always work even if origin file changed.
JE_API jeecs_file* jeecs_load_cache_file(const char* filepath, uint32_t format_version, wo_integer_t virtual_crc64);
// If usecrc64 != 0, cache file will use it instead of reading from origin file.
JE_API void* jeecs_create_cache_file(const char* filepath, uint32_t format_version, wo_integer_t usecrc64);
JE_API size_t jeecs_write_cache_file(const void* write_buffer, size_t elem_size, size_t count, void* file);
JE_API void jeecs_close_cache_file(void* file);

/////////////////////////// GRAPHIC //////////////////////////////
// Here to store low-level-graphic-api.

struct jegl_interface_config
{
    size_t m_windows_width, m_windows_height;
    size_t m_resolution_x, m_resolution_y;

    size_t m_fps;

    const char* m_title;

    bool m_fullscreen;
};

struct jegl_thread_notifier;
struct jegl_graphic_api;
struct jegl_resource;

struct jegl_thread
{
    std::thread* _m_thread;
    jegl_thread_notifier* _m_thread_notifier;
    void* _m_interface_handle;

    jeecs::typing::version_t m_version;

    jegl_graphic_api* m_apis;
    std::atomic_bool  m_stop_update;
};

struct jegl_texture
{
    using pixel_data_t = uint8_t;
    enum texture_format : uint16_t
    {
        MONO = 0x0001,
        RGB = 0x0003,
        RGBA = 0x0004,
        COLOR_DEPTH_MASK = 0x000F,

        COLOR16 = 0x0010,
        DEPTH = 0x0020,
        FRAMEBUF = 0x0040,
        CUBE = 0x0080,

        MSAAx1 = 0x0100,    // WTF?
        MSAAx2 = 0x0200,
        MSAAx4 = 0x0400,
        MSAAx8 = 0x0800,
        MSAAx16 = 0x1000,
        MSAA_MASK = 0xFF00,

        FORMAT_MASK = 0xFFF0,
    };
    enum texture_sampling : uint16_t
    {
        MIN_LINEAR = 0x0000,
        MIN_NEAREST = 0x0001,
        MIN_NEAREST_NEAREST_MIP = 0x0002,
        MIN_LINEAR_NEAREST_MIP = 0x0003,
        MIN_NEAREST_LINEAR_MIP = 0x0004,
        MIN_LINEAR_LINEAR_MIP = 0x0005,

        MAG_LINEAR = 0x0000,
        MAG_NEAREST = 0x0010,

        CLAMP_EDGE_X = 0x0000,
        REPEAT_X = 0x0100,
        CLAMP_EDGE_Y = 0x0000,
        REPEAT_Y = 0x1000,


        FILTER_METHOD_MASK = 0x00FF,
        MIN_FILTER_MASK = 0x000F,
        MAG_FILTER_MASK = 0x00F0,

        WRAP_METHOD_MASK = 0xFF00,
        WRAP_X_METHOD_MASK = 0x0F00,
        WRAP_Y_METHOD_MASK = 0xF000,

        LINEAR = MIN_LINEAR | MAG_LINEAR,
        NEAREST = MIN_NEAREST | MAG_NEAREST,
        CLAMP_EDGE = CLAMP_EDGE_X | CLAMP_EDGE_Y,
        REPEAT = REPEAT_X | REPEAT_Y,

        DEFAULT = LINEAR | CLAMP_EDGE,
    };

    // NOTE:
    // * Pixel data is storage from LEFT/BUTTOM to RIGHT/TOP
    // * If texture's m_pixels is nullptr, only create a texture in pipeline.
    pixel_data_t* m_pixels;
    size_t          m_width;
    size_t          m_height;

    texture_format  m_format;
    texture_sampling m_sampling;

    bool            m_modified;
};

struct jegl_vertex
{
    enum vertex_type
    {
        LINES = 0,
        LINELOOP,
        LINESTRIP,
        TRIANGLES,
        TRIANGLESTRIP,
        QUADS,
    };
    float m_size_x, m_size_y, m_size_z;
    float* m_vertex_datas;
    size_t* m_vertex_formats;
    const char* m_path;
    size_t m_format_count;
    size_t m_point_count;
    size_t m_data_count_per_point;
    vertex_type m_type;
};

struct jegl_shader
{
    enum uniform_type
    {
        INT,
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        FLOAT4X4,
        TEXTURE,
    };
    struct builtin_uniform_location
    {
        uint32_t m_builtin_uniform_m = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_v = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_p = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_mvp = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_mv = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_vp = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_local_scale = jeecs::typing::INVALID_UINT32;

        uint32_t m_builtin_uniform_tiling = jeecs::typing::INVALID_UINT32;
        uint32_t m_builtin_uniform_offset = jeecs::typing::INVALID_UINT32;

        uint32_t m_builtin_uniform_color = jeecs::typing::INVALID_UINT32;
    };

    struct unifrom_variables
    {
        const char* m_name;
        uint32_t    m_index;
        uniform_type m_uniform_type;
        bool        m_updated;
        union
        {
            struct
            {
                float x, y, z, w;
            };
            int n;
            float mat4x4[4][4];
        };

        unifrom_variables* m_next;
    };

    enum class depth_test_method : int8_t
    {
        INVALID = -1,

        OFF,
        NEVER,
        LESS,       /* DEFAULT */
        EQUAL,
        LESS_EQUAL,
        GREATER,
        NOT_EQUAL,
        GREATER_EQUAL,
        ALWAYS,
    };

    enum class depth_mask_method : int8_t
    {
        INVALID = -1,

        DISABLE,
        ENABLE,     /* DEFAULT */
    };

    enum class alpha_test_method : int8_t
    {
        INVALID = -1,

        DISABLE,    /* DEFAULT */
        ENABLE,
    };

    enum class blend_method : int8_t
    {
        INVALID = -1,

        ZERO,       /* DEFAULT SRC = ONE, DST = ZERO (DISABLE BLEND.) */
        ONE,

        SRC_COLOR,
        SRC_ALPHA,

        ONE_MINUS_SRC_ALPHA,
        ONE_MINUS_SRC_COLOR,

        DST_COLOR,
        DST_ALPHA,

        ONE_MINUS_DST_ALPHA,
        ONE_MINUS_DST_COLOR,

        CONST_COLOR,
        ONE_MINUS_CONST_COLOR,

        CONST_ALPHA,
        ONE_MINUS_CONST_ALPHA,
    };

    enum class cull_mode : int8_t
    {
        INVALID = -1,

        NONE,       /* DEFAULT */
        FRONT,
        BACK,
        ALL,
    };

    const char* m_vertex_glsl_src;
    const char* m_fragment_glsl_src;
    unifrom_variables* m_custom_uniforms;
    builtin_uniform_location m_builtin_uniforms;

    depth_test_method   m_depth_test;
    depth_mask_method   m_depth_mask;
    blend_method        m_blend_src_mode, m_blend_dst_mode;
    cull_mode           m_cull_mode;
};

struct jegl_frame_buffer
{
    // In fact, attachment_t is jeecs::basic::resource<jeecs::graphic::texture>
    typedef struct attachment_t attachment_t;
    attachment_t* m_output_attachments;
    size_t          m_attachment_count;
    size_t          m_width;
    size_t          m_height;
};

struct jegl_uniform_buffer
{
    size_t      m_buffer_binding_place;
    size_t      m_buffer_size;
    uint8_t* m_buffer;

    // Used for marking update range;
    size_t      m_update_begin_offset;
    size_t      m_update_length;
};

struct jegl_resource
{
    struct jegl_destroy_resouce
    {
        jegl_resource* m_destroy_resource;
        size_t m_retry_times;
        jegl_destroy_resouce* last;
    };

    using jegl_custom_resource_t = void*;
    enum type
    {
        VERTEX,         // Mesh
        TEXTURE,        // Texture
        SHADER,         // Shader
        FRAMEBUF,       // Framebuffer
        UNIFORMBUF,     // UniformBlock
    };
    type m_type;
    jegl_thread* m_graphic_thread;
    jeecs::typing::version_t m_graphic_thread_version;
    union
    {
        void* m_ptr;
        size_t m_handle;
        struct
        {
            uint32_t m_uint1;
            uint32_t m_uint2;
        };
    };
    const char* m_path;
    int* m_raw_ref_count;
    union
    {
        jegl_custom_resource_t m_custom_resource;
        jegl_texture* m_raw_texture_data;
        jegl_vertex* m_raw_vertex_data;
        jegl_shader* m_raw_shader_data;
        jegl_frame_buffer* m_raw_framebuf_data;
        jegl_uniform_buffer* m_raw_uniformbuf_data;
    };
};

struct jegl_graphic_api
{
    using custom_interface_info_t = void*;

    using startup_interface_func_t = custom_interface_info_t(*)(jegl_thread*, const jegl_interface_config*, bool);
    using shutdown_interface_func_t = void(*)(jegl_thread*, custom_interface_info_t, bool);
    using update_interface_func_t = bool(*)(jegl_thread*, custom_interface_info_t);

    using get_windows_size_func_t = void(*)(jegl_thread*, size_t*, size_t*);

    using init_resource_func_t = void(*)(jegl_thread*, jegl_resource*);
    using using_resource_func_t = void(*)(jegl_thread*, jegl_resource*);
    using update_resource_func_t = void(*)(jegl_thread*, jegl_resource*);
    using close_resource_func_t = void(*)(jegl_thread*, jegl_resource*);

    using draw_vertex_func_t = void(*)(jegl_resource*);
    using bind_texture_func_t = void(*)(jegl_resource*, size_t);

    using set_rendbuf_func_t = void(*)(jegl_thread*, jegl_resource*, size_t x, size_t y, size_t w, size_t h);
    using clear_framebuf_func_t = void(*)(jegl_thread*, jegl_resource*);
    using update_shared_uniform_func_t = void(*)(jegl_thread*, size_t offset, size_t datalen, const void* data);

    using get_uniform_location_func_t = int(*)(jegl_resource*, const char*);
    using set_uniform_func_t = void(*)(jegl_resource*, int, jegl_shader::uniform_type, const void*);

    startup_interface_func_t    init_interface;
    shutdown_interface_func_t   shutdown_interface;
    update_interface_func_t     pre_update_interface;
    update_interface_func_t     update_interface;
    update_interface_func_t     late_update_interface;

    get_windows_size_func_t     get_windows_size;

    init_resource_func_t        init_resource;
    using_resource_func_t       using_resource;
    close_resource_func_t       close_resource;

    draw_vertex_func_t          draw_vertex;
    bind_texture_func_t         bind_texture;

    set_rendbuf_func_t          set_rend_buffer;
    clear_framebuf_func_t       clear_rend_buffer;
    clear_framebuf_func_t       clear_rend_buffer_color;
    clear_framebuf_func_t       clear_rend_buffer_depth;

    get_uniform_location_func_t get_uniform_location;   // this function only used after the shader has been 'using' now.
    set_uniform_func_t          set_uniform;            // too,

};
static_assert(sizeof(jegl_graphic_api) % sizeof(void*) == 0);

using jeecs_api_register_func_t = void(*)(jegl_graphic_api*);

JE_API jegl_thread* jegl_start_graphic_thread(
    jegl_interface_config config,
    jeecs_api_register_func_t register_func,
    void(*frame_rend_work)(void*, jegl_thread*),
    void* arg);

JE_API void jegl_terminate_graphic_thread(jegl_thread* thread_handle);

JE_API bool jegl_update(jegl_thread* thread_handle);

JE_API void jegl_reboot_graphic_thread(
    jegl_thread* thread_handle,
    jegl_interface_config config);

JE_API void jegl_get_windows_size(size_t* x, size_t* y);

JE_API jegl_resource* jegl_load_texture(const char* path);
JE_API jegl_resource* jegl_create_texture(size_t width, size_t height, jegl_texture::texture_format format);

JE_API jegl_resource* jegl_load_vertex(const char* path);
JE_API jegl_resource* jegl_create_vertex(
    jegl_vertex::vertex_type    type,
    const float* datas,
    const size_t* format,
    size_t                      data_length,
    size_t                      format_length);

// TODO: Support create frame buffer with different output attachment resolutions.
JE_API jegl_resource* jegl_create_framebuf(
    size_t width,
    size_t height,
    const jegl_texture::texture_format* attachment_formats,
    size_t attachment_count);

JE_API jegl_resource* jegl_copy_resource(jegl_resource* resource);

typedef struct je_stb_font_data je_font;

JE_API je_font* je_font_load(const char* fontPath, float scalex, float scaley);
JE_API void         je_font_free(je_font* font);
JE_API jeecs::graphic::character* je_font_get_char(je_font* font, unsigned long chcode);

JE_API void jegl_shader_generate_glsl(void* shader_generator, jegl_shader* write_to_shader);
JE_API void jegl_shader_free_generated_glsl(jegl_shader* write_to_shader);
JE_API jegl_resource* jegl_load_shader_source(const char* path, const char* src, bool is_virtual_file);
JE_API jegl_resource* jegl_load_shader(const char* path);

JE_API jegl_resource* jegl_create_uniformbuf(
    size_t length,
    size_t binding_place);
JE_API void jegl_update_uniformbuf(
    jegl_resource* uniformbuf,
    const void* buf,
    size_t update_offset,
    size_t update_length);

JE_API void jegl_using_opengl_apis(jegl_graphic_api* write_to_apis);

JE_API void jegl_using_resource(jegl_resource* resource);
JE_API void jegl_close_resource(jegl_resource* resource);

JE_API void jegl_using_texture(jegl_resource* texture, size_t pass);
JE_API void jegl_draw_vertex(jegl_resource* vert);

JE_API void jegl_clear_framebuffer(jegl_resource* framebuffer);
JE_API void jegl_clear_framebuffer_color(jegl_resource* framebuffer);
JE_API void jegl_clear_framebuffer_depth(jegl_resource* framebuffer);
JE_API void jegl_rend_to_framebuffer(jegl_resource* framebuffer, size_t x, size_t y, size_t w, size_t h);

JE_API int jegl_uniform_location(jegl_resource* shader, const char* name);
JE_API void jegl_uniform_int(jegl_resource* shader, int location, int value);
JE_API void jegl_uniform_float(jegl_resource* shader, int location, float value);
JE_API void jegl_uniform_float2(jegl_resource* shader, int location, float x, float y);
JE_API void jegl_uniform_float3(jegl_resource* shader, int location, float x, float y, float z);
JE_API void jegl_uniform_float4(jegl_resource* shader, int location, float x, float y, float z, float w);
JE_API void jegl_uniform_float4x4(jegl_resource* shader, int location, const float(*mat)[4]);

JE_API jegl_thread* jegl_current_thread();

JE_API void je_io_set_keystate(jeecs::input::keycode keycode, bool keydown);
JE_API void je_io_set_mousepos(int group, int x, int y);
JE_API void je_io_set_windowsize(int x, int y);
JE_API void je_io_set_wheel(int group, float count);

JE_API bool je_io_is_keydown(jeecs::input::keycode keycode);
JE_API void je_io_mouse_pos(int group, int* x, int* y);
JE_API void je_io_windowsize(int* x, int* y);
JE_API float je_io_wheel(int group);

JE_API void je_io_lock_mouse(bool lock, int x, int y);
JE_API bool je_io_should_lock_mouse(int* x, int* y);

JE_API void je_io_update_windowsize(int x, int y);
JE_API bool je_io_should_update_windowsize(int* x, int* y);

// Library / Module loader
JE_API void* je_module_load(const char* name, const char* path);
JE_API void* je_module_func(void* lib, const char* funcname);
JE_API void je_module_unload(void* lib);

// Audio
struct jeal_device;
struct jeal_source;
struct jeal_buffer;

JE_API jeal_device**    jeal_get_all_devices();
JE_API const char*      jeal_device_name(jeal_device* device);
JE_API void             jeal_using_device(jeal_device* device);

JE_API jeal_buffer*     jeal_load_buffer_from_wav(const char* filename);
JE_API void             jeal_close_buffer(jeal_buffer* buffer);

JE_API jeal_source*     jeal_open_source();
JE_API void             jeal_close_source(jeal_source* source);
JE_API void             jeal_source_set_buffer(jeal_source* source, jeal_buffer* buffer);
JE_API void             jeal_source_play(jeal_source* source);
JE_API void             jeal_source_pause(jeal_source* source);
JE_API void             jeal_source_stop(jeal_source* source);

// DEBUG API, SHOULD NOT BE USED IN GAME PROJECT, ONLY USED FOR EDITOR
#ifdef JE_ENABLE_DEBUG_API

// NOTE: need free the return result by 'je_mem_free'
// will return all alive world pointer in the universe.
// [world1, world2,..., nullptr]
JE_API void** jedbg_get_all_worlds_in_universe(void* _universes);

JE_API const char* jedbg_get_world_name(void* _world);

JE_API void jedbg_set_world_name(void* _world, const char* name);

JE_API void jedbg_free_entity(jeecs::game_entity* _entity_list);

// NOTE: need free the return result by 'je_mem_free'(and elem with jedbg_free_entity)
JE_API jeecs::game_entity** jedbg_get_all_entities_in_world(void* _world);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_components_from_entity(const jeecs::game_entity* _entity);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_registed_types(void);

// NOTE: need free the return result by 'je_mem_free'
JE_API const jeecs::typing::type_info** jedbg_get_all_system_attached_in_world(void* _world);

JE_API void jedbg_set_editor_universe(void* universe_handle);

JE_API void* jedbg_get_editor_universe(void);

JE_API bool jedbg_editor(void);

JE_API void jedbg_set_editing_entity(const jeecs::game_entity* _entity);

JE_API const jeecs::game_entity* jedbg_get_editing_entity();

// NOTE: Get graphic thread
JE_API jegl_thread* jedbg_get_editing_graphic_thread(void* universe);
JE_API void* jedbg_get_rendering_world(void* universe);
#endif

WO_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete;\
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete;

    template<typename ElemT>
    class vector
    {
        ElemT* _elems_ptr_begin = nullptr;
        ElemT* _elems_ptr_end = nullptr;
        ElemT* _elems_buffer_end = nullptr;

        static constexpr size_t _single_elem_size = sizeof(ElemT);

        inline static size_t _move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end)noexcept
        {
            for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
            {
                new(to_begin++)ElemT(std::move(*(origin_elem++)));
            }

            return (size_t)(from_end - from_begin);
        }

        inline static size_t _copy(ElemT* to_begin, ElemT* from_begin, ElemT* from_end)noexcept
        {
            for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
            {
                new(to_begin++)ElemT(*(origin_elem++));
            }

            return (size_t)(from_end - from_begin);
        }

        inline static size_t _erase(ElemT* from_begin, ElemT* from_end)noexcept
        {
            if constexpr (!std::is_trivial<ElemT>::value)
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                {
                    (origin_elem++)->~ElemT();
                }
            }

            return (size_t)(from_end - from_begin);
        }

        inline void _reserve(size_t elem_reserving_count)
        {
            const size_t _pre_reserved_count = (size_t)(_elems_buffer_end - _elems_ptr_begin);
            if (elem_reserving_count > _pre_reserved_count)
            {
                ElemT* new_reserved_begin = (ElemT*)je_mem_alloc(elem_reserving_count * _single_elem_size);
                _elems_buffer_end = new_reserved_begin + elem_reserving_count;

                _elems_ptr_end = new_reserved_begin + _move(new_reserved_begin, _elems_ptr_begin, _elems_ptr_end);
                je_mem_free(_elems_ptr_begin);
                _elems_ptr_begin = new_reserved_begin;
            }
        }

        inline void _assure(size_t assure_sz)
        {
            if (assure_sz > reserved_size())
                _reserve(2 * assure_sz);
        }

    public:
        vector()noexcept
        {
        }

        ~vector()
        {
            clear();
            je_mem_free(_elems_ptr_begin);
        }

        vector(const vector& another_list)
        {
            _reserve(another_list.size());
            _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());
        }

        vector(const std::initializer_list<ElemT>& another_list)
        {
            for (auto& elem : another_list)
                push_back(elem);
        }

        vector(ElemT* ptr, size_t length)
        {
            _elems_ptr_begin = ptr;
            _elems_ptr_end = _elems_buffer_end = _elems_ptr_begin + length;
        }

        vector(vector&& another_list)
        {
            _elems_ptr_begin = another_list._elems_ptr_begin;
            _elems_ptr_end = another_list._elems_ptr_end;
            _elems_buffer_end = another_list._elems_buffer_end;

            another_list._elems_ptr_begin =
                another_list._elems_ptr_end =
                another_list._elems_buffer_end = nullptr;
        }

        inline vector& operator = (const vector& another_list)
        {
            _reserve(another_list.size());
            _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());

            return *this;
        }

        inline vector& operator = (vector&& another_list)
        {
            clear();
            je_mem_free(_elems_ptr_begin);

            _elems_ptr_begin = another_list._elems_ptr_begin;
            _elems_ptr_end = another_list._elems_ptr_end;
            _elems_buffer_end = another_list._elems_buffer_end;

            another_list._elems_ptr_begin =
                another_list._elems_ptr_end =
                another_list._elems_buffer_end = nullptr;

            return *this;
        }

        inline size_t size() const noexcept
        {
            return _elems_ptr_end - _elems_ptr_begin;
        }
        inline bool empty() const noexcept
        {
            return _elems_ptr_end == _elems_ptr_begin;
        }
        inline ElemT& front() noexcept
        {
            return *_elems_ptr_begin;
        }
        inline ElemT& back() noexcept
        {
            return *(_elems_ptr_end - 1);
        }
        inline size_t reserved_size() const noexcept
        {
            return _elems_buffer_end - _elems_ptr_begin;
        }
        inline void clear()noexcept
        {
            _erase(_elems_ptr_begin, _elems_ptr_end);
            _elems_ptr_end = _elems_ptr_begin;
        }
        inline void push_back(const ElemT& _e)
        {
            _assure(size() + 1);
            new (_elems_ptr_end++) ElemT(_e);
        }
        inline void pop_back()noexcept
        {
            if constexpr (!std::is_trivial<ElemT>::value)
                (_elems_ptr_end--)->~ElemT();
            else
                _elems_ptr_end--;
        }

        inline auto begin() const noexcept->ElemT*
        {
            return _elems_ptr_begin;
        }

        inline auto end() const noexcept->ElemT*
        {
            return _elems_ptr_end;
        }

        inline auto front() const noexcept->ElemT&
        {
            return *_elems_ptr_begin;
        }

        inline auto back() const noexcept->ElemT&
        {
            return *(_elems_ptr_end - 1);
        }

        inline void erase(size_t index)
        {
            _elems_ptr_begin[index].~ElemT();
            _move(_elems_ptr_begin + index, _elems_ptr_begin + index + 1, _elems_ptr_end--);
        }

        inline void erase_data(const ElemT& data)
        {
            auto fnd_place = std::find(begin(), end(), data);
            if (fnd_place != end())
                erase(fnd_place - begin());
        }

        ElemT* data()const noexcept
        {
            return _elems_ptr_begin;
        }

        ElemT& operator[](size_t index)const noexcept
        {
            return _elems_ptr_begin[index];
        }
    };

    class string
    {
        char* _c_str = nullptr;
        size_t _str_len = 0;
        size_t _buf_len = 0;

        inline void _reserve(size_t buf_sz)
        {
            if (buf_sz > _buf_len)
            {
                _buf_len = buf_sz + 1;
                _c_str = (char*)je_mem_realloc(_c_str, _buf_len);
            }
        }

    public:

        ~string()
        {
            je_mem_free(_c_str);
        }

        string()noexcept
        {
            _reserve(1);
        }

        string(const string& str)noexcept
            :string(str.c_str())
        {
        }

        string(const std::string& str)noexcept
            :string(str.c_str())
        {
        }

        string(string&& str)noexcept
            :_c_str(str._c_str)
            , _buf_len(str._buf_len)
            , _str_len(str._str_len)
        {
            str._c_str = 0;
            str._buf_len = 0;
            str._str_len = 0;
        }

        string(const char* str) noexcept
            : _str_len(strlen(str))
        {
            _reserve(_str_len + 1);
            memcpy(_c_str, str, _str_len);
        }

        inline string& operator=(const string& str) noexcept
        {
            return *this = str.c_str();
        }

        inline string& operator=(string&& str) noexcept
        {
            je_mem_free(_c_str);

            _c_str = str._c_str;
            _buf_len = str._buf_len;
            _str_len = str._str_len;

            str._c_str = 0;
            str._buf_len = 0;
            str._str_len = 0;

            return *this;
        }

        inline string& operator=(const std::string& str)
        {
            return *this = str.c_str();
        }

        inline string& operator=(const char* str)
        {
            _reserve((_str_len = strlen(str)) + 1);
            memcpy(_c_str, str, _str_len);
            return *this;
        }

        operator std::string()const
        {
            return c_str();
        }

        /*string substr(size_t from, size_t count = (size_t)(-1))const
        {
            return  std::string(c_str()).substr(from, count);
        }*/
        size_t size()const
        {
            return _str_len;
        }
        const char* c_str()const
        {
            _c_str[_str_len] = 0;
            return _c_str;
        }
    };

    namespace debug
    {
        template<typename ... ArgTs>
        inline void log(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }
        template<typename ... ArgTs>
        inline void loginfo(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }
        template<typename ... ArgTs>
        inline void logwarn(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }
        template<typename ... ArgTs>
        inline void logerr(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }
        template<typename ... ArgTs>
        inline void logfatal(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_FATAL, format, args...);
        }
    }

    namespace typing
    {
        template<typename T, typename VoidT = void>
        struct sfinae_has_to_string : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_to_string<T, std::void_t<decltype(&T::to_string)>> : std::true_type
        {
        };

        template<typename T, typename VoidT = void>
        struct sfinae_has_parse : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_parse<T, std::void_t<decltype(&T::parse)>> : std::true_type
        {
        };
    }

    namespace basic
    {
        constexpr typing::typehash_t prime = 0x100000001B3ull;
        constexpr typing::typehash_t basis = 0xCBF29CE484222325ull;

        constexpr typing::typehash_t hash_compile_time(char const* str, typing::typehash_t last_value = basis)
        {
            return *str ? hash_compile_time(str + 1, (*str ^ last_value) * prime) : last_value;
        }

        constexpr static size_t allign_size(size_t _origin_sz, size_t _allign_base)
        {
            size_t aligned_sz = (_origin_sz / _allign_base) * _allign_base;
            if (aligned_sz == _origin_sz)
                return aligned_sz;
            return aligned_sz + _allign_base;
        }

        template<typename T, typename  ... ArgTs>
        T* create_new(ArgTs&& ... args)
        {
            static_assert(!std::is_void<T>::value);

            return new(je_mem_alloc(sizeof(T)))T(args...);
        }
        template<typename T, typename  ... ArgTs>
        T* create_new_n(size_t n)
        {
            static_assert(!std::is_void<T>::value);

            return new(je_mem_alloc(sizeof(T) * n))T[n];
        }

        template<typename T>
        void destroy_free(T* address)
        {
            static_assert(!std::is_void<T>::value);

            address->~T();
            je_mem_free(address);
        }
        template<typename T>
        void destroy_free_n(T* address, size_t n)
        {
            static_assert(!std::is_void<T>::value);

            for (size_t i = 0; i < n; i++)
                address[i].~T();

            je_mem_free(address);
        }

        inline char* make_new_string(const char* _str)
        {
            size_t str_length = strlen(_str);
            char* str = (char*)je_mem_alloc(str_length + 1);
            memcpy(str, _str, str_length + 1);

            return str;
        }
        inline char* make_new_string(const std::string& _str)
        {
            return make_new_string(_str.c_str());
        }
        inline char* make_new_string(const jeecs::string& _str)
        {
            return make_new_string(_str.c_str());
        }

        inline std::string make_cpp_string(const char* _str)
        {
            std::string str = _str;
            je_mem_free((void*)_str);
            return str;
        }

        template<typename NodeT>
        struct atomic_list
        {
            std::atomic<NodeT*> last_node = nullptr;

            void add_one(NodeT* node) noexcept
            {
                NodeT* last_last_node = last_node;// .exchange(node);
                do
                {
                    node->last = last_last_node;
                } while (!last_node.compare_exchange_weak(last_last_node, node));
            }

            NodeT* pick_all() noexcept
            {
                NodeT* result = nullptr;
                result = last_node.exchange(nullptr);

                return result;
            }

            NodeT* peek() const noexcept
            {
                return last_node;
            }

            bool empty() const noexcept
            {
                return last_node == nullptr;
            }
        };

        template<typename T>
        struct default_functions
        {
            template<typename U>
            struct has_default_constructor
            {
                template<typename W>
                using _true_type = std::true_type;

                template<typename V>
                static auto _tester(int)->_true_type<decltype(new V())>;
                template<typename V>
                static std::false_type _tester(...);

                constexpr static bool value = decltype(_tester<U>(0))::value;
            };

            static void constructor(void* _ptr, void* arg_ptr)
            {
                if constexpr (has_default_constructor<T>::value)
                    new(_ptr)T;
                else
                    new(_ptr)T(arg_ptr);
            }
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                if constexpr (std::is_copy_constructible<T>::value)
                    new(_ptr)T(*(const T*)_be_copy_ptr);
                else
                    debug::logfatal("This type: '%s' is not copy-constructible but you try to do it."
                        , typeid(T).name());
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                if constexpr (std::is_move_constructible<T>::value)
                    new(_ptr)T(std::move(*(T*)_be_moved_ptr));
                else
                    debug::logfatal("This type: '%s' is not move-constructible but you try to do it."
                        , typeid(T).name());
            }
            static const char* to_string(const void* _ptr)
            {
                if constexpr (typing::sfinae_has_to_string<T>::value)
                    return basic::make_new_string(((const T*)_ptr)->to_string());
                else if constexpr (std::is_fundamental<T>::value)
                {
                    std::stringstream b;
                    std::string str;
                    b << *(const T*)_ptr;
                    b >> str;
                    return basic::make_new_string(str.c_str());
                }
                else if constexpr (std::is_convertible<T, std::string>::value)
                    return basic::make_new_string(*(const T*)_ptr);
                
                static auto call_once = []() {
                    debug::logfatal("This type: '%s' have no function named 'to_string'."
                        , typeid(T).name());
                    return 0;
                }();
                return basic::make_new_string("");
            }
            static void parse(void* _ptr, const char* _memb)
            {
                if constexpr (typing::sfinae_has_parse<T>::value)
                    ((T*)_ptr)->parse(_memb);
                else if constexpr (std::is_fundamental<T>::value)
                {
                    std::stringstream b;
                    b << _memb;
                    b >> *(T*)_ptr;
                }
                else if constexpr (std::is_convertible<const char*, T>::value)
                    *(T*)_ptr = _memb;
                else
                {
                    static auto call_once = []() {
                        debug::logfatal("This type: '%s' have no function named 'parse'."
                            , typeid(T).name());
                        return 0;
                    }();
                }
            }


            template<typename U, typename VoidT = void>
            struct has_update_function : std::false_type
            {
                static_assert(std::is_void<VoidT>::value);
            };
            template<typename U>
            struct has_update_function<U, std::void_t<decltype(&U::Update)>> : std::true_type
            {};

            template<typename U, typename VoidT = void>
            struct has_pre_update_function : std::false_type
            {
                static_assert(std::is_void<VoidT>::value);
            };
            template<typename U>
            struct has_pre_update_function<U, std::void_t<decltype(&U::PreUpdate)>> : std::true_type
            {};

            template<typename U, typename VoidT = void>
            struct has_late_update_function : std::false_type
            {
                static_assert(std::is_void<VoidT>::value);
            };
            template<typename U>
            struct has_late_update_function<U, std::void_t<decltype(&U::LateUpdate)>> : std::true_type
            {};

            static void pre_update(void* _ptr)
            {
                if constexpr (has_pre_update_function<T>::value)
                    reinterpret_cast<T*>(_ptr)->PreUpdate();

            }
            static void update(void* _ptr)
            {
                if constexpr (has_update_function<T>::value)
                    reinterpret_cast<T*>(_ptr)->Update();
            }
            static void late_update(void* _ptr)
            {
                if constexpr (has_late_update_function<T>::value)
                    reinterpret_cast<T*>(_ptr)->LateUpdate();
            }
        };

        template<typename T>
        constexpr auto type_hash()
        {
            return typeid(T).hash_code();
        }

        template<typename ... ArgTs>
        struct type_index_in_varargs
        {
            template<size_t Index, typename AimT, typename CurrentT, typename ... Ts>
            constexpr static size_t _index()
            {
                if constexpr (std::is_same<AimT, CurrentT>::value)
                    return Index;
                else
                    return _index<Index + 1, AimT, Ts...>();
            }

            template<typename AimArgT>
            constexpr size_t index_of() const noexcept
            {
                return _index<0, AimArgT, ArgTs...>();
            }
        };

        template<typename T, typename CounterT = std::atomic_size_t>
        class shared_pointer
        {
            T* ptr = nullptr;
            CounterT* ref_count = nullptr;
            void(*release_func)(T*) = nullptr;

            static_assert(sizeof(CounterT) == sizeof(size_t));

            static void DEFAULT_DESTROY_FUNCTION(T* ptr) { delete ptr; }

        public:
            void clear()
            {
                if (ptr)
                {
                    if (!-- * ref_count)
                    {
                        // Recycle
                        
                            release_func(ptr);

                        ref_count->~CounterT();
                        je_mem_free(ref_count);
                    }
                }
                else
                    assert(ref_count == nullptr);
            }
            ~shared_pointer()
            {
                clear();
            }

            shared_pointer() noexcept = default;
            shared_pointer(T* v, void(*f)(T*) = nullptr) noexcept :
                ptr(v),
                release_func(f ? f : DEFAULT_DESTROY_FUNCTION),
                ref_count(nullptr)
            {
                if (ptr != nullptr)
                    ref_count = new (je_mem_alloc(sizeof(CounterT))) CounterT(1);
            }

            shared_pointer(const shared_pointer& v) noexcept
            {
                ptr = v.ptr;
                release_func = v.release_func;
                if ((ref_count = v.ref_count))
                    ++* ref_count;
            }

            shared_pointer(shared_pointer&& v) noexcept
            {
                ptr = v.ptr;
                release_func = v.release_func;
                ref_count = v.ref_count;
                v.ptr = nullptr;
                v.ref_count = nullptr;
            }

            shared_pointer& operator =(const shared_pointer& v) noexcept
            {
                clear();

                ptr = v.ptr;
                release_func = v.release_func;
                if ((ref_count = v.ref_count))
                    ++* ref_count;

                return *this;
            }

            shared_pointer& operator =(shared_pointer&& v)noexcept
            {
                clear();

                ptr = v.ptr;
                release_func = v.release_func;
                ref_count = v.ref_count;
                v.ptr = nullptr;
                v.ref_count = nullptr;
                return *this;
            }

            T* get() const noexcept
            {
                return ptr;
            }
            operator T& ()const noexcept
            {
                return *ptr;
            }
            T& operator * ()const noexcept
            {
                return *ptr;
            }
            operator T* ()const noexcept
            {
                return ptr;
            }
            operator bool()const noexcept
            {
                return ptr;
            }
            bool operator !()const noexcept
            {
                return !ptr;
            }
            T* operator -> ()const noexcept
            {
                return ptr;
            }

            bool operator ==(const T* pointer)const noexcept
            {
                return ptr == pointer;
            }
            bool operator !=(const T* pointer)const noexcept
            {
                return ptr != pointer;
            }
            bool operator ==(const shared_pointer& pointer)const noexcept
            {
                return ptr == pointer.ptr;
            }
            bool operator !=(const shared_pointer& pointer)const noexcept
            {
                return ptr != pointer.ptr;
            }

            /*  std::string to_string() const
                {
                    if constexpr (Meta::try_get_to_string_of<T>)
                        return ptr->to_string();
                    else
                        return Tool::to_cppstring(Tool::factory::__default_to_string_function<T>(ptr));
                }
                void parse(const char* cstr)
                {
                    if constexpr (Meta::try_get_parse_of<T>)
                        ptr->parse(cstr);
                    else
                        Tool::factory::__default_parse_function<T>(ptr, cstr);
                }*/
        };

        template<typename T>
        using resource = shared_pointer<T>;
    }

    namespace typing
    {
#define JERefRegsiter zzz_jeref_register 

        template<typename T, typename VoidT = void>
        struct sfinae_has_ref_register : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_has_ref_register<T, std::void_t<decltype(&T::JERefRegsiter)>> : std::true_type
        {
        };

        template<typename T, typename VoidT = void>
        struct sfinae_is_static_ref_register_function : std::false_type
        {
            static_assert(std::is_void<VoidT>::value);
        };
        template<typename T>
        struct sfinae_is_static_ref_register_function<T, std::void_t<decltype(T::JERefRegsiter())>> : std::true_type
        {
        };

        struct member_info;

        struct type_info
        {
            typeid_t    m_id;

            typehash_t  m_hash;
            const char* m_typename;   // will be free by je_typing_unregister
            size_t      m_size;
            size_t      m_chunk_size; // calc by je_typing_find_or_register

            construct_func_t    m_constructor;
            destruct_func_t     m_destructor;
            copy_func_t         m_copier;
            move_func_t         m_mover;
            to_string_func_t    m_to_string;
            parse_func_t        m_parse;

            update_func_t       m_pre_update;
            update_func_t       m_update;
            update_func_t       m_late_update;

            je_typing_class     m_type_class;

            const member_info* m_member_types;
        private:
            inline static std::atomic_bool      _m_shutdown_flag = false;

            class _type_unregister_guard
            {
                friend struct type_info;
                _type_unregister_guard() = default;
                std::mutex            _m_self_registed_typeid_mx;
                std::vector<typeid_t> _m_self_registed_typeid;

            public:
                ~_type_unregister_guard()
                {
                    std::lock_guard g1(_m_self_registed_typeid_mx);
                    for (typeid_t typeindex : _m_self_registed_typeid)
                        je_typing_unregister(typeindex);
                    _m_self_registed_typeid.clear();
                    _m_shutdown_flag = true;
                }

                template<typename T>
                typeid_t _register_or_get_type_id(const char* _typename, bool* first_init)
                {
                    bool is_basic_type = false;
                    if (nullptr == _typename)
                    {
                        // If _typename is nullptr, we will not treat it as a component
                        _typename = typeid(T).name();
                        is_basic_type = true;
                    }

                    je_typing_class current_type =
                        is_basic_type
                        ? je_typing_class::JE_BASIC_TYPE
                        : (std::is_base_of<game_system, T>::value
                            ? je_typing_class::JE_SYSTEM
                            : je_typing_class::JE_COMPONENT);

                    typeid_t id = INVALID_TYPE_ID;

                    if (je_typing_find_or_register(
                        &id,
                        _typename,
                        basic::type_hash<T>(),
                        sizeof(T),
                        basic::default_functions<T>::constructor,
                        basic::default_functions<T>::destructor,
                        basic::default_functions<T>::copier,
                        basic::default_functions<T>::mover,
                        basic::default_functions<T>::to_string,
                        basic::default_functions<T>::parse,
                        basic::default_functions<T>::pre_update,
                        basic::default_functions<T>::update,
                        basic::default_functions<T>::late_update,
                        current_type))
                    {
                        *first_init = true;
                        // store to list for unregister
                        std::lock_guard g1(_m_self_registed_typeid_mx);
                        _m_self_registed_typeid.push_back(id);
                    }
                    else
                        *first_init = false;
                    return id;
                }
            };
            inline static _type_unregister_guard _type_guard;

        public:
            static void unregister_all_type_in_shutdown()
            {
                _type_guard.~_type_unregister_guard();
            }
            template<typename T>
            inline static typeid_t id(const char* _typename)
            {
                assert(!_m_shutdown_flag);
                bool first_init = false;
                static typeid_t registed_typeid = _type_guard._register_or_get_type_id<T>(_typename, &first_init);
                if (first_init)
                {
                    if constexpr (sfinae_has_ref_register<T>::value)
                    {
                        if constexpr (sfinae_is_static_ref_register_function<T>::value)
                            T::JERefRegsiter();
                        else
                            static_assert(sfinae_is_static_ref_register_function<T>::value,
                                "T::JERefRegsiter must be static & callable with no arguments.");
                    }
                }
                return registed_typeid;
            }

            template<typename T>
            inline static const type_info* of(const char* _typename)
            {
                assert(!_m_shutdown_flag);
                static typeid_t registed_typeid = id<T>(_typename);
                static const type_info* registed_typeinfo = je_typing_get_info_by_id(registed_typeid);

                return registed_typeinfo;
            }
            inline static const type_info* of(typeid_t _tid)
            {
                assert(!_m_shutdown_flag);
                return je_typing_get_info_by_id(_tid);
            }
            inline static const type_info* of(const char* name)
            {
                assert(!_m_shutdown_flag);
                return je_typing_get_info_by_name(name);
            }

            void construct(void* addr, void* arg = nullptr) const
            {
                m_constructor(addr, arg);
            }
            void destruct(void* addr) const
            {
                m_destructor(addr);
            }
            void copy(void* dst_addr, const void* src_addr) const
            {
                m_copier(dst_addr, src_addr);
            }
            void move(void* dst_addr, void* src_addr) const
            {
                m_mover(dst_addr, src_addr);
            }

            inline bool is_system() const noexcept
            {
                return m_type_class == je_typing_class::JE_SYSTEM;
            }

            inline bool is_component() const noexcept
            {
                return m_type_class == je_typing_class::JE_COMPONENT;
            }

            inline void pre_update(void* addr) const noexcept
            {
                assert(is_system());
                m_pre_update(addr);
            }
            inline void update(void* addr) const noexcept
            {
                assert(is_system());
                m_update(addr);
            }
            inline void late_update(void* addr) const noexcept
            {
                assert(is_system());
                m_late_update(addr);
            }
        };

        struct member_info
        {
            const type_info* m_class_type;

            const char* m_member_name;
            const type_info* m_member_type;
            ptrdiff_t m_member_offset;

            member_info* m_next_member;
        };

        template<typename ClassT, typename MemberT>
        inline void register_member(MemberT(ClassT::* _memboffset), const char* membname)
        {
            const type_info* membt = type_info::of<MemberT>(nullptr);
            assert(membt->m_type_class == je_typing_class::JE_BASIC_TYPE);

            ptrdiff_t member_offset = reinterpret_cast<ptrdiff_t>(&(((ClassT*)nullptr)->*_memboffset));
            je_register_member(
                type_info::id<ClassT>(typeid(ClassT).name()),
                membt,
                membname,
                member_offset);
        }
    }

    class game_universe;

    class game_world
    {
        void* _m_ecs_world_addr;

    public:
        game_world(void* ecs_world_addr)
            :_m_ecs_world_addr(ecs_world_addr)
        {

        }
    private:
        friend class game_system;
    public:
        inline void* handle()const noexcept
        {
            return _m_ecs_world_addr;
        }

        template<typename ... CompTs>
        inline game_entity add_entity()
        {
            typing::typeid_t component_ids[] = {
                typing::type_info::id<CompTs>(typeid(CompTs).name())...,
                typing::INVALID_TYPE_ID
            };
            game_entity gentity;
            je_ecs_world_create_entity_with_components(
                handle(), &gentity, component_ids);

            return gentity;
        }

        inline jeecs::game_system* add_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            return je_ecs_world_add_system_instance(handle(), type);
        }

        template<typename SystemT>
        inline SystemT* add_system()
        {
            return (SystemT*)add_system(typing::type_info::of<SystemT>());
        }

        // ATTENTION: 
        // This function is not thread safe, only useable for in-engine-runtime, do not use it in other thread.
        inline jeecs::game_system* get_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            return je_ecs_world_get_system_instance(handle(), type);
        }

        // ATTENTION: 
        // This function is not thread safe, only useable for in-engine-runtime, do not use it in other thread.
        template<typename SystemT>
        inline SystemT* get_system()
        {
            return (SystemT*)has_system(typing::type_info::of<SystemT>());
        }

        inline void remove_system(const jeecs::typing::type_info* type)
        {
            assert(type->m_type_class == je_typing_class::JE_SYSTEM);
            je_ecs_world_remove_system_instance(handle(), type);
        }

        template<typename SystemT>
        inline void remove_system()
        {
            remove_system(typing::type_info::of<SystemT>());
        }


        // This function only used for editor.
        inline game_entity _add_entity(std::vector<typing::typeid_t> components)
        {
            components.push_back(typing::INVALID_TYPE_ID);

            game_entity gentity;
            je_ecs_world_create_entity_with_components(
                handle(), &gentity, components.data());

            return gentity;
        }

        inline void remove_entity(const game_entity& entity)
        {
            je_ecs_world_destroy_entity(handle(), &entity);
        }

        inline operator bool()const noexcept
        {
            return handle() != nullptr;
        }

        void close() const noexcept
        {
            je_ecs_world_destroy(_m_ecs_world_addr);
        }

        bool is_valid() const noexcept
        {
            return je_ecs_world_is_valid(handle());
        }

        inline game_universe get_universe() const noexcept;
    };


    // Used for select the components of entities which match spcify requirements.
    struct requirement
    {
        enum type : uint8_t
        {
            CONTAIN,        // Must have spcify component
            MAYNOT,         // May have or not have
            ANYOF,          // Must have one of 'ANYOF' components
            EXCEPT,         // Must not contain spcify component
        };

        type m_require;
        size_t m_require_group_id;
        typing::typeid_t m_type;

        requirement(type _require, size_t group_id, typing::typeid_t _type)
            : m_require(_require)
            , m_require_group_id(group_id)
            , m_type(_type)
        { }
    };

    struct dependence
    {
        jeecs::vector<requirement> m_requirements;

        // Store archtypes here?
        game_world  m_world = nullptr;
        bool        m_requirements_inited = false;
        size_t      m_current_arch_version = 0;

        // archs of dependences:
        struct arch_chunks_info
        {
            void* m_arch;
            size_t m_entity_count;

            /* An arch will contain a chain of chunks
            --------------------------------------
            | ArchType
            | chunk5->chunk4->chunk3->chunk2...
            --------------------------------------

            Components data buf will store at chunk' head.
            --------------------------------------
            | Chunk
            | [COMPONENT_BUFFER 64KByte] [OTHER DATAS ..Byte]
            --------------------------------------

            In buffer, components will store like this:
            --------------------------------------
            | Buffer
            | [COMPONENT_1 0 1 2...] [COMPONENT_2 0 1 2...]...
            --------------------------------------

            Each type of components will have a size, and begin-offset in buffer.
            We can use these informations to get all components to walk through.
            */
            size_t m_component_count;
            size_t* m_component_sizes;
            size_t* m_component_offsets;

        };
        jeecs::vector<arch_chunks_info*>       m_archs;

        void clear_archs()noexcept
        {
            je_ecs_clear_dependence_archinfos(this);
        }
        bool need_update(const game_world& aim_world)noexcept
        {
            assert(aim_world.handle() != nullptr);

            size_t arch_updated_ver = je_ecs_world_archmgr_updated_version(aim_world.handle());
            if (m_world != aim_world || m_current_arch_version != arch_updated_ver)
            {
                m_current_arch_version = arch_updated_ver;
                return true;
            }
            return false;
        }
    };

    struct selector
    {
        size_t                      m_curstep = 0;
        size_t                      m_any_id = 0;
        game_world                  m_current_world = nullptr;
        jeecs::vector<dependence>   m_steps;

        game_system* m_system_instance = nullptr;

        selector(game_system* game_sys)
            : m_system_instance(game_sys)
        {
        }

    private:
        template<size_t ArgN, typename FT>
        void _apply_dependence(dependence& dep)
        {
            using f = typing::function_traits<FT>;
            if constexpr (ArgN < f::arity)
            {
                using CurRequireT = typename f::template argument<ArgN>::type;

                if constexpr (ArgN == 0 && std::is_same<CurRequireT, game_entity>::value)
                {
                    // First argument is game_entity, skip this argument
                }
                else if constexpr (std::is_reference<CurRequireT>::value)
                    // Reference, means CONTAIN
                    dep.m_requirements.push_back(
                        requirement(requirement::type::CONTAIN, 0,
                            typing::type_info::id<jeecs::typing::origin_t<CurRequireT>>(typeid(CurRequireT).name())));
                else if constexpr (std::is_pointer<CurRequireT>::value)
                    // Pointer, means MAYNOT
                    dep.m_requirements.push_back(
                        requirement(requirement::type::MAYNOT, 0,
                            typing::type_info::id<jeecs::typing::origin_t<CurRequireT>>(typeid(CurRequireT).name())));
                else
                {
                    static_assert(std::is_void<CurRequireT>::value || !std::is_void<CurRequireT>::value,
                        "'exec' of selector only accept ref or ptr type of Components.");
                }

                _apply_dependence<ArgN + 1, FT>(dep);
            }
        }

        template<typename CurRequireT, typename ... Ts>
        void _apply_except(dependence& dep)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>(typeid(CurRequireT).name());

#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id)
                    debug::logwarn("Repeat or conflict when excepting component '%s'.",
                        typing::type_info::of<CurRequireT>(typeid(CurRequireT).name())->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::EXCEPT, 0, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_except<Ts...>(dep);
        }

        template<typename CurRequireT, typename ... Ts>
        void _apply_contain(dependence& dep)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>(typeid(CurRequireT).name());

#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id)
                    debug::logwarn("Repeat or conflict when containing component '%s'.",
                        typing::type_info::of<CurRequireT>(typeid(CurRequireT).name())->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::CONTAIN, 0, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_contain<Ts...>(dep);
        }

        template<typename CurRequireT, typename ... Ts>
        void _apply_anyof(dependence& dep, size_t any_group)
        {
            static_assert(std::is_same<typing::origin_t<CurRequireT>, CurRequireT>::value);

            auto id = typing::type_info::id<CurRequireT>(typeid(CurRequireT).name());
#ifndef NDEBUG
            for (const requirement& req : dep.m_requirements)
                if (req.m_type == id && req.m_require != requirement::type::MAYNOT)
                    debug::logwarn("Repeat or conflict when require any of component '%s'.",
                        typing::type_info::of<CurRequireT>(typeid(CurRequireT).name())->m_typename);
#endif
            dep.m_requirements.push_back(requirement(requirement::type::ANYOF, any_group, id));
            if constexpr (sizeof...(Ts) > 0)
                _apply_anyof<Ts...>(dep, any_group);
        }

        template<typename FT>
        struct _executor_extracting_agent : std::false_type
        { };

        template<typename RT, typename ... ArgTs>
        struct _executor_extracting_agent<RT(ArgTs...)> : std::true_type
        {
            template<typename ComponentT>
            struct _const_type_index
            {
                using f_t = typing::function_traits<RT(ArgTs...)>;
                template<size_t id = 0>
                static constexpr size_t _index()
                {
                    if constexpr (std::is_same<typename f_t::template argument<id>::type, ComponentT>::value)
                        return id;
                    else
                        return _index<id + 1>();
                }
                static constexpr size_t index = _index();
            };

            template<typename ComponentT>
            inline static ComponentT _get_component(dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id)
            {
                constexpr size_t cid = _const_type_index<ComponentT>::index;

                assert(cid < archinfo->m_component_count);
                size_t offset = archinfo->m_component_offsets[cid] + archinfo->m_component_sizes[cid] * entity_id;

                if (archinfo->m_component_sizes[cid])
                {
                    if constexpr (std::is_reference<ComponentT>::value)
                        return *reinterpret_cast<typename typing::origin_t<ComponentT>*>(reinterpret_cast<intptr_t>(chunkbuf) + offset);
                    else
                    {
                        static_assert(std::is_pointer<ComponentT>::value);
                        return reinterpret_cast<typename typing::origin_t<ComponentT>*>(reinterpret_cast<intptr_t>(chunkbuf) + offset);
                    }
                }
                if constexpr (std::is_reference<ComponentT>::value)
                {
                    assert(("Only maynot/anyof canbe here. 'je_ecs_world_update_dependences_archinfo' may have some problem.", false));
                    return *(typename typing::origin_t<ComponentT>*)nullptr;
                }
                else
                    return nullptr; // Only maynot/anyof can be here, no need to cast the type;
            }

            inline static bool get_entity_avaliable(const void* entity_meta, size_t eid)noexcept
            {
                static const size_t meta_size = je_arch_entity_meta_size();
                static const size_t meta_entity_stat_offset = je_arch_entity_meta_state_offset();

                uint8_t* _addr = ((uint8_t*)entity_meta) + eid * meta_size + meta_entity_stat_offset;
                return jeecs::game_entity::entity_stat::READY == *(const jeecs::game_entity::entity_stat*)_addr;
            }

            template<typename FT>
            inline static void exec(dependence* depend, FT&& f, game_system* sys) noexcept
            {
                for (auto* archinfo : depend->m_archs)
                {
                    auto cur_chunk = je_arch_get_chunk(archinfo->m_arch);
                    while (cur_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                        for (size_t eid = 0; eid < archinfo->m_entity_count; ++eid)
                        {
                            if (get_entity_avaliable(entity_meta_addr, eid))
                            {
                                if constexpr (std::is_void<typename typing::function_traits<FT>::this_t>::value)
                                    f(_get_component<ArgTs>(archinfo, cur_chunk, eid)...);
                                else
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)(_get_component<ArgTs>(archinfo, cur_chunk, eid)...);
                            }
                        }

                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template<typename RT, typename ... ArgTs>
        struct _executor_extracting_agent<RT(game_entity, ArgTs...)> : std::true_type
        {
            template<typename ComponentT>
            struct _const_type_index
            {
                using f_t = typing::function_traits<RT(ArgTs...)>;
                template<size_t id = 0>
                static constexpr size_t _index()
                {
                    if constexpr (std::is_same<typename f_t::template argument<id>::type, ComponentT>::value)
                        return id;
                    else
                        return _index<id + 1>();
                }
                static constexpr size_t index = _index();
            };

            template<typename ComponentT>
            inline static ComponentT _get_component(dependence::arch_chunks_info* archinfo, void* chunkbuf, size_t entity_id)
            {
                constexpr size_t cid = _const_type_index<ComponentT>::index;

                assert(cid < archinfo->m_component_count);
                size_t offset = archinfo->m_component_offsets[cid] + archinfo->m_component_sizes[cid] * entity_id;

                if (archinfo->m_component_sizes[cid])
                {
                    if constexpr (std::is_reference<ComponentT>::value)
                        return *reinterpret_cast<typename typing::origin_t<ComponentT>*>(reinterpret_cast<intptr_t>(chunkbuf) + offset);
                    else
                    {
                        static_assert(std::is_pointer<ComponentT>::value);
                        return reinterpret_cast<typename typing::origin_t<ComponentT>*>(reinterpret_cast<intptr_t>(chunkbuf) + offset);
                    }
                }
                if constexpr (std::is_reference<ComponentT>::value)
                {
                    assert(("Only maynot/anyof canbe here. 'je_ecs_world_update_dependences_archinfo' may have some problem.", false));
                    return *(typename typing::origin_t<ComponentT>*)nullptr;
                }
                else
                    return nullptr; // Only maynot/anyof can be here, no need to cast the type;
            }

            inline static typing::version_t get_entity_avaliable(const void* entity_meta, size_t eid, typing::version_t* out_version)noexcept
            {
                static const size_t meta_size = je_arch_entity_meta_size();
                static const size_t meta_entity_stat_offset = je_arch_entity_meta_state_offset();
                static const size_t meta_entity_vers_offset = je_arch_entity_meta_version_offset();

                uint8_t* _addr = ((uint8_t*)entity_meta) + eid * meta_size + meta_entity_stat_offset;
                if (jeecs::game_entity::entity_stat::READY == *(const jeecs::game_entity::entity_stat*)_addr)
                {
                    *out_version = *(typing::version_t*)(((uint8_t*)entity_meta) + eid * meta_size + meta_entity_vers_offset);
                    return true;
                }
                return false;
            }

            template<typename FT>
            inline static void exec(dependence* depend, FT&& f, game_system* sys) noexcept
            {
                for (auto* archinfo : depend->m_archs)
                {
                    auto cur_chunk = je_arch_get_chunk(archinfo->m_arch);
                    while (cur_chunk)
                    {
                        auto entity_meta_addr = je_arch_entity_meta_addr_in_chunk(cur_chunk);
                        typing::version_t version;
                        for (size_t eid = 0; eid < archinfo->m_entity_count; ++eid)
                        {
                            if (get_entity_avaliable(entity_meta_addr, eid, &version))
                            {
                                if constexpr (std::is_void<typename typing::function_traits<FT>::this_t>::value)
                                    f(game_entity()._set_arch_chunk_info(cur_chunk, eid, version), _get_component<ArgTs>(archinfo, cur_chunk, eid)...);
                                else
                                    (static_cast<typename typing::function_traits<FT>::this_t*>(sys)->*f)(game_entity()._set_arch_chunk_info(cur_chunk, eid, version), _get_component<ArgTs>(archinfo, cur_chunk, eid)...);
                            }
                        }

                        cur_chunk = je_arch_next_chunk(cur_chunk);
                    }
                }
            }
        };

        template<typename FT>
        bool _update(FT&& exec)
        {
            if (!m_current_world)
            {
                jeecs::debug::logwarn("Failed to execute current jobs(%p). Game world not specify!");
                return false;
            }

            // Let last step finished!
            if (m_curstep)
                m_steps[m_curstep - 1].m_requirements_inited = true;

            assert(m_curstep <= m_steps.size());
            if (m_curstep == m_steps.size())
            {
                // First times to execute this job or arch/world changed, register requirements
                m_steps.push_back(dependence());
                dependence& dep = m_steps.back();

                assert(dep.m_requirements.size() == 0 && dep.m_requirements_inited == false);
                _apply_dependence<0, FT>(dep);
            }

            dependence& cur_dependence = m_steps[m_curstep];
            // NOTE: Only execute when current dependence has been inited.
            // Because some requirement might append after exec(...)
            if (cur_dependence.m_requirements_inited)
            {
                if (cur_dependence.need_update(m_current_world))
                {
                    cur_dependence.m_world = m_current_world;
                    je_ecs_world_update_dependences_archinfo(m_current_world.handle(), &cur_dependence);
                }

                // OK! Execute step function!
                static_assert(
                    _executor_extracting_agent<typename typing::function_traits<FT>::flat_func_t>::value,
                    "Fail to extract types of arguments from 'FT'.");
                _executor_extracting_agent<typename typing::function_traits<FT>::flat_func_t>::exec(
                    &cur_dependence, exec, m_system_instance);
            }
            return true;
        }
    public:
        selector()
        {
            jeecs::debug::loginfo("New selector(%p) created.", this);
        }
        ~selector()
        {
            for (auto& step : m_steps)
                step.clear_archs();
            m_steps.clear();

            jeecs::debug::loginfo("Selector(%p) closed.", this);
        }

        selector& at(game_world w)
        {
            if (!m_steps.empty())
            {
                assert(m_curstep == m_steps.size());
                m_steps[m_curstep - 1].m_requirements_inited = true;
            }
            m_curstep = 0;
            m_current_world = w;
            return *this;
        }

        template<typename FT>
        selector& exec(FT&& _exec)
        {
            if (_update(_exec))
                ++m_curstep;

            return *this;
        }

        template<typename ... Ts>
        inline selector& except() noexcept
        {
            assert(m_curstep > 0);
            const size_t last_step = m_curstep - 1;

            auto& depend = m_steps[last_step];

            if (!depend.m_requirements_inited)
            {
                _apply_except<Ts...>(depend);
            }
            return *this;
        }
        template<typename ... Ts>
        inline selector& contain() noexcept
        {
            assert(m_curstep > 0);
            const size_t last_step = m_curstep - 1;

            auto& depend = m_steps[last_step];

            if (!depend.m_requirements_inited)
            {
                _apply_contain<Ts...>(depend);
            }
            return *this;
        }
        template<typename ... Ts>
        inline selector& anyof() noexcept
        {
            assert(m_curstep > 0);
            const size_t last_step = m_curstep - 1;

            auto& depend = m_steps[last_step];

            if (!depend.m_requirements_inited)
            {
                _apply_anyof<Ts...>(depend, m_any_id);
                ++m_any_id;
            }
            return *this;
        }
    };

    class game_system
    {
        JECS_DISABLE_MOVE_AND_COPY(game_system);
    private:
        double _m_delta_time;

        game_world _m_game_world;
        selector   _m_default_selector;

    public:
        game_system(game_world world, double delta_tm = 1. / 60.)
            : _m_game_world(world)
            , _m_delta_time(delta_tm)
            , _m_default_selector(this)
        { }

        // Get binded world or attached world
        game_world get_world() const noexcept
        {
            assert(_m_game_world);
            return _m_game_world;
        }

        // Select from default selector
        // ATTENTION: Not thread safe!
        inline selector& select_from(game_world w) noexcept
        {
            return _m_default_selector.at(w);
        }
        inline selector& select() noexcept
        {
            return _m_default_selector;
        }

        inline float delta_time() const noexcept
        {
            return (float)_m_delta_time;
        }

#define PreUpdate PreUpdate
#define Update Update
#define LateUpdate LateUpdate

        // void PreUpdate()
        // void Update()
        // void LateUpdate()
        /*
        struct TranslationUpdater : game_system
        {
            void LateUpdate()
            {
                select()
                    .exec(...);
                    .exec(...);
            }
        }
        */
    };

    class game_universe
    {
        void* _m_universe_addr;
    public:

        game_universe(void* universe_addr)
            :_m_universe_addr(universe_addr)
        {

        }

        inline void* handle()const noexcept
        {
            return _m_universe_addr;
        }

        game_world create_world()
        {
            return je_ecs_world_create(_m_universe_addr);
        }

        inline void wait()const noexcept
        {
            je_ecs_universe_loop(handle());
        }

        inline void stop() const noexcept
        {
            je_ecs_universe_stop(handle());
        }

        inline operator bool() const noexcept
        {
            return _m_universe_addr;
        }

    public:
        static game_universe create_universe()
        {
            return game_universe(je_ecs_universe_create());
        }
        static void destroy_universe(game_universe universe)
        {
            return je_ecs_universe_destroy(universe.handle());
        }
    };

    inline game_universe game_world::get_universe() const noexcept
    {
        return game_universe(je_ecs_world_in_universe(handle()));
    }

    namespace editor
    {
        static std::string dump_entity_editor_information(const jeecs::game_entity& e)
        {
#ifdef JE_ENABLE_DEBUG_API
            if (e.valid())
            {
                // Get all components.
                const typing::type_info** component_types = jedbg_get_all_components_from_entity(&e);

                std::string result = "{";
                bool first_component = true;
                for (auto** cur_component = component_types; *cur_component; ++cur_component)
                {
                    if (first_component)
                    {
                        first_component = false;
                        result += ",";
                    }

                    const typing::type_info* type = *cur_component;
                    result += std::string("\"") + type->m_typename + "\":{\"";

                    result += basic::make_cpp_string(type->m_member_types->m_member_name) + "\":\"";
                    // TODO: Store all member datas.

                    result += "}";
                }
                result += "}";
                je_mem_free(component_types);

                return result;
            }
            else
                debug::logerr("Current entity is invalid when dumping editor informations.");
#else
            debug::logerr("If you want to dump entity's editor information, you must #define JE_ENABLE_DEBUG_API.");
#endif
            return "";
        }
    }

    template<typename T>
    inline T* game_entity::get_component()const noexcept
    {
        auto* type = typing::type_info::of<T>(typeid(T).name());
        assert(type->is_component());
        return (T*)je_ecs_world_entity_get_component(this, type);
    }

    template<typename T>
    inline T* game_entity::add_component()const noexcept
    {
        auto* type = typing::type_info::of<T>(typeid(T).name());
        assert(type->is_component());
        return (T*)je_ecs_world_entity_add_component(je_ecs_world_of_entity(this), this, type);
    }

    template<typename T>
    inline void game_entity::remove_component() const noexcept
    {
        auto* type = typing::type_info::of<T>(typeid(T).name());
        assert(type->is_component());
        return je_ecs_world_entity_remove_component(je_ecs_world_of_entity(this), this, type);
    }

    inline bool game_entity::valid() const noexcept
    {
        return je_ecs_world_validate_entity(this);
    }

    inline jeecs::game_world game_entity::game_world() const noexcept
    {
        return jeecs::game_world(je_ecs_world_of_entity(this));
    }

    inline void game_entity::close() const noexcept
    {
        game_world().remove_entity(*this);
    }

    namespace math
    {
        constexpr static float RAD2DEG = 57.29578f;

        template<typename T>
        static T clamp(T src, T min, T max)
        {
            return (src < min) ? (
                min
                ) :
                (
                    (src > max) ?
                    (max) :
                    (src)
                    );
        }
        template<typename T>
        inline T lerp(T va, T vb, float deg)
        {
            return va * (1.0f - deg) + vb * deg;
        }

        template<typename T>
        inline T random(T from, T to)
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());

            if constexpr (std::is_floating_point<T>::value)
            {
                std::uniform_real_distribution<> dis(from, to);
                return (T)dis(gen);
            }
            else
            {
                std::uniform_int_distribution<> dis(from, to);
                return (T)dis(gen);
            }

        }

        inline void mat4xmat4(float* out_result, const float* left, const float* right)
        {
#define R(x, y) (out_result[(x) + (y)* 4])
#define A(x, y) (left[(x) + (y)* 4])
#define B(x, y) (right[(x) + (y)* 4])
            memset(out_result, 0, 16 * sizeof(float));
            for (size_t iy = 0; iy < 4; iy++)
                for (size_t ix = 0; ix < 4; ix++)
                {
                    R(ix, iy)
                        += A(ix, 0) * B(0, iy)
                        + A(ix, 1) * B(1, iy)
                        + A(ix, 2) * B(2, iy)
                        + A(ix, 3) * B(3, iy);
                }

#undef R
#undef A
#undef B
        }
        inline void mat4xmat4(float(*out_result)[4], const float(*left)[4], const float(*right)[4])
        {
            return mat4xmat4((float*)out_result, (const float*)left, (const float*)right);
        }

        inline void mat4xvec4(float* out_result, const float* left_mat, const float* right_vex)
        {
#define R(x) (out_result[x])
#define A(x, y) (left_mat[(x) + (y)* 4])
#define B(x) (right_vex[x])
            R(0) = A(0, 0) * B(0) + A(0, 1) * B(1) + A(0, 2) * B(2) + A(0, 3) * B(3);
            R(1) = A(1, 0) * B(0) + A(1, 1) * B(1) + A(1, 2) * B(2) + A(1, 3) * B(3);
            R(2) = A(2, 0) * B(0) + A(2, 1) * B(1) + A(2, 2) * B(2) + A(2, 3) * B(3);
            R(3) = A(3, 0) * B(0) + A(3, 1) * B(1) + A(3, 2) * B(2) + A(3, 3) * B(3);
#undef R
#undef A
#undef B
        }
        inline void mat4xvec4(float* out_result, const float(*left)[4], const float* right)
        {
            return mat4xvec4(out_result, (const float*)left, right);
        }

        inline void mat4xvec3(float* out_result, const float* left, const float* right)
        {
            float dat[4] = { right[0], right[1], right[2], 1.f };
            float result[4];
            mat4xvec4(result, left, dat);
            if (result[3] != 0.f)
            {
                out_result[0] = result[0] / result[3];
                out_result[1] = result[1] / result[3];
                out_result[2] = result[2] / result[3];
            }
            else
            {
                out_result[0] = result[0];
                out_result[1] = result[1];
                out_result[2] = result[2];
            }
        }
        inline void mat4xvec3(float* out_result, const float(*left)[4], const float* right)
        {
            mat4xvec3(out_result, (const float*)left, right);
        }

        struct _basevec2
        {
            float x, y;
            constexpr _basevec2(float _x, float _y) noexcept :x(_x), y(_y) {}
        };
        struct _basevec3
        {
            float x, y, z;
            constexpr _basevec3(float _x, float _y, float _z) noexcept :x(_x), y(_y), z(_z) {}
        };
        struct _basevec4
        {
            float x, y, z, w;
            constexpr _basevec4(float _x, float _y, float _z, float _w) noexcept :x(_x), y(_y), z(_z), w(_w) {}
        };

        struct vec2 : public _basevec2
        {
            constexpr vec2(float _x = 0.f, float _y = 0.f) noexcept
                :_basevec2(_x, _y) {}
            constexpr vec2(const vec2& _v2)noexcept
                :_basevec2(_v2.x, _v2.y) {}
            constexpr vec2(vec2&& _v2)noexcept
                :_basevec2(_v2.x, _v2.y) {}

            constexpr vec2(const _basevec3& _v3)noexcept
                :_basevec2(_v3.x, _v3.y) {}
            constexpr vec2(_basevec3&& _v3)noexcept
                :_basevec2(_v3.x, _v3.y) {}
            constexpr vec2(const _basevec4& _v4)noexcept
                :_basevec2(_v4.x, _v4.y) {}
            constexpr vec2(_basevec4&& _v4)noexcept
                :_basevec2(_v4.x, _v4.y) {}

            // + - * / with another vec2
            inline constexpr vec2 operator + (const vec2& _v2) const noexcept
            {
                return vec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr vec2 operator - (const vec2& _v2) const noexcept
            {
                return vec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr vec2 operator - () const noexcept
            {
                return vec2(-x, -y);
            }
            inline constexpr vec2 operator + () const noexcept
            {
                return vec2(x, y);
            }
            inline constexpr vec2 operator * (const vec2& _v2) const noexcept
            {
                return vec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr vec2 operator / (const vec2& _v2) const noexcept
            {
                return vec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr vec2 operator * (float _f) const noexcept
            {
                return vec2(x * _f, y * _f);
            }
            inline constexpr vec2 operator / (float _f) const noexcept
            {
                return vec2(x / _f, y / _f);
            }

            inline constexpr vec2& operator = (const vec2& _v2) noexcept = default;
            inline constexpr vec2& operator += (const vec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr vec2& operator -= (const vec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator *= (const vec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                return *this;
            }
            inline constexpr vec2& operator /= (const vec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr vec2& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const vec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator != (const vec2& _v2) const noexcept
            {
                return x != _v2.x || y != _v2.y;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return sqrt(x * x + y * y);
            }
            inline vec2 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec2(x / vlength, y / vlength);
                return vec2();
            }
            inline constexpr float dot(const vec2& _v2) const noexcept
            {
                return x * _v2.x + y * _v2.y;
            }
            /*inline constexpr vec2 cross(const vec2& _v2) const noexcept
            {

            }*/

            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f)", &x, &y);
            }
        };
        inline static constexpr vec2 operator * (float _f, const vec2& _v2) noexcept
        {
            return vec2(_v2.x * _f, _v2.y * _f);
        }

        struct ivec2
        {
            int x;
            int y;
            constexpr ivec2(int _x = 0.f, int _y = 0.f) noexcept
                : x(_x)
                , y(_y)
            {

            }
            constexpr ivec2(const vec2& _v2)noexcept
                : x(_v2.x)
                , y(_v2.y)
            {

            }

            constexpr ivec2(vec2&& _v2)noexcept
                : x(_v2.x)
                , y(_v2.y)
            {

            }

            // + - * / with another vec2
            inline constexpr ivec2 operator + (const ivec2& _v2) const noexcept
            {
                return ivec2(x + _v2.x, y + _v2.y);
            }
            inline constexpr ivec2 operator - (const ivec2& _v2) const noexcept
            {
                return ivec2(x - _v2.x, y - _v2.y);
            }
            inline constexpr ivec2 operator - () const noexcept
            {
                return ivec2(-x, -y);
            }
            inline constexpr ivec2 operator + () const noexcept
            {
                return ivec2(x, y);
            }
            inline constexpr ivec2 operator * (const ivec2& _v2) const noexcept
            {
                return ivec2(x * _v2.x, y * _v2.y);
            }
            inline constexpr ivec2 operator / (const ivec2& _v2) const noexcept
            {
                return ivec2(x / _v2.x, y / _v2.y);
            }
            // * / with float
            inline constexpr ivec2 operator * (int _f) const noexcept
            {
                return ivec2(x * _f, y * _f);
            }
            inline constexpr ivec2 operator / (int _f) const noexcept
            {
                return vec2(x / _f, y / _f);
            }

            inline constexpr ivec2& operator = (const ivec2& _v2) noexcept = default;
            inline constexpr ivec2& operator += (const ivec2& _v2) noexcept
            {
                x += _v2.x;
                y += _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator -= (const ivec2& _v2) noexcept
            {
                x -= _v2.x;
                y -= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator *= (const ivec2& _v2) noexcept
            {
                x *= _v2.x;
                y *= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                return *this;
            }
            inline constexpr ivec2& operator /= (const ivec2& _v2) noexcept
            {
                x /= _v2.x;
                y /= _v2.y;
                return *this;
            }
            inline constexpr ivec2& operator /= (int _f) noexcept
            {
                x /= _f;
                y /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const ivec2& _v2) const noexcept
            {
                return x == _v2.x && y == _v2.y;
            }
            inline constexpr bool operator != (const ivec2& _v2) const noexcept
            {
                return x != _v2.x || y != _v2.y;
            }
        };

        struct vec3 :public _basevec3
        {
            constexpr vec3(float _x = 0.f, float _y = 0.f, float _z = 0.f)noexcept
                :_basevec3(_x, _y, _z) {}
            constexpr vec3(const vec3& _v3)noexcept
                :_basevec3(_v3.x, _v3.y, _v3.z) {}
            constexpr vec3(vec3&& _v3)noexcept
                :_basevec3(_v3.x, _v3.y, _v3.z) {}

            constexpr vec3(const _basevec2& _v2)noexcept
                :_basevec3(_v2.x, _v2.y, 0.f) {}
            constexpr vec3(_basevec2&& _v2)noexcept
                :_basevec3(_v2.x, _v2.y, 0.f) {}
            constexpr vec3(const _basevec4& _v4)noexcept
                :_basevec3(_v4.x, _v4.y, _v4.z) {}
            constexpr vec3(_basevec4&& _v4)noexcept
                :_basevec3(_v4.x, _v4.y, _v4.z) {}

            // + - * / with another vec3
            inline constexpr vec3 operator + (const vec3& _v3) const noexcept
            {
                return vec3(x + _v3.x, y + _v3.y, z + _v3.z);
            }
            inline constexpr vec3 operator - (const vec3& _v3) const noexcept
            {
                return vec3(x - _v3.x, y - _v3.y, z - _v3.z);
            }
            inline constexpr vec3 operator - () const noexcept
            {
                return vec3(-x, -y, -z);
            }
            inline constexpr vec3 operator + () const noexcept
            {
                return vec3(x, y, z);
            }
            inline constexpr vec3 operator * (const vec3& _v3) const noexcept
            {
                return vec3(x * _v3.x, y * _v3.y, z * _v3.z);
            }
            inline constexpr vec3 operator / (const vec3& _v3) const noexcept
            {
                return vec3(x / _v3.x, y / _v3.y, z / _v3.z);
            }
            // * / with float
            inline constexpr vec3 operator * (float _f) const noexcept
            {
                return vec3(x * _f, y * _f, z * _f);
            }
            inline constexpr vec3 operator / (float _f) const noexcept
            {
                return vec3(x / _f, y / _f, z / _f);
            }

            inline constexpr vec3& operator = (const vec3& _v3) noexcept = default;
            inline constexpr vec3& operator += (const vec3& _v3) noexcept
            {
                x += _v3.x;
                y += _v3.y;
                z += _v3.z;
                return *this;
            }
            inline constexpr vec3& operator -= (const vec3& _v3) noexcept
            {
                x -= _v3.x;
                y -= _v3.y;
                z -= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator *= (const vec3& _v3) noexcept
            {
                x *= _v3.x;
                y *= _v3.y;
                z *= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                return *this;
            }
            inline constexpr vec3& operator /= (const vec3& _v3) noexcept
            {
                x /= _v3.x;
                y /= _v3.y;
                z /= _v3.z;
                return *this;
            }
            inline constexpr vec3& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                return *this;
            }
            // == !=
            inline constexpr bool operator == (const vec3& _v3) const noexcept
            {
                return x == _v3.x && y == _v3.y && z == _v3.z;
            }
            inline constexpr bool operator != (const vec3& _v3) const noexcept
            {
                return x != _v3.x || y != _v3.y || z != _v3.z;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return std::sqrt(x * x + y * y + z * z);
            }
            inline vec3 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec3(x / vlength, y / vlength, z / vlength);
                return vec3();
            }
            inline constexpr float dot(const vec3& _v3) const noexcept
            {
                return x * _v3.x + y * _v3.y + z * _v3.z;
            }
            inline constexpr vec3 cross(const vec3& _v3) const noexcept
            {
                return vec3(
                    y * _v3.z - z * _v3.y,
                    z * _v3.x - x * _v3.z,
                    x * _v3.y - y * _v3.x);
            }

            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f)", &x, &y, &z);
            }
        };
        inline static constexpr vec3 operator * (float _f, const vec3& _v3) noexcept
        {
            return vec3(_v3.x * _f, _v3.y * _f, _v3.z * _f);
        }

        struct vec4 :public _basevec4
        {
            constexpr vec4(float _x = 0.f, float _y = 0.f, float _z = 0.f, float _w = 0.f)noexcept
                :_basevec4(_x, _y, _z, _w) {}
            constexpr vec4(const vec4& _v4)noexcept
                :_basevec4(_v4.x, _v4.y, _v4.z, _v4.w) {}
            constexpr vec4(vec4&& _v4)noexcept
                :_basevec4(_v4.x, _v4.y, _v4.z, _v4.w) {}

            constexpr vec4(const _basevec2& _v2)noexcept
                :_basevec4(_v2.x, _v2.y, 0.f, 0.f) {}
            constexpr vec4(_basevec2&& _v2)noexcept
                :_basevec4(_v2.x, _v2.y, 0.f, 0.f) {}
            constexpr vec4(const _basevec3& _v3)noexcept
                :_basevec4(_v3.x, _v3.y, _v3.z, 0.f) {}
            constexpr vec4(_basevec3&& _v3)noexcept
                :_basevec4(_v3.x, _v3.y, _v3.z, 0.f) {}

            // + - * / with another vec4
            inline constexpr vec4 operator + (const vec4& _v4) const noexcept
            {
                return vec4(x + _v4.x, y + _v4.y, z + _v4.z, w + _v4.w);
            }
            inline constexpr vec4 operator - (const vec4& _v4) const noexcept
            {
                return vec4(x - _v4.x, y - _v4.y, z - _v4.z, w - _v4.w);
            }
            inline constexpr vec4 operator - () const noexcept
            {
                return vec4(-x, -y, -z, -w);
            }
            inline constexpr vec4 operator + () const noexcept
            {
                return vec4(x, y, z, w);
            }
            inline constexpr vec4 operator * (const vec4& _v4) const noexcept
            {
                return vec4(x * _v4.x, y * _v4.y, z * _v4.z, w * _v4.w);
            }
            inline constexpr vec4 operator / (const vec4& _v4) const noexcept
            {
                return vec4(x / _v4.x, y / _v4.y, z / _v4.z, w / _v4.w);
            }
            // * / with float
            inline constexpr vec4 operator * (float _f) const noexcept
            {
                return vec4(x * _f, y * _f, z * _f, w * _f);
            }
            inline constexpr vec4 operator / (float _f) const noexcept
            {
                return vec4(x / _f, y / _f, z / _f, w / _f);
            }

            inline constexpr vec4& operator = (const vec4& _v4) noexcept = default;
            inline constexpr vec4& operator += (const vec4& _v4) noexcept
            {
                x += _v4.x;
                y += _v4.y;
                z += _v4.z;
                w += _v4.w;
                return *this;
            }
            inline constexpr vec4& operator -= (const vec4& _v4) noexcept
            {
                x -= _v4.x;
                y -= _v4.y;
                z -= _v4.z;
                w -= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator *= (const vec4& _v4) noexcept
            {
                x *= _v4.x;
                y *= _v4.y;
                z *= _v4.z;
                w *= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator *= (float _f) noexcept
            {
                x *= _f;
                y *= _f;
                z *= _f;
                w *= _f;
                return *this;
            }
            inline constexpr vec4& operator /= (const vec4& _v4) noexcept
            {
                x /= _v4.x;
                y /= _v4.y;
                z /= _v4.z;
                w /= _v4.w;
                return *this;
            }
            inline constexpr vec4& operator /= (float _f) noexcept
            {
                x /= _f;
                y /= _f;
                z /= _f;
                w /= _f;
                return *this;
            }

            // == !=
            inline constexpr bool operator == (const vec4& _v4) const noexcept
            {
                return x == _v4.x && y == _v4.y && z == _v4.z && w == _v4.w;
            }
            inline constexpr bool operator != (const vec4& _v4) const noexcept
            {
                return x != _v4.x || y != _v4.y || z != _v4.z || w != _v4.w;
            }

            // length/ unit/ dot/ cross
            inline float length() const noexcept
            {
                return sqrt(x * x + y * y + z * z + w * w);
            }
            inline vec4 unit() const noexcept
            {
                float vlength = length();
                if (vlength != 0.f)
                    return vec4(x / vlength, y / vlength, z / vlength, w / vlength);
                return vec4();
            }
            inline constexpr float dot(const vec4& _v4) const noexcept
            {
                return x * _v4.x + y * _v4.y + z * _v4.z + w * _v4.w;
            }
            /*inline constexpr vec4 cross(const vec4& _v4) const noexcept
            {
            }*/
            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << "," << w << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f,%f)", &x, &y, &z, &w);
            }
        };
        inline static constexpr vec4 operator * (float _f, const vec4& _v4) noexcept
        {
            return vec4(_v4.x * _f, _v4.y * _f, _v4.z * _f, _v4.w * _f);
        }

        struct quat
        {
            float x, y, z, w;

            inline constexpr bool operator == (const quat& q) const noexcept
            {
                return x == q.x && y == q.y && z == q.z && w == q.w;
            }
            inline constexpr bool operator != (const quat& q) const noexcept
            {
                return x != q.x || y != q.y || z != q.z || w != q.w;
            }

            inline quat(float _x, float _y, float _z, float _w) noexcept
            {
                auto len = sqrt(_x * _x + _y * _y + _z * _z + _w * _w);
                x = _x / len;
                y = _y / len;
                z = _z / len;
                w = _w / len;
            }

            constexpr quat() noexcept
                : x(0.f)
                , y(0.f)
                , z(0.f)
                , w(1.f) { }

            quat(float yaw, float pitch, float roll) noexcept
            {
                set_euler_angle(yaw, pitch, roll);
            }

            inline void create_matrix(float* pMatrix) const noexcept
            {
                if (!pMatrix) return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = w;

                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }
            inline void create_inv_matrix(float* pMatrix) const noexcept
            {

                if (!pMatrix) return;
                float m_x = x;
                float m_y = y;
                float m_z = z;
                float m_w = -w;

                pMatrix[0] = 1.0f - 2.0f * (m_y * m_y + m_z * m_z);
                pMatrix[1] = 2.0f * (m_x * m_y + m_z * m_w);
                pMatrix[2] = 2.0f * (m_x * m_z - m_y * m_w);
                pMatrix[3] = 0.0f;

                pMatrix[4] = 2.0f * (m_x * m_y - m_z * m_w);
                pMatrix[5] = 1.0f - 2.0f * (m_x * m_x + m_z * m_z);
                pMatrix[6] = 2.0f * (m_z * m_y + m_x * m_w);
                pMatrix[7] = 0.0f;

                pMatrix[8] = 2.0f * (m_x * m_z + m_y * m_w);
                pMatrix[9] = 2.0f * (m_y * m_z - m_x * m_w);
                pMatrix[10] = 1.0f - 2.0f * (m_x * m_x + m_y * m_y);
                pMatrix[11] = 0.0f;

                pMatrix[12] = 0;
                pMatrix[13] = 0;
                pMatrix[14] = 0;
                pMatrix[15] = 1.0f;
            }

            inline void create_matrix(float(*pMatrix)[4]) const
            {
                create_matrix((float*)pMatrix);
            }
            inline void create_inv_matrix(float(*pMatrix)[4]) const
            {
                create_inv_matrix((float*)pMatrix);
            }

            inline constexpr float dot(const quat& _quat) const noexcept
            {
                return x * _quat.x + y * _quat.y + z * _quat.z + w * _quat.w;
            }
            inline void set_euler_angle(const vec3& euler) noexcept
            {
                set_euler_angle(euler.x, euler.y, euler.z);
            }
            inline void set_euler_angle(float yaw, float pitch, float roll) noexcept
            {
                yaw = yaw / RAD2DEG;
                pitch = pitch / RAD2DEG;
                roll = roll / RAD2DEG;

                float angle;
                float sinRoll, sinPitch, sinYaw, cosRoll, cosPitch, cosYaw;

                angle = yaw * 0.5f;
                sinYaw = sin(angle);
                cosYaw = cos(angle);
                angle = pitch * 0.5f;
                sinPitch = sin(angle);
                cosPitch = cos(angle);
                angle = roll * 0.5f;
                sinRoll = sin(angle);
                cosRoll = cos(angle);

                float _y = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
                float _x = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;
                float _z = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
                float _w = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

                float mag = _x * _x + _y * _y + _z * _z + _w * _w;

                x = _x / mag;
                y = _y / mag;
                z = _z / mag;
                w = _w / mag;
            }
            inline constexpr void set(float _x, float _y, float _z, float _w) noexcept
            {
                x = _x;
                y = _y;
                z = _z;
                w = _w;
            }

            static inline quat euler(float x, float y, float z) noexcept
            {
                quat na;
                na.set_euler_angle(x, y, z);
                return na;
            }
            static inline quat euler(const vec3& v3) noexcept
            {
                quat na;
                na.set_euler_angle(v3.x, v3.y, v3.z);
                return na;
            }
            static inline quat axis_angle(const vec3& a, float arg) noexcept
            {
                auto sv = sin(arg / 2.0f / RAD2DEG);
                auto cv = cos(arg / 2.0f / RAD2DEG);
                return quat(a.x * sv, a.y * sv, a.z * sv, cv);
            }

            static inline quat lerp(const quat& a, const quat& b, float t)
            {
                return quat((1 - t) * a.x + t * b.x,
                    (1 - t) * a.y + t * b.y,
                    (1 - t) * a.z + t * b.z,
                    (1 - t) * a.w + t * b.w);
            }
            static inline quat slerp(const quat& a, const quat& b, float t)
            {
                float cos_theta = a.dot(b);

                // if B is on opposite hemisphere from A, use -B instead
                float sign;
                if (cos_theta < 0.f)
                {
                    cos_theta = -cos_theta;
                    sign = -1.f;
                }
                else sign = 1.f;
                float c1, c2;
                if (cos_theta > 1.f - FLT_EPSILON)
                {
                    // if q2 is (within precision limits) the same as q1,
                    // just linear interpolate between A and B.
                    c2 = t;
                    c1 = 1.f - t;
                }
                else
                {
                    // float theta = gFloat::ArcCosTable(cos_theta);
                    // faster than table-based :
                    //const float theta = myacos(cos_theta);
                    float theta = acos(cos_theta);
                    float sin_theta = sin(theta);
                    float t_theta = t * theta;
                    float inv_sin_theta = 1.f / sin_theta;
                    c2 = sin(t_theta) * inv_sin_theta;
                    c1 = sin(theta - t_theta) * inv_sin_theta;
                }
                c2 *= sign; // or c1 *= sign
                            // just affects the overrall sign of the output
                            // interpolate
                return quat(a.x * c1 + b.x * c2, a.y * c1 + b.y * c2, a.z * c1 + b.z * c2, a.w * c1 + b.w * c2);
            }
            static inline float delta_angle(const quat& lhs, const quat& rhs)
            {
                float cos_theta = lhs.dot(rhs);
                // if B is on opposite hemisphere from A, use -B instead
                if (cos_theta < 0.f)
                {
                    cos_theta = -cos_theta;
                }
                float theta = acos(cos_theta);
                return 2 * RAD2DEG * theta;
            }

            static inline quat rotation(const vec3& a, const vec3& b)noexcept
            {
                auto axis = b.cross(a);
                auto angle = RAD2DEG * acos(b.dot(a) / (b.length() * a.length()));

                return quat::axis_angle(axis.unit(), angle);
            }

            inline quat conjugate() const noexcept
            {
                return quat(-x, -y, -z, w);
            }
            inline quat inverse() const noexcept
            {
                return quat(-x, -y, -z, w);
            }
            inline vec3 euler_angle() const noexcept
            {
                float yaw = atan2(2 * (w * x + z * y), 1 - 2 * (x * x + y * y));
                float pitch = asin(clamp(2 * (w * y - x * z), -1.0f, 1.0f));
                float roll = atan2(2 * (w * z + x * y), 1 - 2 * (z * z + y * y));
                return vec3(RAD2DEG * yaw, RAD2DEG * pitch, RAD2DEG * roll);
            }

            inline quat operator * (const quat& _quat) const noexcept
            {
                float w1 = w;
                float w2 = _quat.w;

                vec3 v1(x, y, z);
                vec3 v2(_quat.x, _quat.y, _quat.z);
                float w3 = w1 * w2 - v1.dot(v2);
                vec3 v3 = v1.cross(v2) + w1 * v2 + w2 * v1;
                return quat(v3.x, v3.y, v3.z, w3);
            }
            inline constexpr vec3 operator *(const vec3& _v3) const noexcept
            {
                vec3 u(x, y, z);
                return 2.0f * u.dot(_v3) * u
                    + (w * w - u.dot(u)) * _v3
                    + 2.0f * w * u.cross(_v3);
            }
            inline std::string to_string()const
            {
                std::string result;
                std::stringstream ss;
                ss << "(" << x << "," << y << "," << z << "," << w << ")";
                ss >> result;

                return result;
            }
            inline void parse(const std::string& str)
            {
                sscanf(str.c_str(), "(%f,%f,%f,%f)", &x, &y, &z, &w);
            }
        };

        inline math::vec3 mat4trans(const float* left_mat, const math::vec3& v3)
        {
            float v33[3] = { v3.x, v3.y, v3.z };
            float result[3];
            mat4xvec3(result, left_mat, v33);

            return math::vec3(result[0], result[1], result[2]);
        }
        inline math::vec3 mat4trans(const float(*left_mat)[4], const math::vec3& v3)
        {
            float v33[3] = { v3.x, v3.y, v3.z };
            float result[3];
            mat4xvec3(result, left_mat, v33);

            return math::vec3(result[0], result[1], result[2]);
        }
        inline math::vec4 mat4trans(const float* left_mat, const math::vec4& v4)
        {
            float v44[4] = { v4.x, v4.y, v4.z, v4.w };
            float result[4];
            mat4xvec4(result, left_mat, v44);

            return math::vec4(result[0], result[1], result[2], result[3]);
        }
        inline math::vec4 mat4trans(const float(*left_mat)[4], const math::vec4& v4)
        {
            float v44[4] = { v4.x, v4.y, v4.z, v4.w };
            float result[4];
            mat4xvec4(result, left_mat, v44);

            return math::vec4(result[0], result[1], result[2], result[3]);
        }
    }

    namespace graphic
    {
        class resource_basic
        {
            JECS_DISABLE_MOVE_AND_COPY(resource_basic);

            jegl_resource* _m_resouce;
        protected:
            resource_basic(jegl_resource* res) noexcept
                :_m_resouce(res)
            {

            }
        public:
            inline operator bool() const noexcept
            {
                return enabled();
            }
            inline bool enabled() const noexcept
            {
                return nullptr != _m_resouce
                    && nullptr != _m_resouce->m_custom_resource;
            }
            inline operator jegl_resource* () const noexcept
            {
                return _m_resouce;
            }
            inline jegl_resource* resouce() const noexcept
            {
                return _m_resouce;
            }
            ~resource_basic()
            {
                if (_m_resouce)
                    jegl_close_resource(_m_resouce);
            }
        };

        class texture : public resource_basic
        {
        public:
            explicit texture(const std::string& str)
                : resource_basic(jegl_load_texture(str.c_str()))
            {
            }
            explicit texture(const texture& res)
                : resource_basic(jegl_copy_resource(res.resouce()))
            {
            }
            explicit texture(size_t width, size_t height, jegl_texture::texture_format format)
                : resource_basic(jegl_create_texture(width, height, format))
            {
            }

            class pixel
            {
                jegl_texture* _m_texture;
                jegl_texture::pixel_data_t* _m_pixel;
            public:
                pixel(jegl_resource* _texture, size_t x, size_t y) noexcept
                    : _m_texture(_texture->m_raw_texture_data)
                {
                    assert(_texture->m_type == jegl_resource::type::TEXTURE);
                    assert(sizeof(jegl_texture::pixel_data_t) == 1);

                    _m_pixel =
                        _m_texture->m_pixels
                        + y * _m_texture->m_width * _m_texture->m_format
                        + x * _m_texture->m_format;
                }
                inline math::vec4 get() const noexcept
                {
                    switch (_m_texture->m_format)
                    {
                    case jegl_texture::texture_format::MONO:
                        return math::vec4{ _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f, _m_pixel[0] / 255.0f };
                    case jegl_texture::texture_format::RGB:
                        return math::vec4{ _m_pixel[0] / 255.0f, _m_pixel[1] / 255.0f, _m_pixel[2] / 255.0f, 1.0f };
                    case jegl_texture::texture_format::RGBA:
                        return math::vec4{ _m_pixel[0] / 255.0f, _m_pixel[1] / 255.0f, _m_pixel[2] / 255.0f, _m_pixel[3] / 255.0f };
                    default:
                        assert(0); return {};
                    }
                }
                inline void set(const math::vec4& value) const noexcept
                {
                    _m_texture->m_modified = true;
                    switch (_m_texture->m_format)
                    {
                    case jegl_texture::texture_format::MONO:
                        _m_pixel[0] = math::clamp((jegl_texture::pixel_data_t)round(value.x * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        break;
                    case jegl_texture::texture_format::RGB:
                        _m_pixel[0] = math::clamp((jegl_texture::pixel_data_t)round(value.x * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[1] = math::clamp((jegl_texture::pixel_data_t)round(value.y * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[2] = math::clamp((jegl_texture::pixel_data_t)round(value.z * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        break;
                    case jegl_texture::texture_format::RGBA:
                        _m_pixel[0] = math::clamp((jegl_texture::pixel_data_t)round(value.x * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[1] = math::clamp((jegl_texture::pixel_data_t)round(value.y * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[2] = math::clamp((jegl_texture::pixel_data_t)round(value.z * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        _m_pixel[3] = math::clamp((jegl_texture::pixel_data_t)round(value.w * 255.0f), (jegl_texture::pixel_data_t)0, (jegl_texture::pixel_data_t)255);
                        break;
                    default:
                        assert(0); break;
                    }
                }

                inline operator math::vec4()const noexcept
                {
                    return get();
                }
                inline pixel& operator = (const math::vec4& col) noexcept
                {
                    set(col);
                    return *this;
                }
            };

            // pixel's x/y is from LEFT-BUTTOM to RIGHT/TOP
            pixel pix(size_t x, size_t y) const noexcept
            {
                assert(enabled());
                return pixel(*this, x, y);
            }
            inline size_t height() const noexcept
            {
                if (enabled())
                {
                    assert(resouce()->m_raw_texture_data != nullptr);
                    return resouce()->m_raw_texture_data->m_height;
                }
                return 0;
            }
            inline size_t width() const noexcept
            {
                if (enabled())
                {
                    assert(resouce()->m_raw_texture_data != nullptr);
                    return resouce()->m_raw_texture_data->m_width;
                }
                return 0;
            }
            inline math::vec2 size() const noexcept
            {
                if (enabled())
                {
                    assert(resouce()->m_raw_texture_data != nullptr);
                    return math::vec2(resouce()->m_raw_texture_data->m_width, resouce()->m_raw_texture_data->m_height);
                }
                return math::vec2(0.f, 0.f);
            }
        };

        class shader : public resource_basic
        {
        public:
            jegl_shader::builtin_uniform_location* m_builtin;

            explicit shader(const std::string& name_path, const std::string& src)
                : resource_basic(jegl_load_shader_source(name_path.c_str(), src.c_str(), true))
                , m_builtin(nullptr)
            {
                if (enabled())
                    m_builtin = &((jegl_resource*)(*this))->m_raw_shader_data->m_builtin_uniforms;
            }
            explicit shader(const shader& res)
                : resource_basic(jegl_copy_resource(res.resouce()))
                , m_builtin(nullptr)
            {
                if (enabled())
                    m_builtin = &((jegl_resource*)(*this))->m_raw_shader_data->m_builtin_uniforms;
            }
            explicit shader(const std::string& src_path)
                : resource_basic(jegl_load_shader(src_path.c_str()))
                , m_builtin(nullptr)
            {
                if (enabled())
                    m_builtin = &((jegl_resource*)(*this))->m_raw_shader_data->m_builtin_uniforms;
            }

            void set_uniform(const std::string& name, int val)noexcept
            {
                if (enabled())
                {
                    auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                    while (jegl_shad_uniforms)
                    {
                        if (jegl_shad_uniforms->m_name == name)
                        {
                            if (jegl_shad_uniforms->m_uniform_type !=
                                jegl_shader::uniform_type::INT)
                                debug::logerr("Trying set uniform('%s' = %d) to shader(%p), but current uniform type is not 'INT'."
                                    , name.c_str(), val, this);
                            else
                            {
                                jegl_shad_uniforms->n = val;
                                jegl_shad_uniforms->m_updated = true;
                            }
                            return;
                        }
                        jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                    }
                }
                else
                    debug::logerr("Trying set uniform('%s' = %d) to invalid shader(%p), please check."
                        , name.c_str(), val, this);
            }
            void set_uniform(const std::string& name, float val)noexcept
            {
                if (enabled())
                {
                    auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                    while (jegl_shad_uniforms)
                    {
                        if (jegl_shad_uniforms->m_name == name)
                        {
                            if (jegl_shad_uniforms->m_uniform_type !=
                                jegl_shader::uniform_type::FLOAT)
                                debug::logerr("Trying set uniform('%s' = %f) to shader(%p), but current uniform type is not 'FLOAT'."
                                    , name.c_str(), val, this);
                            else
                            {
                                jegl_shad_uniforms->x = val;
                                jegl_shad_uniforms->m_updated = true;
                            }
                            return;
                        }
                        jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                    }
                }
                else
                    debug::logerr("Trying set uniform('%s' = %f) to invalid shader(%p), please check."
                        , name.c_str(), val, this);
            }
            void set_uniform(const std::string& name, const math::vec2& val)noexcept
            {
                if (enabled())
                {
                    auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                    while (jegl_shad_uniforms)
                    {
                        if (jegl_shad_uniforms->m_name == name)
                        {
                            if (jegl_shad_uniforms->m_uniform_type !=
                                jegl_shader::uniform_type::FLOAT2)
                                debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT2'."
                                    , name.c_str(), val.to_string().c_str(), this);
                            else
                            {
                                jegl_shad_uniforms->x = val.x;
                                jegl_shad_uniforms->y = val.y;
                                jegl_shad_uniforms->m_updated = true;
                            }
                            return;
                        }
                        jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                    }
                }
                else
                    debug::logerr("Trying set uniform('%s' = %s) to invalid shader(%p), please check."
                        , name.c_str(), val.to_string().c_str(), this);
            }
            void set_uniform(const std::string& name, const math::vec3& val)noexcept
            {
                if (enabled())
                {
                    auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                    while (jegl_shad_uniforms)
                    {
                        if (jegl_shad_uniforms->m_name == name)
                        {
                            if (jegl_shad_uniforms->m_uniform_type !=
                                jegl_shader::uniform_type::FLOAT3)
                                debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT3'."
                                    , name.c_str(), val.to_string().c_str(), this);
                            else
                            {
                                jegl_shad_uniforms->x = val.x;
                                jegl_shad_uniforms->y = val.y;
                                jegl_shad_uniforms->z = val.z;
                                jegl_shad_uniforms->m_updated = true;
                            }
                            return;
                        }
                        jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                    }
                }
                else
                    debug::logerr("Trying set uniform('%s' = %s) to invalid shader(%p), please check."
                        , name.c_str(), val.to_string().c_str(), this);
            }
            void set_uniform(const std::string& name, const math::vec4& val)noexcept
            {
                if (enabled())
                {
                    auto* jegl_shad_uniforms = resouce()->m_raw_shader_data->m_custom_uniforms;
                    while (jegl_shad_uniforms)
                    {
                        if (jegl_shad_uniforms->m_name == name)
                        {
                            if (jegl_shad_uniforms->m_uniform_type !=
                                jegl_shader::uniform_type::FLOAT4)
                                debug::logerr("Trying set uniform('%s' = %s) to shader(%p), but current uniform type is not 'FLOAT4'."
                                    , name.c_str(), val.to_string().c_str(), this);
                            else
                            {
                                jegl_shad_uniforms->x = val.x;
                                jegl_shad_uniforms->y = val.y;
                                jegl_shad_uniforms->z = val.z;
                                jegl_shad_uniforms->w = val.w;
                                jegl_shad_uniforms->m_updated = true;
                            }
                            return;
                        }
                        jegl_shad_uniforms = jegl_shad_uniforms->m_next;
                    }
                }
                else
                    debug::logerr("Trying set uniform('%s' = %s) to invalid shader(%p), please check."
                        , name.c_str(), val.to_string().c_str(), this);
            }
        };

        class vertex : public resource_basic
        {
        public:
            using type = jegl_vertex::vertex_type;

            explicit vertex(const std::string& str)
                : resource_basic(jegl_load_vertex(str.c_str()))
            {
            }
            explicit vertex(const vertex& res)
                : resource_basic(jegl_copy_resource(res.resouce()))
            {
            }
            explicit vertex(type vertex_type, const std::vector<float>& pdatas, const std::vector<size_t>& formats)
                : resource_basic(jegl_create_vertex(vertex_type, pdatas.data(), formats.data(), pdatas.size(), formats.size()))
            {
            }
        };

        class framebuffer : public resource_basic
        {
        public:
            explicit framebuffer(size_t reso_w, size_t reso_h, const std::vector<jegl_texture::texture_format>& attachment)
                :resource_basic(jegl_create_framebuf(reso_w, reso_h, attachment.data(), attachment.size()))
            {
            }

            basic::resource<texture> get_attachment(size_t index) const
            {
                if (enabled())
                {
                    if (index < resouce()->m_raw_framebuf_data->m_attachment_count)
                    {
                        auto* attachments = (basic::resource<graphic::texture>*)resouce()->m_raw_framebuf_data->m_output_attachments;
                        return attachments[index];
                    }
                    return nullptr;
                }
                return nullptr;
            }
        };

        class uniformbuffer : public resource_basic
        {
        public:
            explicit uniformbuffer(size_t binding_place, size_t buffersize)
                :resource_basic(jegl_create_uniformbuf(binding_place, buffersize))
            {
            }

            void update_buffer(size_t offset, size_t size, const void* datafrom) const noexcept
            {
                if (enabled())
                    jegl_update_uniformbuf(resouce(), datafrom, offset, size);
            }
        };

        inline void ortho_projection(
            float(*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float RATIO = 1024.0f;
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * RATIO / 2.0f / scale / 100.0f;
            const float L = -R;
            const float T = RATIO / 2.0f / scale / 100.0f;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = 2.0f / (R - L);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 2.0f / (T - B);
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 2.0f / (zfar - znear);
            m[2][3] = 0;

            m[3][0] = -((R + L) / (R - L));
            m[3][1] = -((T + B) / (T - B));
            m[3][2] = -((zfar + znear) / (zfar - znear));
            m[3][3] = 1;
        }

        inline void ortho_inv_projection(
            float(*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float scale,
            float znear,
            float zfar)
        {
            const float RATIO = 1024.0f;
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;

            const float R = WIDTH_HEIGHT_RATIO * RATIO / 2.0f / scale / 100.0f;
            const float L = -R;
            const float T = RATIO / 2.0f / scale / 100.0f;
            const float B = -T;

            auto m = out_proj_mat;
            m[0][0] = (R - L) / 2.0f;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = (T - B) / 2.0f;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = (zfar - znear) / 2.0f;
            m[2][3] = 0;

            m[3][0] = (R + L) / 2.0f;
            m[3][1] = (T + B) / 2.0f;
            m[3][2] = (zfar + znear) / 2.0f;
            m[3][3] = 1;
        }

        inline void perspective_projection(
            float(*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);

            auto m = out_proj_mat;
            m[0][0] = 1.0f / (TAN_HALF_FOV * WIDTH_HEIGHT_RATIO);
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = 1.0f / TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = (-znear - zfar) / ZRANGE;
            m[2][3] = 1.0f;

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 2.0f * zfar * znear / ZRANGE;
            m[3][3] = 0;
        }

        inline void perspective_inv_projection(
            float(*out_proj_mat)[4],
            float windows_width,
            float windows_height,
            float angle,
            float znear,
            float zfar)
        {
            const float WIDTH_HEIGHT_RATIO = windows_width / windows_height;
            const float ZRANGE = znear - zfar;
            const float TAN_HALF_FOV = tanf(angle / math::RAD2DEG / 2.0f);


            auto m = out_proj_mat;
            m[0][0] = TAN_HALF_FOV * WIDTH_HEIGHT_RATIO;
            m[0][1] = 0;
            m[0][2] = 0;
            m[0][3] = 0;

            m[1][0] = 0;
            m[1][1] = TAN_HALF_FOV;
            m[1][2] = 0;
            m[1][3] = 0;

            m[2][0] = 0;
            m[2][1] = 0;
            m[2][2] = 0;
            m[2][3] = ZRANGE / (2 * zfar * znear);

            m[3][0] = 0;
            m[3][1] = 0;
            m[3][2] = 1;
            m[3][3] = (zfar + znear) / (2 * zfar * znear);
        }

        struct character
        {
            basic::resource<texture> m_texture;
            wchar_t                  m_char = 0;
            int                      m_width = 0;
            int                      m_height = 0;
            int                      m_adv_x = 0;
            int                      m_adv_y = 0;
            int                      m_delta_x = 0;
            int                      m_delta_y = 0;
        };

        class font
        {
            JECS_DISABLE_MOVE_AND_COPY(font);

            je_font* m_font;
            size_t          m_size;
            jeecs::string   m_path;
        public:
            font(const std::string& fontfile, size_t size)noexcept
                : m_size(size)
                , m_path(fontfile)
            {
                m_font = je_font_load(fontfile.c_str(), (float)size, (float)size);
            }
            ~font()
            {
                je_font_free(m_font);
            }
            character* get_character(unsigned int wcharacter) const noexcept
            {
                return je_font_get_char(m_font, wcharacter);
            }
            inline bool enabled() const noexcept
            {
                return nullptr != m_font;
            }

            basic::resource<texture> text_texture(const std::wstring& text, float one_line_size = 10000.0f, int max_line_count = INT_MAX)
            {
                return text_texture(*this, text, one_line_size, max_line_count);
            }
            basic::resource<texture> text_texture(const std::string& text, float one_line_size = 10000.0f, int max_line_count = INT_MAX)
            {
                return text_texture(*this, wo_str_to_wstr(text.c_str()), one_line_size, max_line_count);
            }
        private:
            inline static basic::resource<texture> text_texture(
                font& font_base,
                const std::wstring& text,
                float max_line_character_size,
                int max_line_count) noexcept
            {
                std::map<std::string, std::map<int, basic::resource<font>>> FONT_POOL;
                math::vec4 TEXT_COLOR = { 1,1,1,1 };
                float TEXT_SCALE = 1.0f;
                math::vec2 TEXT_OFFSET = { 0,0 };

                //计算文字所需要纹理的大小
                int next_ch_x = 0;
                int next_ch_y = 0;
                int line_count = 0;

                int min_px = INT_MAX, min_py = INT_MAX, max_px = INT_MIN, max_py = INT_MIN;

                font* used_font = &font_base;

                for (size_t ti = 0; ti < text.size(); ti++)
                {
                    bool IgnoreEscapeSign = false;
                next_ch_sign:
                    wchar_t ch = text[ti];
                    auto gcs = used_font->get_character((unsigned int)ch);

                    if (gcs)
                    {
                        if (ch == L'\\' && ti + 1 < text.size() && !IgnoreEscapeSign)
                        {
                            ti++;
                            IgnoreEscapeSign = true;
                            goto next_ch_sign;
                        }
                        else if (ch == L'{' && !IgnoreEscapeSign)
                        {
                            std::string item_name;
                            std::string value;

                            for (size_t ei = ti + 1; ei < text.size(); ei++)
                            {
                                if (text[ei] == L':')
                                {
                                    item_name = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;
                                }
                                else if (text[ei] == L'}')
                                {
                                    value = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                    if (item_name == "scale")
                                    {
                                        TEXT_SCALE = std::stof(value);
                                        if (TEXT_SCALE == 1.0f)
                                            used_font = &font_base;
                                        else
                                        {
                                            auto& new_font = FONT_POOL[font_base.m_path][int(TEXT_SCALE * font_base.m_size + 0.5f)/*四舍五入*/];
                                            if (new_font == nullptr)
                                                new_font = new font(font_base.m_path, int(TEXT_SCALE * font_base.m_size + 0.5f));

                                            used_font = new_font.get();
                                        }
                                    }
                                    else if (item_name == "offset")
                                    {
                                        math::vec2 offset;
                                        offset.parse(value);
                                        TEXT_OFFSET += offset;
                                    }

                                    break;
                                }
                            }
                        }
                        else if (ch != L'\n')
                        {
                            int px_min = next_ch_x + 0 + gcs->m_delta_x + int(TEXT_OFFSET.x * font_base.m_size);
                            int py_min = -next_ch_y + 0 + gcs->m_delta_y - gcs->m_adv_y + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * font_base.m_size);

                            int px_max = next_ch_x + gcs->m_texture->width() - 1 + gcs->m_delta_x + int(TEXT_OFFSET.x * font_base.m_size);
                            int py_max = -next_ch_y + gcs->m_texture->height() - 1 + gcs->m_delta_y - gcs->m_adv_y + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * font_base.m_size);

                            min_px = min_px > px_min ? px_min : min_px;
                            min_py = min_py > py_min ? py_min : min_py;

                            max_px = max_px < px_max ? px_max : max_px;
                            max_py = max_py < py_max ? py_max : max_py;

                            next_ch_x += gcs->m_adv_x;
                        }

                        if (ch == L'\n' || next_ch_x >= int(max_line_character_size * font_base.m_size))
                        {
                            next_ch_x = 0;
                            next_ch_y -= gcs->m_adv_y;
                            line_count++;
                        }
                        if (line_count >= max_line_count)
                            break;
                    }
                }
                int size_x = max_px - min_px;
                int size_y = max_py - min_py;

                int correct_x = -min_px;
                int correct_y = -min_py;

                next_ch_x = 0;
                next_ch_y = 0;
                line_count = 0;
                TEXT_SCALE = 1.0f;
                TEXT_OFFSET = { 0,0 };
                used_font = &font_base;

                auto new_texture = new texture(size_x + 1, size_y + 1, jegl_texture::texture_format::RGBA);
                std::memset(new_texture->resouce()->m_raw_texture_data->m_pixels, 0, new_texture->width() * new_texture->size().y * 4);

                for (size_t ti = 0; ti < text.size(); ti++)
                {
                    bool IgnoreEscapeSign = false;
                next_ch_sign_display:
                    wchar_t ch = text[ti];
                    auto gcs = used_font->get_character((unsigned int)ch);

                    if (gcs)
                    {
                        if (ch == L'\\' && ti + 1 < text.size() && !IgnoreEscapeSign)
                        {
                            ti++;
                            IgnoreEscapeSign = true;
                            goto next_ch_sign_display;
                        }
                        else if (ch == L'{' && !IgnoreEscapeSign)
                        {
                            std::string item_name;
                            std::string value;

                            for (size_t ei = ti + 1; ei < text.size(); ei++)
                            {
                                if (text[ei] == L':')
                                {
                                    item_name = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                }
                                else if (text[ei] == L'}')
                                {
                                    value = wo_wstr_to_str(text.substr(ti + 1, ei - ti - 1).c_str());
                                    ti = ei;

                                    if (item_name == "color")
                                    {
                                        char color[9] = "00000000";
                                        for (size_t i = 0; i < 8 && i < value.size(); i++)
                                            color[i] = (char)value[i];

                                        unsigned int colordata = strtoul(color, NULL, 16);

                                        unsigned char As = (*(unsigned char*)(&colordata));
                                        unsigned char Bs = (*(((unsigned char*)(&colordata)) + 1));
                                        unsigned char Gs = (*(((unsigned char*)(&colordata)) + 2));
                                        unsigned char Rs = (*(((unsigned char*)(&colordata)) + 3));

                                        TEXT_COLOR = { Rs / 255.0f,Gs / 255.0f ,Bs / 255.0f ,As / 255.0f };
                                    }
                                    else if (item_name == "scale")
                                    {
                                        TEXT_SCALE = std::stof(value);
                                        if (TEXT_SCALE == 1.0f)
                                            used_font = &font_base;
                                        else
                                        {
                                            auto& new_font = FONT_POOL[font_base.m_path][int(TEXT_SCALE * font_base.m_size + 0.5f)/*四舍五入*/];
                                            assert(new_font != nullptr);
                                            used_font = new_font.get();
                                        }
                                    }
                                    else if (item_name == "offset")
                                    {
                                        math::vec2 offset;
                                        offset.parse(value);
                                        TEXT_OFFSET = TEXT_OFFSET + offset;
                                    }
                                    break;
                                }
                            }
                        }
                        else if (ch != L'\n')
                        {
                            struct p_index
                            {
                                size_t id;
                                p_index operator ++()
                                {
                                    return { ++id };
                                }
                                p_index operator ++(int)
                                {
                                    return { id++ };
                                }
                                bool operator ==(const p_index& pindex)const
                                {
                                    return id == pindex.id;
                                }
                                bool operator !=(const p_index& pindex)const
                                {
                                    return id != pindex.id;
                                }
                                size_t& operator *()
                                {
                                    return id;
                                }
                                ptrdiff_t operator-(const p_index& another)const
                                {
                                    return ptrdiff_t(id - another.id);
                                }

                                typedef std::forward_iterator_tag iterator_category;
                                typedef size_t value_type;
                                typedef ptrdiff_t difference_type;
                                typedef const size_t* pointer;
                                typedef const size_t& reference;
                            };
                            std::for_each(
#ifdef __cpp_lib_execution
                                std::execution::par_unseq,
#endif
                                p_index{ 0 }, p_index{ size_t(gcs->m_texture->size().y) },
                                [&](size_t fy) {
                                    std::for_each(
#ifdef __cpp_lib_execution
                                        std::execution::par_unseq,
#endif
                                        p_index{ 0 }, p_index{ size_t(gcs->m_texture->width()) },
                                        [&](size_t fx) {

                                            auto pdst = new_texture->pix(
                                                correct_x + next_ch_x + int(fx) + gcs->m_delta_x + int(TEXT_OFFSET.x * font_base.m_size),
                                                correct_y - next_ch_y + int(fy) + gcs->m_delta_y - gcs->m_adv_y + int((TEXT_OFFSET.y + TEXT_SCALE - 1.0f) * font_base.m_size)
                                            );

                                            auto psrc = gcs->m_texture->pix(int(fx), int(fy)).get();

                                            float src_alpha = psrc.w * TEXT_COLOR.w;

                                            pdst.set(
                                                math::vec4(
                                                    src_alpha * psrc.x * TEXT_COLOR.x + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().x : 1.0f),
                                                    src_alpha * psrc.y * TEXT_COLOR.y + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().y : 1.0f),
                                                    src_alpha * psrc.z * TEXT_COLOR.z + (1.0f - src_alpha) * (pdst.get().w ? pdst.get().z : 1.0f),
                                                    src_alpha * psrc.w * TEXT_COLOR.w + (1.0f - src_alpha) * pdst.get().w
                                                )
                                            );
                                        }
                                    ); // end of  for each
                                }
                            );
                            next_ch_x += gcs->m_adv_x;
                        }
                        if (ch == L'\n' || next_ch_x >= int(max_line_character_size * font_base.m_size))
                        {
                            next_ch_x = 0;
                            next_ch_y -= gcs->m_adv_y;
                            line_count++;
                        }
                        if (line_count >= max_line_count)
                            break;
                    }
                }
                return new_texture;
            }

        };
    }

    namespace Transform
    {
        // An entity without childs and parent will contain these components:
        // LocalPosition/LocalRotation/LocalScale and using LocalToWorld to apply
        // local transform to Translation
        // If an entity have childs, it will have ChildAnchor 
        // If an entity have parent, it will have LocalToParent and without
        // LocalToWorld.
        /*
                                                    Parent's Translation--\
                                                         /(apply)          \
                                                        /                   \
                                 LocalToParent-----LocalToParent             >Translation (2x  4x4 matrix)
                                                 yes  /                     /
        LocalPosition------------>(HAVE_PARENT?)-----/                     /
                       /    /              \ no                           /
        LocalScale----/    /                ---------> LocalToWorld------/
                          /
        LocalRotation----/                                (LocalToXXX) Still using 2x 3 vector + 1x quat

        */

        struct Translation
        {
            float object2world[4][4] = { };

            math::vec3 world_position = { 0,0,0 };
            math::quat world_rotation;
            math::vec3 local_scale = { 1,1,1 };

            inline void set_position(const math::vec3& _v3) noexcept
            {
                world_position = _v3;
            }
            inline void set_scale(const math::vec3& _v3) noexcept
            {
                local_scale = _v3;
            }
            inline void set_rotation(const math::quat& _quat)noexcept
            {
                world_rotation = _quat;
            }
        };

        struct LocalRotation
        {
            math::quat rot;
            inline math::quat get_parent_world_rotation(const Translation& translation)const noexcept
            {
                return translation.world_rotation * rot.inverse();
            }
            inline void set_world_rotation(const math::quat& _rot, const Translation& translation) noexcept
            {
                auto x = get_parent_world_rotation(translation).inverse();
                rot = _rot * get_parent_world_rotation(translation).inverse();
            }

            static void JERefRegsiter()
            {
                typing::register_member(&LocalRotation::rot, "rot");
            }
        };

        struct LocalPosition
        {
            math::vec3 pos;
            inline math::vec3 get_parent_world_position(const Translation& translation, const LocalRotation* rotation) const noexcept
            {
                if (rotation)
                    return translation.world_position - rotation->get_parent_world_rotation(translation) * pos;
                else
                    return translation.world_position - translation.world_rotation * pos;
            }

            void set_world_position(const math::vec3& _pos, const Translation& translation, const LocalRotation* rotation) noexcept
            {
                if (rotation)
                    pos = rotation->get_parent_world_rotation(translation).inverse() * (_pos - get_parent_world_position(translation, rotation));
                else
                    pos = translation.world_rotation.inverse() * (_pos - get_parent_world_position(translation, nullptr));
            }

            static void JERefRegsiter()
            {
                typing::register_member(&LocalPosition::pos, "pos");
            }
        };

        struct LocalScale
        {
            math::vec3 scale = { 1.0f, 1.0f, 1.0f };

            static void JERefRegsiter()
            {
                typing::register_member(&LocalScale::scale, "scale");
            }
        };

        struct ChildAnchor
        {
            typing::uid_t anchor_uid = je_uid_generate();

            static void JERefRegsiter()
            {
                typing::register_member(&ChildAnchor::anchor_uid, "anchor_uid");
            }
        };

        struct LocalToParent
        {
            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;

            typing::uid_t parent_uid = {};

            static void JERefRegsiter()
            {
                typing::register_member(&LocalToParent::parent_uid, "parent_uid");
            }
        };

        struct LocalToWorld
        {
            math::vec3 pos;
            math::vec3 scale;
            math::quat rot;
        };
        static_assert(offsetof(LocalToParent, pos) == offsetof(LocalToWorld, pos));
        static_assert(offsetof(LocalToParent, scale) == offsetof(LocalToWorld, scale));
        static_assert(offsetof(LocalToParent, rot) == offsetof(LocalToWorld, rot));
    }
    namespace Renderer
    {
        // An entity need to be rendered, must have Transform::Translation and 
        // Renderer, 
        /*

        (Viewport)
        OrthoProjection/PerspectiveProjection
        Projection
        Clip  ----------\
                         >------> Cameras
                        /          / | \
        Translation ---<          /  |  \ (for each..)
                        \        /   |   \
                         >--  The entities need
        Material--------/         be rend..
                       /
        (Shape)-------/

        */
        struct Rendqueue
        {
            int rend_queue = 0;

            static void JERefRegsiter()
            {
                typing::register_member(&Rendqueue::rend_queue, "rend_queue");
            }
        };

        struct Shape
        {
            basic::resource<graphic::vertex> vertex;
        };

        struct Shaders
        {
            // NOTE: shaders should not be nullptr!
            jeecs::vector<basic::resource<graphic::shader>> shaders;

            template<typename T>
            void set_uniform(const std::string& name, const T& val) noexcept
            {
                for (auto& shad : shaders)
                    shad->set_uniform(name, val);
            }
        };

        struct Textures
        {
            // NOTE: textures should not be nullptr!
            struct texture_with_passid
            {
                size_t m_pass_id;
                basic::resource<graphic::texture> m_texture;

                texture_with_passid(size_t pass, const basic::resource<graphic::texture>& tex)
                    : m_pass_id(pass)
                    , m_texture(tex)
                {

                }
            };
            math::vec2 tiling = math::vec2(1.f, 1.f);
            math::vec2 offset = math::vec2(0.f, 0.f);
            jeecs::vector<texture_with_passid> textures;

            void bind_texture(size_t passid, const basic::resource<graphic::texture>& texture)
            {
                assert(texture);
                for (auto& [pass, tex] : textures)
                {
                    if (pass == passid)
                    {
                        tex = texture;
                        return;
                    }
                }
                textures.push_back(texture_with_passid(passid, texture));
            }
            basic::resource<graphic::texture> get_texture(size_t passid) const
            {
                for (auto& [pass, tex] : textures)
                    if (pass == passid)
                        return tex;

                return nullptr;
            }

            static void JERefRegsiter()
            {
                typing::register_member(&Textures::tiling, "tiling");
                typing::register_member(&Textures::offset, "offset");
            }
        };

    }
    namespace Camera
    {
        struct Clip
        {
            float znear = 0.3f;
            float zfar = 1000.0f;

            static void JERefRegsiter()
            {
                typing::register_member(&Clip::znear, "znear");
                typing::register_member(&Clip::zfar, "zfar");
            }
        };

        struct Projection
        {
            float view[4][4] = {};
            float projection[4][4] = {};
            float inv_projection[4][4] = {};
        };

        struct OrthoProjection
        {
            float scale = 1.0f;
            static void JERefRegsiter()
            {
                typing::register_member(&OrthoProjection::scale, "scale");
            }
        };

        struct PerspectiveProjection
        {
            float angle = 75.0f;
            static void JERefRegsiter()
            {
                typing::register_member(&PerspectiveProjection::angle, "angle");
            }
        };

        struct Viewport
        {
            math::vec4 viewport = math::vec4(0, 0, 1, 1);
            static void JERefRegsiter()
            {
                typing::register_member(&Viewport::viewport, "viewport");
            }
        };

        struct RendToFramebuffer
        {
            basic::resource<graphic::framebuffer> framebuffer = nullptr;
        };
    }

    namespace Physics2D
    {
        struct Rigidbody
        {
            void* native_rigidbody = nullptr;
        };
        struct Mass
        {
            float density = 1.f;

            static void JERefRegsiter()
            {
                typing::register_member(&Mass::density, "density");
            }
        };
        struct Friction
        {
            float value = 1.f;
            static void JERefRegsiter()
            {
                typing::register_member(&Friction::value, "value");
            }
        };
        struct Restitution
        {
            float value = 1.f;
            static void JERefRegsiter()
            {
                typing::register_member(&Restitution::value, "value");
            }
        };
        struct Kinematics
        {
            math::vec2 linear_velocity = {};
            float angular_velocity = 0.f;
            float linear_damping = 0.f;
            float angular_damping = 0.f;
            float gravity_scale = 1.f;

            static void JERefRegsiter()
            {
                typing::register_member(&Kinematics::linear_velocity, "linear_velocity");
                typing::register_member(&Kinematics::angular_velocity, "angular_velocity");
                typing::register_member(&Kinematics::linear_damping, "linear_damping");
                typing::register_member(&Kinematics::angular_damping, "angular_damping");
                typing::register_member(&Kinematics::gravity_scale, "gravity_scale");
            }
        };
        struct Bullet
        {

        };
        struct BoxCollider
        {
            void* native_fixture = nullptr;
            math::vec2 scale = {1.f, 1.f};

            static void JERefRegsiter()
            {
                typing::register_member(&BoxCollider::scale, "scale");
            }
        };
        struct CircleCollider
        {
            void* native_fixture = nullptr;
            float scale = 1.f;

            static void JERefRegsiter()
            {
                typing::register_member(&CircleCollider::scale, "scale");
            }
        };
    }

    namespace Light2D
    {
        struct Color
        {
            math::vec4 color = math::vec4(1, 1, 1, 1);

            static void JERefRegsiter()
            {
                typing::register_member(&Color::color, "color");
            }
        };

        struct Shadow
        {
            size_t resolution_width = 1024;
            size_t resolution_height = 768;
            basic::resource<graphic::framebuffer> shadow_buffer = nullptr;
            float shape_shadow_scale = 2.f;

            static void JERefRegsiter()
            {
                typing::register_member(&Shadow::resolution_width, "resolution_width");
                typing::register_member(&Shadow::resolution_height, "resolution_height");
                typing::register_member(&Shadow::shape_shadow_scale, "shape_shadow_scale");
            }
        };

        struct Parallel
        {

        };

        struct Point
        {
            float decay = 10.f;

            static void JERefRegsiter()
            {
                typing::register_member(&Point::decay, "decay");
            }
        };

        struct CameraPass
        {
            basic::resource<graphic::framebuffer> defer_rend_aim = nullptr;
            basic::resource<jeecs::graphic::framebuffer> defer_light_effect = nullptr;
        };

        struct Block
        {
            struct block_mesh
            {
                jeecs::vector<math::vec2> m_block_points = {
                    math::vec2(-0.5f, -0.5f),
                    math::vec2(-0.5f, 0.5f),
                    math::vec2(0.5f, 0.5f),
                    math::vec2(0.5f, -0.5f),
                };
                basic::resource<graphic::vertex> m_block_mesh = nullptr;

                inline std::string to_string()const
                {
                    std::string result = "#je_light2d_block_shape#";
                    result += "size:" + std::to_string(m_block_points.size()) + ";";
                    for (size_t id = 0; id < m_block_points.size(); ++id)
                    {
                        result += std::to_string(id) + ":" + m_block_points[id].to_string() + ";";
                    }
                    return result;
                }
                inline void parse(const char*& databuf)
                {
                    size_t readed_length = 0;

                    size_t size = 0;
                    if (sscanf(databuf, "#je_light2d_block_shape#size:%zu;%zn", &size, &readed_length) == 1)
                    {
                        databuf += readed_length;
                        m_block_points.clear();
                        m_block_mesh = nullptr;
                        for (size_t i = 0; i < size; ++i)
                        {
                            size_t idx = 0;
                            math::vec2 pos;
                            if (sscanf(databuf, "%zu:(%f,%f);%zn", &idx, &pos.x, &pos.y, &readed_length) == 3)
                            {
                                databuf += readed_length;
                                m_block_points.push_back(pos);
                            }
                            else
                                m_block_points.push_back(math::vec2(0.f, 0.f));
                        }
                    }
                }
            };

            block_mesh mesh;
            bool shadow = true;

            static void JERefRegsiter()
            {
                typing::register_member(&Block::mesh, "mesh");
                typing::register_member(&Block::shadow, "shadow");
            }
        };
    }

    inline std::string game_entity::name()
    {
        return je_ecs_get_name_of_entity(this);
    }

    inline std::string game_entity::name(const std::string& _name)
    {
        return je_ecs_set_name_of_entity(this, _name.c_str());
    }

    namespace math
    {
        struct ray
        {
        public:
            vec3 orgin = { 0,0,0 };
            vec3 direction = { 0,0,1 };

            ray() = default;

            ray(ray&&) = default;
            ray(const ray&) = default;

            ray& operator = (ray&&) = default;
            ray& operator = (const ray&) = default;

            ray(const vec3& _orgin, const vec3& _direction) :
                orgin(_orgin),
                direction(_direction)
            {

            }
            ray(const Transform::Translation& camera_trans, const Camera::Projection& camera_proj, const vec2& screen_pos, bool ortho)
            {
                //根据摄像机和屏幕坐标创建射线
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
                bool intersected = false;
                vec3 place = {};
                float distance = INFINITY;

                intersect_result() = default;
                intersect_result(bool rslt, float dist = INFINITY, const vec3& plce = vec3(0, 0, 0)) :
                    intersected(rslt),
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

                *u = T.dot(P);// T.dot(P);
                if (*u < 0.0f || *u > det)
                    return false;

                // Q

                vec3 Q = T.cross(E1);// T.Cross(E1);

                // Calculate v and make sure u + v <= 1
                *v = direction.dot(Q); //direction.dot(Q);

                if (*v < 0.0f || *u + *v > det)
                    return false;

                // Calculate t, scale parameters, ray intersects triangle

                *t = E2.dot(Q);// .dot(Q);

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
                //if (vec3::dot(delta, direction) < 0.0f)
                //	return false;
                return intersect_result(true, delta.length(), clid);
            }
            intersect_result intersect_rectangle(const vec3& v0, const vec3& v1, const vec3& v2, const vec3& v3) const
            {
                /*

                v0				v3


                v1				v2

                */



                auto&& inresult = intersect_triangle(v0, v1, v2);
                if (inresult.intersected)
                    return inresult;
                return intersect_triangle(v0, v3, v2);
            }
            intersect_result intersect_box(const vec3& size, const vec3& centerpos, const quat& rotation) const
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

                //pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = size.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = size.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = size.z / 2.0f);

                //rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = (rotation * finalBoxPos[i]) + centerpos;
                {
                    //front
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //back
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //left
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //right
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //top
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //buttom
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                }


                return minResult;
            }
            intersect_result intersect_entity(const Transform::Translation& translation, const Renderer::Shape* entity_shape, float insRange = 0.0f) const
            {
                vec3 entity_box_sz;
                if (entity_shape && entity_shape->vertex && entity_shape->vertex->enabled())
                {
                    auto* vertex_dat = entity_shape->vertex->resouce()->m_raw_vertex_data;
                    entity_box_sz = { vertex_dat->m_size_x, vertex_dat->m_size_y ,vertex_dat->m_size_z };
                }
                else
                    entity_box_sz = vec3(1, 1, 0); // default shape size

                vec3 finalBoxPos[8];
                intersect_result minResult = false;
                minResult.distance = INFINITY;

                //pos
                finalBoxPos[0].x = finalBoxPos[2].x = finalBoxPos[4].x = finalBoxPos[6].x =
                    -(finalBoxPos[1].x = finalBoxPos[3].x = finalBoxPos[5].x = finalBoxPos[7].x = entity_box_sz.x / 2.0f);
                finalBoxPos[2].y = finalBoxPos[3].y = finalBoxPos[6].y = finalBoxPos[7].y =
                    -(finalBoxPos[0].y = finalBoxPos[1].y = finalBoxPos[4].y = finalBoxPos[5].y = entity_box_sz.y / 2.0f);
                finalBoxPos[0].z = finalBoxPos[1].z = finalBoxPos[2].z = finalBoxPos[3].z =
                    -(finalBoxPos[4].z = finalBoxPos[5].z = finalBoxPos[6].z = finalBoxPos[7].z = entity_box_sz.z / 2.0f);

                //rot and transform
                for (int i = 0; i < 8; i++)
                    finalBoxPos[i] = mat4trans(translation.object2world, finalBoxPos[i]);

                {
                    //front
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[3], finalBoxPos[2]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //back
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[4], finalBoxPos[5], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //left
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[2], finalBoxPos[6], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //right
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[1], finalBoxPos[3], finalBoxPos[7], finalBoxPos[5]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //top
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[0], finalBoxPos[1], finalBoxPos[5], finalBoxPos[4]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                    //buttom
                    {
                        auto&& f = intersect_rectangle(finalBoxPos[2], finalBoxPos[3], finalBoxPos[7], finalBoxPos[6]);
                        if (f.intersected && f.distance < minResult.distance)
                            minResult = f;
                    }
                }

                return minResult;
            }
        };
    }

    namespace enrty
    {
        inline void module_entry()
        {
            // 0. register built-in components
            using namespace typing;
            type_info::of<Transform::LocalPosition>("Transform::LocalPosition");
            type_info::of<Transform::LocalRotation>("Transform::LocalRotation");
            type_info::of<Transform::LocalScale>("Transform::LocalScale");
            type_info::of<Transform::ChildAnchor>("Transform::ChildAnchor");
            type_info::of<Transform::LocalToWorld>("Transform::LocalToWorld");
            type_info::of<Transform::LocalToParent>("Transform::LocalToParent");
            type_info::of<Transform::Translation>("Transform::Translation");

            type_info::of<Renderer::Rendqueue>("Renderer::Rendqueue");
            type_info::of<Renderer::Shape>("Renderer::Shape");
            type_info::of<Renderer::Shaders>("Renderer::Shaders");
            type_info::of<Renderer::Textures>("Renderer::Textures");

            type_info::of<Camera::Clip>("Camera::Clip");
            type_info::of<Camera::Projection>("Camera::Projection");
            type_info::of<Camera::OrthoProjection>("Camera::OrthoProjection");
            type_info::of<Camera::PerspectiveProjection>("Camera::PerspectiveProjection");
            type_info::of<Camera::Viewport>("Camera::Viewport");
            type_info::of<Camera::RendToFramebuffer>("Camera::RendToFramebuffer");

            type_info::of<Light2D::Color>("Light2D::Color");
            type_info::of<Light2D::Shadow>("Light2D::Shadow");
            type_info::of<Light2D::Parallel>("Light2D::Parallel");
            type_info::of<Light2D::Point>("Light2D::Point");
            type_info::of<Light2D::CameraPass>("Light2D::CameraPass");
            type_info::of<Light2D::Block>("Light2D::Block");

            type_info::of<Physics2D::Rigidbody>("Physics2D::Rigidbody");
            type_info::of<Physics2D::Kinematics>("Physics2D::Kinematics");
            type_info::of<Physics2D::Mass>("Physics2D::Mass");
            type_info::of<Physics2D::Bullet>("Physics2D::Bullet");
            type_info::of<Physics2D::BoxCollider>("Physics2D::BoxCollider");
            type_info::of<Physics2D::CircleCollider>("Physics2D::CircleCollider"); 
            type_info::of<Physics2D::Restitution>("Physics2D::Restitution");
            type_info::of<Physics2D::Friction>("Physics2D::Friction");

            // 1. register core&graphic systems.
            jeecs_entry_register_core_systems();
        }

        inline void module_leave()
        {
            // 0. ungister this module components
            typing::type_info::unregister_all_type_in_shutdown();
        }
    }

    namespace input
    {
        inline bool keydown(keycode key)
        {
            return je_io_is_keydown(key);
        }
        inline float wheel(int group)
        {
            return je_io_wheel(group);
        }
        inline math::ivec2 mousepos(int group)
        {
            int x, y;
            je_io_mouse_pos(group, &x, &y);
            return { x, y };
        }
        inline math::ivec2 windowsize()
        {
            int x, y;
            je_io_windowsize(&x, &y);
            return { x, y };
        }

        template<typing::typehash_t hash_v1, int v2>
        static bool _isUp(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (!keystate) && lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template<typing::typehash_t hash_v1, int v2>
        static bool _firstDown(bool keystate)
        {
            static bool lastframekeydown;
            bool res = (keystate) && !lastframekeydown;
            lastframekeydown = keystate;
            return res;
        }
        template<typing::typehash_t hash_v1, int v2>
        static bool _doubleClick(bool keystate, float i = 0.1f)
        {
            static typing::ms_stamp_t lact_click_tm = je_clock_time_stamp();
            static bool release_for_next_click = false;

            auto cur_time = je_clock_time_stamp();

            // 1. first click.
            if (keystate)
            {
                if (release_for_next_click && cur_time - lact_click_tm < (typing::ms_stamp_t)(i * 1000.f))
                {
                    // Is Double click!
                    lact_click_tm = 0; // reset the time.
                    release_for_next_click = false;
                    return true;
                }
                // Release for a long time, re calc the time
                if (!release_for_next_click || cur_time - lact_click_tm > (typing::ms_stamp_t)(i * 1000.f))
                {
                    lact_click_tm = cur_time;
                    release_for_next_click = false;
                }
            }
            else
            {
                if (!release_for_next_click && cur_time - lact_click_tm < (typing::ms_stamp_t)(i * 1000.f))
                    release_for_next_click = true;
            }
            return false;
        }

        static void is_up(...);
        static void first_down(...);
        static void double_click(...);// just for fool ide

#define is_up _isUp<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
#define first_down _firstDown<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
#define double_click _doubleClick<jeecs::basic::hash_compile_time(__FILE__),__LINE__>
    }
}

#endif
