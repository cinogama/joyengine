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

namespace jeecs::type {}

using namespace std;
using namespace jeecs;
using namespace jeecs::type;

namespace jeecs::type
{
    namespace
    {
        class FactoryHolder
        {
            JECS_DISABLE_MOVE_AND_COPY(FactoryHolder);

            using sequence_type_records = vector<je_type_info_t*>;
            using named_type_records = unordered_map<string, je_type_id_t>;
            using hash_type_records = unordered_map<je_type_hash_t, je_type_id_t>;

            mutable shared_mutex _m_factory_mx;

            sequence_type_records _m_type_records;
            named_type_records _m_named_type_records;
            hash_type_records _m_hash_type_records;

            FactoryHolder() = default;

        public:
            ~FactoryHolder()
            {
#ifndef NDEBUG
                debug::loginfo("Factory manager shutdown.");
#endif
            }

            static FactoryHolder* holder() noexcept
            {
                static FactoryHolder holder;
                return &holder;
            }

            je_type_info_t* declear_type(
                const char* _typename,
                je_type_hash_t _hash,
                size_t _size,
                size_t _align,
                je_typing_class_t _typecls,
                je_construct_func_t _constructor,
                je_destruct_func_t _destructor,
                je_copy_construct_func_t _copy_constructor,
                je_move_construct_func_t _move_constructor) noexcept
            {
                assert(_align != 0 && (_align & (_align - 1)) == 0);
                assert(_size != 0 && _size % _align == 0);

                je_type_info_t* const tinfo = new je_type_info_t{
                    typing::INVALID_TYPE_ID,
                    basic::make_new_string(_typename),
                    _size,
                    _align,
                    _hash,

                    _constructor,
                    _destructor,
                    _copy_constructor,
                    _move_constructor,

                    _typecls,

                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr,
                };

                lock_guard g1(_m_factory_mx);

                auto fnd = _m_named_type_records.find(tinfo->m_typename);
                if (fnd != _m_named_type_records.end())
                {
                    tinfo->m_id = fnd->second;

                    // Replace the last type info, and link the new type info.
                    auto* last_type_info = _m_type_records.at(tinfo->m_id - 1);
                    while (last_type_info->m_next != nullptr)
                        last_type_info = const_cast<je_type_info_t*>(last_type_info->m_next);

                    last_type_info->m_next = tinfo;
                }
                else
                {
                    // Not registered, trying to find a empty slot or append at the end.
                    auto holder_fnd = find(_m_type_records.begin(), _m_type_records.end(), nullptr);
                    if (holder_fnd != _m_type_records.end())
                    {
                        *holder_fnd = tinfo;
                        tinfo->m_id = 1 + (je_type_id_t)(holder_fnd - _m_type_records.begin());
                    }
                    else
                    {
                        _m_type_records.push_back(tinfo);
                        tinfo->m_id = _m_type_records.size();
                    }

                    if (_m_named_type_records.emplace(tinfo->m_typename, tinfo->m_id).second == false)
                        // Failed to instert, should not happen.
                        debug::logfatal("Type '%s' is already registered?, please check!", tinfo->m_typename);

                    if (_m_hash_type_records.emplace(tinfo->m_hash, tinfo->m_id).second == false)
                        debug::logerr("Type '%s' hash conflict with '%s', please check!",
                            tinfo->m_typename, _m_type_records[_m_hash_type_records[tinfo->m_hash] - 1]->m_typename);
                }

                return tinfo;
            }

            void reset_type(
                const je_type_info_t* _type,
                size_t _size,
                size_t _align,
                je_construct_func_t _constructor,
                je_destruct_func_t _destructor,
                je_copy_construct_func_t _copy_constructor,
                je_move_construct_func_t _move_constructor) noexcept
            {
                assert(_align != 0 && (_align & (_align - 1)) == 0);
                assert(_size != 0 && _size % _align == 0);

                je_type_info_t* const tinfo =
                    const_cast<je_type_info_t*>(_type);

                tinfo->m_size = _size;
                tinfo->m_align = _align;

                tinfo->m_constructor = _constructor;
                tinfo->m_destructor = _destructor;
                tinfo->m_copier = _copy_constructor;
                tinfo->m_mover = _move_constructor;

                if (tinfo->m_member_types != nullptr)
                    _free_member_infors(tinfo->m_member_types);

                if (tinfo->m_system_updaters != nullptr)
                    _free_system_updater(tinfo->m_system_updaters);

                if (tinfo->m_script_parsers != nullptr)
                    _free_script_parser(tinfo->m_script_parsers);

                tinfo->m_member_types = nullptr;
                tinfo->m_script_parsers = nullptr;
                tinfo->m_system_updaters = nullptr;
            }

            void declear_member(
                je_type_info_t* _classtype,
                const je_type_info_t* _membertype,
                const char* _member_name,
                const char* _woovalue_type_may_null,
                woort_value _boxed_woovalue_init_may_ignored,
                ptrdiff_t _member_offset) noexcept
            {
                if (_classtype->m_member_types == nullptr)
                {
                    auto members = new je_typeinfo_member_t();
                    members->m_member_count = 0;
                    members->m_members = nullptr;

                    _classtype->m_member_types = members;
                }
                else
                {
                    assert(_classtype->m_member_types->m_member_count != 0);
                    assert(_classtype->m_member_types->m_members != nullptr);
                }
                auto* class_member_info = const_cast<je_typeinfo_member_t*>(_classtype->m_member_types);

                je_member_info_t* meminfo =
                    new je_member_info_t();

                meminfo->m_class_type = _classtype;
                meminfo->m_member_type = _membertype;
                meminfo->m_member_name = basic::make_new_string(_member_name);
                meminfo->m_woovalue_type_may_null = nullptr;
                meminfo->m_woovalue_init_may_null = nullptr;
                meminfo->m_member_offset = _member_offset;
                meminfo->m_next_member = nullptr;

                if (_woovalue_type_may_null != nullptr && _boxed_woovalue_init_may_ignored != WOORT_IGNORE)
                {
                    meminfo->m_woovalue_type_may_null = basic::make_new_string(_woovalue_type_may_null);
                    meminfo->m_woovalue_init_may_null = woort_GCPin_create(1);
                    woort_GCPin_set(
                        meminfo->m_woovalue_init_may_null, 0, _boxed_woovalue_init_may_ignored);
                }

                auto** m_new_member_ptr = &class_member_info->m_members;
                while (*m_new_member_ptr)
                    m_new_member_ptr = &((*m_new_member_ptr)->m_next_member);

                *m_new_member_ptr = meminfo;

                ++class_member_info->m_member_count;
            }

            void declear_script_parser(
                je_type_info_t* _typeinfo,
                je_parse_c2w_func_t _c2w,
                je_parse_w2c_func_t _w2c,
                const char* _woolang_typename,
                const char* _woolang_typedecl) noexcept
            {
                if (_typeinfo->m_script_parsers != nullptr)
                {
#ifndef NDEBUG
                    debug::logwarn("Type '%s' has already registered script parser, skip.", _typeinfo->m_typename);
#endif
                }
                else
                {
                    je_typeinfo_script_parser_t* parser = new je_typeinfo_script_parser_t();
                    parser->m_script_parse_c2w = _c2w;
                    parser->m_script_parse_w2c = _w2c;
                    parser->m_woolang_typename = basic::make_new_string(_woolang_typename);
                    parser->m_woolang_typedecl = basic::make_new_string(_woolang_typedecl);

                    _typeinfo->m_script_parsers = parser;
                }
            }

            void declear_system_updater(
                je_type_info_t* _typeinfo,
                je_on_enable_or_disable_func_t _on_enable,
                je_on_enable_or_disable_func_t _on_disable,
                je_update_func_t _pre_update,
                je_update_func_t _state_update,
                je_update_func_t _update,
                je_update_func_t _physics_update,
                je_update_func_t _transform_update,
                je_update_func_t _late_update,
                je_update_func_t _commit_update,
                je_update_func_t _graphic_update)
            {
                if (_typeinfo->m_system_updaters != nullptr)
                {
#ifndef NDEBUG
                    debug::logwarn("Type '%s' has already registered system updater, skip.", _typeinfo->m_typename);
#endif
                }
                else
                {
                    assert(_pre_update != nullptr);
                    assert(_state_update != nullptr);
                    assert(_update != nullptr);
                    assert(_physics_update != nullptr);
                    assert(_transform_update != nullptr);
                    assert(_late_update != nullptr);
                    assert(_commit_update != nullptr);
                    assert(_graphic_update != nullptr);

                    je_typeinfo_system_updater_t* updater = new je_typeinfo_system_updater_t();
                    updater->m_on_enable = _on_enable;
                    updater->m_on_disable = _on_disable;
                    updater->m_pre_update = _pre_update;
                    updater->m_state_update = _state_update;
                    updater->m_update = _update;
                    updater->m_physics_update = _physics_update;
                    updater->m_transform_update = _transform_update;
                    updater->m_late_update = _late_update;
                    updater->m_commit_update = _commit_update;
                    updater->m_graphic_update = _graphic_update;

                    _typeinfo->m_system_updaters = updater;
                }
            }

            static void _free_member_infors(const je_typeinfo_member_t* member) noexcept
            {
                auto* meminfo = member->m_members;
                while (meminfo != nullptr)
                {
                    auto* current_member = meminfo;
                    meminfo = meminfo->m_next_member;

                    je_mem_free((void*)current_member->m_member_name);

                    if (current_member->m_woovalue_type_may_null != nullptr)
                        je_mem_free((void*)current_member->m_woovalue_type_may_null);

                    const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                    {
                        if (current_member->m_woovalue_init_may_null != nullptr)
                            woort_GCPin_destroy(current_member->m_woovalue_init_may_null);
                    }
                    if (entry_tmp_gc_guard)
                        woort_GC_sync_marking_unlock();

                    delete current_member;
                }
                delete member;
            }
            static void _free_system_updater(const je_typeinfo_system_updater_t* updater) noexcept
            {
                delete updater;
            }
            static void _free_script_parser(const je_typeinfo_script_parser_t* parser) noexcept
            {
                je_mem_free((void*)parser->m_woolang_typename);
                je_mem_free((void*)parser->m_woolang_typedecl);
                delete parser;
            }

            void undeclear_type(je_type_info_t* tinfo)
            {
                lock_guard g1(_m_factory_mx);

                assert(tinfo->m_id != typing::INVALID_TYPE_ID && tinfo->m_id <= _m_type_records.size());
                auto* type_list = _m_type_records.at(tinfo->m_id - 1);

                bool need_update_type = type_list == tinfo;
                bool need_clear_hashed_and_named_type = type_list == tinfo && tinfo->m_next == nullptr;

                if (need_update_type)
                {
                    _m_type_records.at(tinfo->m_id - 1) =
                        const_cast<je_type_info_t*>(tinfo->m_next);
                }
                else
                {
                    while (type_list->m_next != tinfo)
                        type_list = const_cast<je_type_info_t*>(type_list->m_next);

                    assert(type_list != nullptr);
                    type_list->m_next = tinfo->m_next;
                }

                if (need_clear_hashed_and_named_type)
                {
                    _m_named_type_records.erase(tinfo->m_typename);
                    _m_hash_type_records.erase(tinfo->m_hash);
                }

                if (tinfo->m_member_types != nullptr)
                    _free_member_infors(tinfo->m_member_types);

                if (tinfo->m_system_updaters != nullptr)
                    _free_system_updater(tinfo->m_system_updaters);

                if (tinfo->m_script_parsers != nullptr)
                    _free_script_parser(tinfo->m_script_parsers);

                je_mem_free((void*)tinfo->m_typename);

                delete tinfo;
            }

            je_type_info_t* get_info_by_id(je_type_id_t id) noexcept
            {
                if (id && id != typing::INVALID_TYPE_ID)
                {
                    shared_lock sg1(_m_factory_mx);
                    if (id <= _m_type_records.size())
                        return _m_type_records[id - 1];
                }
                return nullptr;
            }
            je_type_info_t* get_info_by_hash(je_type_hash_t hash) noexcept
            {
                shared_lock sg1(_m_factory_mx);
                auto fnd = _m_hash_type_records.find(hash);
                if (fnd != _m_hash_type_records.end())
                    return _m_type_records[fnd->second - 1];
                return nullptr;
            }
            je_type_info_t* get_info_by_name(const char* name) noexcept
            {
                assert(name != nullptr);
                {
                    shared_lock sg1(_m_factory_mx);
                    auto fnd = _m_named_type_records.find(name);
                    if (fnd != _m_named_type_records.end())
                        return _m_type_records[fnd->second - 1];

                    // Not found? find it from woolang name?
                    for (auto* typeinfo : _m_type_records)
                    {
                        if (typeinfo == nullptr)
                            continue;

                        if (typeinfo->m_script_parsers != nullptr &&
                            strcmp(typeinfo->m_script_parsers->m_woolang_typename, name) == 0)
                            return typeinfo;
                    }
                }
                return nullptr;
            }
            vector<je_type_info_t*> get_all_registed_types() noexcept
            {
                shared_lock sg1(_m_factory_mx);
                vector<je_type_info_t*> types;

                for (auto* t : _m_type_records)
                {
                    if (t != nullptr)
                        types.push_back(t);
                }
                return types;
            }
        };
    }
}

const je_type_info_t* je_typing_register(
    const char* _name,
    je_type_hash_t _hash,
    size_t _size,
    size_t _align,
    je_typing_class_t _typecls,
    je_construct_func_t _constructor,
    je_destruct_func_t _destructor,
    je_copy_construct_func_t _copy_constructor,
    je_move_construct_func_t _move_constructor)
{
    return FactoryHolder::holder()->declear_type(
        _name,
        _hash,
        _size,
        _align,
        _typecls,
        _constructor,
        _destructor,
        _copy_constructor,
        _move_constructor);
}

void je_typing_reset(
    const je_type_info_t* _tinfo,
    size_t _size,
    size_t _align,
    je_construct_func_t _constructor,
    je_destruct_func_t _destructor,
    je_copy_construct_func_t _copy_constructor,
    je_move_construct_func_t _move_constructor)
{
    return FactoryHolder::holder()->reset_type(
        _tinfo,
        _size,
        _align,
        _constructor,
        _destructor,
        _copy_constructor,
        _move_constructor);
}

/* OPTIONAL: If _id is invalid or unregistered */ const je_type_info_t* je_typing_info_get_by_id(
    je_type_id_t _id)
{
    return FactoryHolder::holder()
        ->get_info_by_id(_id);
}

/* OPTIONAL: If _hash is not registered */ const je_type_info_t* je_typing_info_get_by_hash(
    je_type_hash_t _hash)
{
    return FactoryHolder::holder()
        ->get_info_by_hash(_hash);
}

/* OPTIONAL: If type_name is not found */ const je_type_info_t* je_typing_info_get_by_name(
    const char* type_name)
{
    return FactoryHolder::holder()->get_info_by_name(type_name);
}

void je_typing_unregister(const je_type_info_t* tinfo)
{
    FactoryHolder::holder()->undeclear_type(
        const_cast<je_type_info_t*>(tinfo));
}

void je_typing_member_register(
    const je_type_info_t* _classtype,
    const je_type_info_t* _membertype,
    const char* _member_name,
    /* OPTIONAL */ const char* _woovalue_type_may_null,
    woort_value _boxed_woovalue_init_may_ignored,
    ptrdiff_t _member_offset)
{
    FactoryHolder::holder()
        ->declear_member(
            const_cast<je_type_info_t*>(_classtype),
            _membertype,
            _member_name,
            _woovalue_type_may_null,
            _boxed_woovalue_init_may_ignored,
            _member_offset);
}

void je_typing_script_parser_register(
    const je_type_info_t* _type,
    je_parse_c2w_func_t c2w,
    je_parse_w2c_func_t w2c,
    const char* woolang_typename,
    const char* woolang_typedecl)
{
    FactoryHolder::holder()
        ->declear_script_parser(
            const_cast<je_type_info_t*>(_type),
            c2w,
            w2c,
            woolang_typename,
            woolang_typedecl);
}

void je_typing_system_updater_register(
    const je_type_info_t* _type,
    je_on_enable_or_disable_func_t _on_enable,
    je_on_enable_or_disable_func_t _on_disable,
    je_update_func_t _pre_update,
    je_update_func_t _state_update,
    je_update_func_t _update,
    je_update_func_t _physics_update,
    je_update_func_t _transform_update,
    je_update_func_t _late_update,
    je_update_func_t _commit_update,
    je_update_func_t _graphic_update)
{
    FactoryHolder::holder()
        ->declear_system_updater(
            const_cast<je_type_info_t*>(_type),
            _on_enable,
            _on_disable,
            _pre_update,
            _state_update,
            _update,
            _physics_update,
            _transform_update,
            _late_update,
            _commit_update,
            _graphic_update);
}

///////////////////////////////////////////////////////////////////////////

// NOTE: need free the return result by 'je_mem_free'
const je_type_info_t** je_debug_types_registered_get_all(void)
{
    auto&& types = FactoryHolder::holder()->get_all_registed_types();
    auto result = (const je_type_info_t**)je_mem_alloc(sizeof(const je_type_info_t*) * (types.size() + 1));
    result[types.size()] = nullptr;

    memcpy(result, types.data(), types.size() * sizeof(const je_type_info_t*));

    return result;
}

#define JE_DECL_ATOMIC_OPERATOR_API(TYPE)                                                                 \
    TYPE je_atomic_exchange_##TYPE(TYPE *aim, TYPE value)                                                 \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        return launder(reinterpret_cast<_atomic_t *>(aim))->exchange(value);                         \
    }                                                                                                     \
    bool je_atomic_cas_##TYPE(TYPE *aim, TYPE *comparer, TYPE value)                                      \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        return launder(reinterpret_cast<_atomic_t *>(aim))->compare_exchange_weak(*comparer, value); \
    }                                                                                                     \
    TYPE je_atomic_fetch_add_##TYPE(TYPE *aim, TYPE value)                                                \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        return launder(reinterpret_cast<_atomic_t *>(aim))->fetch_add(value);                        \
    }                                                                                                     \
    TYPE je_atomic_fetch_sub_##TYPE(TYPE *aim, TYPE value)                                                \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        return launder(reinterpret_cast<_atomic_t *>(aim))->fetch_sub(value);                        \
    }                                                                                                     \
    TYPE je_atomic_fetch_##TYPE(TYPE *aim)                                                                \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        return launder(reinterpret_cast<_atomic_t *>(aim))->load();                                  \
    }                                                                                                     \
    void je_atomic_store_##TYPE(TYPE *aim, TYPE value)                                                    \
    {                                                                                                     \
        using _atomic_t = atomic<TYPE>;                                                              \
        static_assert(sizeof(_atomic_t) == sizeof(TYPE));                                                 \
        static_assert(_atomic_t::is_always_lock_free);                                                    \
        launder(reinterpret_cast<_atomic_t *>(aim))->store(value);                                   \
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
