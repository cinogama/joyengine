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
        std::mutex                              _m_type_holder_mx;

        std::vector<jeecs::typing::type_info*>  _m_type_holder_list;
        std::unordered_map<jeecs::typing::typehash_t, jeecs::typing::typeid_t>  _m_type_hash_id_mapping;
        std::unordered_map<std::string, jeecs::typing::typeid_t>  _m_type_name_id_mapping;

        type_info_holder() = default;
    public:
        ~type_info_holder()
        {

        }

        inline static type_info_holder* holder() noexcept
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
            jeecs::typing::move_func_t      _mover,
            jeecs::typing::to_string_func_t _to_string,
            jeecs::typing::parse_func_t     _parse,
            je_typing_class                 _typecls) noexcept
        {
            std::lock_guard g1(_m_type_holder_mx);

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

                // Register alias-hash
                _m_type_hash_id_mapping[_hash] = *out_typeid;
                jeecs::debug::log_warn("Type '%s' with different type-hash. alias-hash has been created.");
                return false;
            }

            // Not found! create!
            jeecs::typing::type_info* tinfo = jeecs::basic::create_new<jeecs::typing::type_info>();
            tinfo->m_typename = jeecs::basic::make_new_string(_name);
            tinfo->m_size = _size;
            tinfo->m_chunk_size = jeecs::basic::allign_size(_size, jeecs::typing::ALLIGN_BASE);
            tinfo->m_hash = _hash;
            tinfo->m_constructor = _constructor;
            tinfo->m_destructor = _destructor;
            tinfo->m_copier = _copier;
            tinfo->m_mover = _mover;
            tinfo->m_to_string = _to_string;
            tinfo->m_parse = _parse;
            tinfo->m_type_class = _typecls;
            tinfo->m_member_types = nullptr;

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

        jeecs::typing::type_info* get_info_by_id(jeecs::typing::typeid_t id) noexcept
        {
            if (id && id != jeecs::typing::INVALID_TYPE_ID)
            {
                std::lock_guard g1(_m_type_holder_mx);
                if (id <= _m_type_holder_list.size())
                    return _m_type_holder_list[id - 1];
            }
            return nullptr;
        }

        jeecs::typing::type_info* get_info_by_name(const char* name) noexcept
        {
            if (name)
            {
                std::lock_guard g1(_m_type_holder_mx);
                auto fnd = _m_type_name_id_mapping.find(name);
                if (fnd != _m_type_name_id_mapping.end())
                    return _m_type_holder_list[fnd->second - 1];
            }
            return nullptr;
        }

        std::vector<jeecs::typing::type_info*> get_all_registed_types() noexcept
        {
            std::lock_guard g1(_m_type_holder_mx);
            return _m_type_holder_list;
        }

        // ATTENTION: This function do not promise for thread safe.
        void register_member_by_id(jeecs::typing::typeid_t classid,
            const jeecs::typing::type_info* _membertype,
            const char* _member_name,
            ptrdiff_t _member_offset) noexcept
        {
            jeecs::typing::member_info* meminfo = jeecs::basic::create_new<jeecs::typing::member_info>();

            auto* classtype = get_info_by_id(classid);

            meminfo->m_class_type = classtype;
            meminfo->m_member_type = _membertype;
            meminfo->m_member_name = jeecs::basic::make_new_string(_member_name);
            meminfo->m_member_offset = _member_offset;
            meminfo->m_next_member = nullptr;

            auto** m_new_member_ptr = const_cast<jeecs::typing::member_info**>(&classtype->m_member_types);
            while (*m_new_member_ptr)
                m_new_member_ptr = &((*m_new_member_ptr)->m_next_member);

            *m_new_member_ptr = meminfo;
        }

        void unregister_member_info(jeecs::typing::type_info* classtype) noexcept
        {
            auto* meminfo = classtype->m_member_types;
            while (meminfo)
            {
                auto* curmem = const_cast<jeecs::typing::member_info*>(meminfo);
                meminfo = meminfo->m_next_member;

                je_mem_free((void*)curmem->m_member_name);

                jeecs::basic::destroy_free(curmem);
            }
        }

        void unregister_by_id(jeecs::typing::typeid_t id) noexcept
        {
            if (id && id != jeecs::typing::INVALID_TYPE_ID)
            {
                std::lock_guard g1(_m_type_holder_mx);
                if (id <= _m_type_holder_list.size())
                {
                    jeecs::typing::type_info* typeinfo = nullptr;
                    std::swap(typeinfo, _m_type_holder_list[id - 1]);

                    _m_type_hash_id_mapping.erase(typeinfo->m_hash);
                    _m_type_name_id_mapping.erase(typeinfo->m_typename);
                    je_mem_free((void*)typeinfo->m_typename);

                    unregister_member_info(typeinfo);
                }
            }
        }


    };
}

bool je_typing_find_or_register(
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
    je_typing_class                 _typecls)
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
            _mover,
            _to_string,
            _parse,
            _typecls);
}

const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id)
{
    return jeecs_impl::type_info_holder::holder()->get_info_by_id(_id);
}

const jeecs::typing::type_info* je_typing_get_info_by_name(
    const char* type_name)
{
    return jeecs_impl::type_info_holder::holder()->get_info_by_name(type_name);
}

void je_typing_unregister(
    jeecs::typing::typeid_t _id)
{
    jeecs_impl::type_info_holder::holder()->unregister_by_id(_id);
}

void je_register_member(
    jeecs::typing::typeid_t         _classid,
    const jeecs::typing::type_info* _membertype,
    const char*                     _member_name,
    ptrdiff_t                       _member_offset)
{
    jeecs_impl::type_info_holder::holder()
        ->register_member_by_id(_classid, _membertype, _member_name, _member_offset);
}

///////////////////////////////////////////////////////////////////////////

// NOTE: need free the return result by 'je_mem_free'
extern "C" // FUCKYOU MSVC!
const jeecs::typing::type_info** jedbg_get_all_registed_types()
{
    auto&& types = jeecs_impl::type_info_holder::holder()->get_all_registed_types();
    auto result = (const jeecs::typing::type_info**)je_mem_alloc(sizeof(const jeecs::typing::type_info*) * (types.size() + 1));
    result[types.size()] = nullptr;

    memcpy(result, types.data(), types.size());

    return result;
}