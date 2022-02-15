#pragma once

#ifndef __cplusplus
#error jeecs.h only support for c++
#else

#include <cstdint>
#include <typeinfo>

#include <atomic>

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

// You should promise: different type should have different name.
JE_API jeecs::typing::typeid_t je_typing_find_or_register(
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover);

JE_API jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);

JE_API void je_typing_clear_register_buffer();

RS_FORCE_CAPI_END

namespace jeecs
{
#define JECS_DISABLE_MOVE_AND_COPY(TYPE) \
    TYPE(const TYPE &)  = delete;\
    TYPE(TYPE &&)       = delete;\
    TYPE& operator = (const TYPE &) = delete;\
    TYPE& operator = (TYPE &&) = delete;

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
            return new(je_mem_alloc(sizeof(T)))T(args...);
        }
        template<typename T, typename  ... ArgTs>
        T* create_new_n(size_t n)
        {
            return new(je_mem_alloc(sizeof(T) * n))T[n];
        }

        template<typename T>
        void destroy_free(T* address)
        {
            address->~T();
            je_mem_free(address);
        }
        template<typename T>
        void destroy_free_n(T* address, size_t n)
        {
            for (size_t i = 0; i < n; i++)
                address[i].~T();

            je_mem_free(address);
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
    }

    namespace typing
    {
        struct type_info
        {
            typehash_t  m_hash;
            const char* m_typename;
            size_t      m_size;
            size_t      m_chunk_size; // calc by je_typing_find_or_register

            construct_func_t    m_constructor;
            destruct_func_t     m_destructor;
            copy_func_t         m_copier;
            move_func_t         m_mover;

            template<typename T>
            static type_info* of(const char* _typename = typeid(T).name())
            {
                static typeid_t* registed_typeid = je_typing_find_or_register(
                    _typename, 
                    basic::type_hash<T>(),
                    sizeof(T),
                    basic::default_functions<T>::constructor,
                    basic::default_functions<T>::destructor,
                    basic::default_functions<T>::copier,
                    basic::default_functions<T>::mover);

                static type_info* registed_typeinfo = je_typing_get_info_by_id(registed_typeid);

                return registed_typeinfo;
            }
        };
    }

}

#endif