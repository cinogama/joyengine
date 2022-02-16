// jeecs_type_mgr.cpp
//
// Here will store the typing & factory informations

#define JE_IMPL
#include "jeecs.hpp"

#include <shared_mutex>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>

namespace jeecs_impl
{
    class type_info_holder
    {
        std::mutex                              _m_type_holde_mx;

        std::vector<jeecs::typing::type_info*>  _m_type_holder_list;
        std::unordered_map<jeecs::typing::typehash_t, jeecs::typing::typeid_t>  _m_type_hash_id_mapping;
        std::unordered_map<std::string, jeecs::typing::typeid_t>  _m_type_name_id_mapping;

        type_info_holder() = default;
    public:
        ~type_info_holder()
        {

        }

        inline static type_info_holder* holder()
        {
            static type_info_holder holder;
            return &holder;
        }
    public:
        bool register_type(
            jeecs::typing::typeid_t* out_typeid,
            const char* _name,
            jeecs::typing::typehash_t _hash,
            size_t                    _size,
            jeecs::typing::construct_func_t _constructor,
            jeecs::typing::destruct_func_t  _destructor,
            jeecs::typing::copy_func_t      _copier,
            jeecs::typing::move_func_t      _mover)
        {
            std::lock_guard g1(_m_type_holde_mx);

            // 1. Find typeid with hash
            auto fnd_with_hash = _m_type_hash_id_mapping.find(_hash);
            if (fnd_with_hash != _m_type_hash_id_mapping.end())
            {
                *out_typeid = fnd_with_hash->second;
                return false;
            }

            // 2. Find typeid with name
            auto fnd_with_name = _m_type_name_id_mapping.find(_name);
            if (fnd_with_name != _m_type_name_id_mapping.end())
            {
                *out_typeid = fnd_with_name->second;
                return false;
            }

            // Not found! create!
            jeecs::typing::type_info* tinfo = jeecs::basic::create_new<jeecs::typing::type_info>();
            tinfo->m_typename = jeecs::basic::make_new_string(_name);
            tinfo->m_size = _size;
            tinfo->m_chunk_size = jeecs::basic::allign_size(_size, jeecs::arch::ALLIGN_BASE);
            tinfo->m_hash = _hash;
            tinfo->m_constructor = _constructor;
            tinfo->m_destructor = _destructor;
            tinfo->m_copier = _copier;
            tinfo->m_mover = _mover;

            // Ok Find a place to store~
            auto holder_fnd = std::find(
                _m_type_holder_list.begin(),
                _m_type_holder_list.end(),
                nullptr);
            if (holder_fnd != _m_type_holder_list.end())
            {
                *holder_fnd = tinfo;
                *out_typeid = 1 + (jeecs::typing::typeid_t)(holder_fnd - _m_type_holder_list.begin());
            }
            else
            {
                _m_type_holder_list.push_back(tinfo);
                *out_typeid = _m_type_holder_list.size();
            }
            _m_type_hash_id_mapping[_hash] = *out_typeid;
            _m_type_name_id_mapping[_name] = *out_typeid;
            tinfo->m_id = *out_typeid;

            return true;
        }

        jeecs::typing::type_info* get_info_by_id(jeecs::typing::typeid_t id)
        {

            if (id && id != jeecs::typing::INVALID_TYPE_ID)
            {
                std::lock_guard g1(_m_type_holde_mx);
                if (id <= _m_type_holder_list.size())
                    return _m_type_holder_list[id - 1];
            }
            return nullptr;
        }
        void unregister_by_id(jeecs::typing::typeid_t id)
        {
            if (id && id != jeecs::typing::INVALID_TYPE_ID)
            {
                std::lock_guard g1(_m_type_holde_mx);
                if (id <= _m_type_holder_list.size())
                {
                    jeecs::typing::type_info* typeinfo = nullptr;
                    std::swap(typeinfo, _m_type_holder_list[id - 1]);

                    _m_type_hash_id_mapping.erase(typeinfo->m_hash);
                    _m_type_name_id_mapping.erase(typeinfo->m_typename);
                    je_mem_free((void*)typeinfo->m_typename);
                }
            }
        }
    };
}

JE_API bool je_typing_find_or_register(
    jeecs::typing::typeid_t* out_typeid,
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover)
{
    return
        jeecs_impl::type_info_holder::holder()->register_type(
            out_typeid,
            _name,
            _hash,
            _size,
            _constructor,
            _destructor,
            _copier,
            _mover);
}

JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id)
{
    return jeecs_impl::type_info_holder::holder()->get_info_by_id(_id);
}

JE_API void je_typing_unregister(
    jeecs::typing::typeid_t _id)
{
    jeecs_impl::type_info_holder::holder()->unregister_by_id(_id);
}