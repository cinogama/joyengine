// jeecs_type_mgr.cpp
//
// Here will store the typing & factory informations

#define JE_IMPL
#define JE_ENABLE_DEBUG_API
#include "jeecs.hpp"

#include <shared_mutex>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <optional>

namespace jeecs_impl
{
    class type_info_holder
    {
        JECS_DISABLE_MOVE_AND_COPY(type_info_holder);
        std::mutex                              _m_type_holder_mx;

        std::unordered_map<jeecs::typing::type_info*, std::vector<jeecs::typing::type_info*>>
            _m_registered_typeinfo;

        std::vector<jeecs::typing::type_info*>  _m_type_holder_list;
        std::unordered_map<std::string, jeecs::typing::typeid_t>  _m_type_name_id_mapping;

        type_info_holder() = default;
    public:
        ~type_info_holder()
        {
            jeecs::debug::loginfo("Type manager shutdown.");
        }

        inline static type_info_holder* holder() noexcept
        {
            static type_info_holder holder;
            return &holder;
        }

        inline jeecs::typing::type_info* _record_typeinfo(jeecs::typing::typeid_t id, jeecs::typing::type_info* tinfo)
        {
            assert(id != 0);
            auto* type_info = _m_type_holder_list[id - 1];
            auto& registered_typeinfo_list = _m_registered_typeinfo[type_info];

            assert(type_info != nullptr &&
                std::find(
                    registered_typeinfo_list.begin(),
                    registered_typeinfo_list.end(), tinfo) == registered_typeinfo_list.end());

            registered_typeinfo_list.push_back(tinfo);
            return tinfo;
        }
    public:
        jeecs::typing::type_info* register_type(
            const char* _name,
            jeecs::typing::typehash_t _hash,
            size_t                    _size,
            jeecs::typing::construct_func_t _constructor,
            jeecs::typing::destruct_func_t  _destructor,
            jeecs::typing::copy_func_t      _copier,
            jeecs::typing::move_func_t      _mover,
            jeecs::typing::to_string_func_t _to_string,
            jeecs::typing::parse_func_t     _parse,
            jeecs::typing::update_func_t    _state_update,
            jeecs::typing::update_func_t    _pre_update,
            jeecs::typing::update_func_t    _update,
            jeecs::typing::update_func_t    _script_update,
            jeecs::typing::update_func_t    _late_update,
            jeecs::typing::update_func_t    _apply_update,
            jeecs::typing::update_func_t    _commit_update,
            je_typing_class                 _typecls) noexcept
        {
            // 0. Create type_info instance
            jeecs::typing::type_info* tinfo = new jeecs::typing::type_info();
            tinfo->m_typename = jeecs::basic::make_new_string(_name);
            tinfo->m_size = _size;
            tinfo->m_chunk_size = jeecs::basic::allign_size(_size, jeecs::typing::ALLIGN_BASE);

            assert(tinfo->m_size != 0 && tinfo->m_chunk_size != 0);

            tinfo->m_hash = _hash;
            tinfo->m_constructor = _constructor;
            tinfo->m_destructor = _destructor;
            tinfo->m_copier = _copier;
            tinfo->m_mover = _mover;
            tinfo->m_to_string = _to_string;
            tinfo->m_parse = _parse;
            tinfo->m_type_class = _typecls;
            tinfo->m_state_update = _state_update;
            tinfo->m_pre_update = _pre_update;
            tinfo->m_update = _update;
            tinfo->m_script_update = _script_update;
            tinfo->m_late_update = _late_update;
            tinfo->m_apply_update = _apply_update;
            tinfo->m_commit_update = _commit_update;
            tinfo->m_member_count = 0;
            tinfo->m_member_types = nullptr;
            tinfo->m_script_parser_info = nullptr;

            std::lock_guard g1(_m_type_holder_mx);

            // 1. Find typeid with name
            if (auto fnd_with_name = _m_type_name_id_mapping.find(_name); fnd_with_name != _m_type_name_id_mapping.end())
            {
                tinfo->m_id = fnd_with_name->second;
            }
            else
            {
                // 3. No such type, append.
                auto holder_fnd = std::find(_m_type_holder_list.begin(), _m_type_holder_list.end(), nullptr);
                if (holder_fnd != _m_type_holder_list.end())
                {
                    *holder_fnd = tinfo;
                    tinfo->m_id = 1 + (jeecs::typing::typeid_t)(holder_fnd - _m_type_holder_list.begin());
                }
                else
                {
                    _m_type_holder_list.push_back(tinfo);
                    tinfo->m_id = _m_type_holder_list.size();
                }
                _m_type_name_id_mapping[_name] = tinfo->m_id;
            }

            return _record_typeinfo(tinfo->m_id, tinfo);
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
            std::vector<jeecs::typing::type_info*> types;

            for (auto* t : _m_type_holder_list)
            {
                if (t != nullptr)
                    types.push_back(t);
            }
            return _m_type_holder_list;
        }

        // ATTENTION: This function do not promise for thread safe for adding new member.
        void register_member(
            const jeecs::typing::type_info* _classtype,
            const jeecs::typing::type_info* _membertype,
            const char* _member_name,
            ptrdiff_t _member_offset) noexcept
        {
            jeecs::typing::member_info* meminfo = new jeecs::typing::member_info();

            meminfo->m_class_type = _classtype;
            meminfo->m_member_type = _membertype;
            meminfo->m_member_name = jeecs::basic::make_new_string(_member_name);
            meminfo->m_member_offset = _member_offset;
            meminfo->m_next_member = nullptr;

            auto** m_new_member_ptr = const_cast<jeecs::typing::member_info**>(&_classtype->m_member_types);
            while (*m_new_member_ptr)
                m_new_member_ptr = &((*m_new_member_ptr)->m_next_member);

            *m_new_member_ptr = meminfo;

            ++*const_cast<size_t* volatile>(&_classtype->m_member_count);
        }
        void unregister_member_info(jeecs::typing::type_info* classtype) noexcept
        {
            auto* meminfo = classtype->m_member_types;
            while (meminfo)
            {
                auto* curmem = const_cast<jeecs::typing::member_info*>(meminfo);
                meminfo = meminfo->m_next_member;

                je_mem_free((void*)curmem->m_member_name);

                delete curmem;
            }
        }
        void unregister_type(const jeecs::typing::type_info* tinfo) noexcept
        {
            std::lock_guard g1(_m_type_holder_mx);

            assert(tinfo != nullptr);
            if (tinfo->m_id && tinfo->m_id != jeecs::typing::INVALID_TYPE_ID)
            {
                auto id = tinfo->m_id;

                assert(id != 0 && id <= _m_type_holder_list.size());
                if (auto*& current_type_info = _m_type_holder_list[id - 1])
                {
                    // 1. Free current type info from list;
                    auto& registered_typeinfo = _m_registered_typeinfo[current_type_info];
                    auto fnd = std::find(
                        registered_typeinfo.begin(),
                        registered_typeinfo.end(),
                        const_cast<jeecs::typing::type_info*>(tinfo));

                    if (fnd == registered_typeinfo.end())
                        jeecs::debug::logerr("Type info: '%p' is invalid, please check.", tinfo);
                    else
                    {
                        bool need_update_current_type_info = fnd == registered_typeinfo.begin();

                        auto typename_to_free = (*fnd)->m_typename;

                        unregister_member_info(*fnd);
                        unregister_script_parser(*fnd);

                        if (*fnd != current_type_info)
                            // First registed type info, will reuse.
                            delete (*fnd);

                        registered_typeinfo.erase(fnd);

                        if (registered_typeinfo.empty())
                        {
                            // All type info has been freed, close current type info.
                            _m_type_name_id_mapping.erase(typename_to_free);
                            _m_registered_typeinfo.erase(current_type_info);

                            // current_type_info->m_typename has been freed.
                            delete current_type_info;

                            // Free slot
                            current_type_info = nullptr;
                        }
                        else if (need_update_current_type_info)
                        {
                            // Update current type info.
                            *current_type_info = *registered_typeinfo.front();
                        }

                        je_mem_free((void*)typename_to_free);
                    }
                    return;
                }
            }
            jeecs::debug::logerr("Type info: '%p' is invalid, please check.", tinfo);
        }
        void unregister_script_parser(jeecs::typing::type_info* classtype) noexcept
        {
            if (classtype->m_script_parser_info != nullptr)
            {
                je_mem_free((void*)classtype->m_script_parser_info->m_woolang_typename);
                je_mem_free((void*)classtype->m_script_parser_info->m_woolang_typedecl);
                delete classtype->m_script_parser_info;
            }
        }
        void register_script_parser(
            const jeecs::typing::type_info* tinfo,
            jeecs::typing::parse_c2w_func_t c2w,
            jeecs::typing::parse_w2c_func_t w2c,
            const char* woolang_typename,
            const char* woolang_typedecl)
        {
            auto* sinfo = new jeecs::typing::script_parser_info;
            sinfo->m_script_parse_c2w = c2w;
            sinfo->m_script_parse_w2c = w2c;
            sinfo->m_woolang_typename = jeecs::basic::make_new_string(woolang_typename);
            sinfo->m_woolang_typedecl = jeecs::basic::make_new_string(woolang_typedecl);

            const_cast<jeecs::typing::script_parser_info* volatile&>(tinfo->m_script_parser_info) = sinfo;
        }
    };
}

const jeecs::typing::type_info* je_typing_register(
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t                    _size,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t  _destructor,
    jeecs::typing::copy_func_t      _copier,
    jeecs::typing::move_func_t      _mover,
    jeecs::typing::to_string_func_t _to_string,
    jeecs::typing::parse_func_t     _parse,
    jeecs::typing::update_func_t    _state_update,
    jeecs::typing::update_func_t    _pre_update,
    jeecs::typing::update_func_t    _update,
    jeecs::typing::update_func_t    _script_update,
    jeecs::typing::update_func_t    _late_update,
    jeecs::typing::update_func_t    _apply_update,
    jeecs::typing::update_func_t    _commit_update,
    je_typing_class                 _typecls)
{
    return
        jeecs_impl::type_info_holder::holder()->register_type(
            _name,
            _hash,
            _size,
            _constructor,
            _destructor,
            _copier,
            _mover,
            _to_string,
            _parse,
            _state_update,
            _pre_update,
            _update,
            _script_update,
            _late_update,
            _apply_update,
            _commit_update,
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

void je_typing_unregister(const jeecs::typing::type_info* tinfo)
{
    jeecs_impl::type_info_holder::holder()->unregister_type(tinfo);
}

void je_register_member(
    const jeecs::typing::type_info* _classtype,
    const jeecs::typing::type_info* _membertype,
    const char* _member_name,
    ptrdiff_t                       _member_offset)
{
    jeecs_impl::type_info_holder::holder()
        ->register_member(_classtype, _membertype, _member_name, _member_offset);
}

void je_register_script_parser(
    const jeecs::typing::type_info* _type,
    jeecs::typing::parse_c2w_func_t c2w,
    jeecs::typing::parse_w2c_func_t w2c,
    const char* woolang_typename,
    const char* woolang_typedecl)
{
    jeecs_impl::type_info_holder::holder()
        ->register_script_parser(
            _type,
            c2w,
            w2c,
            woolang_typename,
            woolang_typedecl);
}

///////////////////////////////////////////////////////////////////////////

// NOTE: need free the return result by 'je_mem_free'
const jeecs::typing::type_info** jedbg_get_all_registed_types(void)
{
    auto&& types = jeecs_impl::type_info_holder::holder()->get_all_registed_types();
    auto result = (const jeecs::typing::type_info**)je_mem_alloc(sizeof(const jeecs::typing::type_info*) * (types.size() + 1));
    result[types.size()] = nullptr;

    memcpy(result, types.data(), types.size() * sizeof(const jeecs::typing::type_info**));

    return result;
}

#define JE_DECL_ATOMIC_OPERATOR_API(TYPE)\
    TYPE je_atomic_exchange_##TYPE(TYPE* aim, TYPE value){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        return std::launder(reinterpret_cast<_atomic_t*>(aim))->exchange(value);\
    }\
    bool je_atomic_cas_##TYPE(TYPE* aim, TYPE* comparer, TYPE value){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        return std::launder(reinterpret_cast<_atomic_t*>(aim))->compare_exchange_weak(*comparer, value);\
    }\
    TYPE je_atomic_fetch_add_##TYPE(TYPE* aim, TYPE value){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        return std::launder(reinterpret_cast<_atomic_t*>(aim))->fetch_add(value);\
    }\
    TYPE je_atomic_fetch_sub_##TYPE(TYPE* aim, TYPE value){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        return std::launder(reinterpret_cast<_atomic_t*>(aim))->fetch_sub(value);\
    }\
    TYPE je_atomic_fetch_##TYPE(TYPE* aim){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        return std::launder(reinterpret_cast<_atomic_t*>(aim))->load();\
    }\
    void je_atomic_store_##TYPE(TYPE* aim, TYPE value){\
        using _atomic_t = std::atomic<TYPE>;\
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));\
        static_assert(_atomic_t::is_always_lock_free);\
        std::launder(reinterpret_cast<_atomic_t*>(aim))->store(value);\
    }

JE_DECL_ATOMIC_OPERATOR_API(int8_t);
JE_DECL_ATOMIC_OPERATOR_API(uint8_t);
JE_DECL_ATOMIC_OPERATOR_API(int16_t);
JE_DECL_ATOMIC_OPERATOR_API(uint16_t);
JE_DECL_ATOMIC_OPERATOR_API(int32_t);
JE_DECL_ATOMIC_OPERATOR_API(uint32_t);
JE_DECL_ATOMIC_OPERATOR_API(int64_t);
JE_DECL_ATOMIC_OPERATOR_API(uint64_t);
JE_DECL_ATOMIC_OPERATOR_API(size_t);
JE_DECL_ATOMIC_OPERATOR_API(intptr_t);

#undef JE_DECL_ATOMIC_OPERATOR_API
