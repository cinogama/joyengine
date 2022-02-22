#pragma once

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cassert>

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
#ifdef __cpp_lib_execution
#include <execution>
#endif

#define RS_FORCE_CAPI extern "C"{
#define RS_FORCE_CAPI_END }

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

namespace jeecs
{
    namespace typing
    {
        using typehash_t = size_t;
        using typeid_t = size_t;

        constexpr typeid_t INVALID_TYPE_ID = SIZE_MAX;
        constexpr size_t ALLIGN_BASE = alignof(std::max_align_t);

        struct type_info;

        using construct_func_t = void(*)(void*);
        using destruct_func_t = void(*)(void*);
        using copy_func_t = void(*)(void*, const void*);
        using move_func_t = void(*)(void*, void*);
    }

}

RS_FORCE_CAPI
JE_API void* je_mem_alloc(size_t sz);
JE_API void je_mem_free(void* ptr);

#define JE_LOG_NORMAL 0
#define JE_LOG_INFO 1
#define JE_LOG_WARNING 2
#define JE_LOG_ERROR 3
#define JE_LOG_FATAL 4
JE_API void je_log(int level, const char* format, ...);

// You should promise: different type should have different name.
JE_API bool je_typing_find_or_register(
    jeecs::typing::typeid_t* out_typeid,
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover);

JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

JE_API void je_typing_unregister(
    jeecs::typing::typeid_t _id);


RS_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete;\
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete;

    namespace debug
    {
        template<typename ... ArgTs>
        inline void log(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_NORMAL, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_info(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_INFO, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_warn(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_WARNING, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_error(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_ERROR, format, args...);
        }
        template<typename ... ArgTs>
        inline void log_fatal(const char* format, ArgTs&& ... args)
        {
            je_log(JE_LOG_FATAL, format, args...);
        }
    }

    namespace basic
    {
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

        template<typename NodeT>
        struct atomic_list
        {
            std::atomic<NodeT*> last_node = nullptr;

            void add_one(NodeT* node)
            {
                NodeT* last_last_node = last_node;// .exchange(node);
                do
                {
                    node->last = last_last_node;
                } while (!last_node.compare_exchange_weak(last_last_node, node));
            }

            NodeT* pick_all()
            {
                NodeT* result = nullptr;
                result = last_node.exchange(nullptr);

                return result;
            }

            NodeT* peek()
            {
                return last_node;
            }
        };

        template<typename T>
        struct default_functions
        {
            static void constructor(void* _ptr)
            {
                new(_ptr)T;
            }
            static void destructor(void* _ptr)
            {
                ((T*)_ptr)->~T();
            }
            static void copier(void* _ptr, const void* _be_copy_ptr)
            {
                new(_ptr)T(*(const T*)_be_copy_ptr);
            }
            static void mover(void* _ptr, void* _be_moved_ptr)
            {
                new(_ptr)T(std::move(*(T*)_be_moved_ptr));
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
    }

    namespace typing
    {
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
                typeid_t _register_or_get_type_id(const char* _typename)
                {
                    typeid_t id = INVALID_TYPE_ID;
                    if (je_typing_find_or_register(
                        &id,
                        _typename,
                        basic::type_hash<T>(),
                        sizeof(T),
                        basic::default_functions<T>::constructor,
                        basic::default_functions<T>::destructor,
                        basic::default_functions<T>::copier,
                        basic::default_functions<T>::mover))
                    {
                        // store to list for unregister
                        std::lock_guard g1(_m_self_registed_typeid_mx);
                        _m_self_registed_typeid.push_back(id);
                    }
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
            inline static typeid_t id(const char* _typename = typeid(T).name())
            {
                assert(!_m_shutdown_flag);
                static typeid_t registed_typeid = _type_guard._register_or_get_type_id<T>(_typename);
                return registed_typeid;
            }
            template<typename T>
            inline static const type_info* of(const char* _typename = typeid(T).name())
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

            void construct(void* addr) const
            {
                m_constructor(addr);
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
        };
    }


}

#endif