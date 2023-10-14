#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <shared_mutex>
#include <unordered_map>
#include <list>
#include <mutex>

namespace jeecs
{
    struct ScriptRuntimeSystem :public game_system
    {
        struct vm_info
        {
            wo_vm vm;
            wo_integer_t create_func;
            wo_integer_t update_func;
        };

        std::unordered_map<std::string, vm_info> _compiled_vms;
        std::recursive_mutex _coroutine_list_mx;
        std::list<wo_vm> _coroutine_list;
        
        double deltatime = 0.;

        inline static thread_local ScriptRuntimeSystem* system_instance = nullptr;

        ScriptRuntimeSystem(game_world w)
            : game_system(w)
        {

        }
        ~ScriptRuntimeSystem()
        {
            for (auto& [_, v] : _compiled_vms)
            {
                if (v.vm != nullptr)
                    wo_close_vm(v.vm);
            }
            for (auto* co_vm : _coroutine_list)
            {
                wo_release_vm(co_vm);
            }
        }

        void CommitUpdate()
        {
            system_instance = this;

            select_from(get_world()).
                exec(
                    [this](game_entity e, Script::Woolang& woolang)
                    {
                        if (woolang._vm_failed == true)
                            return;

                        if (!woolang.path.has_resource())
                        {
                            woolang._vm_failed = true;
                            return;
                        }

                        if (woolang._vm_instance == nullptr)
                        {
                            auto fnd = _compiled_vms.find(woolang.path.get_path());
                            const vm_info* found_base_vm_info = nullptr;
                            if (fnd == _compiled_vms.end())
                            {
                                auto& info = _compiled_vms[woolang.path.get_path()];

                                auto* file = jeecs_file_open(woolang.path.get_path().c_str());
                                if (file == nullptr)
                                    file = jeecs_file_open((woolang.path.get_path().c_str() + std::string(".woo")).c_str());
                                if (file == nullptr)
                                {
                                    jeecs::debug::logerr("Failed to open '%s' when loading script for entity.",
                                        woolang.path.get_path().c_str());

                                    woolang._vm_failed = true;
                                    return;
                                }

                                std::vector<char> srcdata(file->m_file_length);
                                auto readed_len = jeecs_file_read(srcdata.data(), sizeof(char), file->m_file_length, file);
                                assert(readed_len == file->m_file_length);

                                jeecs_file_close(file);

                                wo_vm basevm = wo_create_vm();
                                if (false == wo_load_binary(basevm, woolang.path.get_path().c_str(), srcdata.data(), srcdata.size()))
                                {
                                    jeecs::debug::logerr("Failed to load '%s':\n %s",
                                        woolang.path.get_path().c_str(),
                                        wo_get_compile_error(basevm, WO_DEFAULT));

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }
                                wo_jit(basevm);
                                if (nullptr == wo_run(basevm))
                                {
                                    jeecs::debug::logerr("Failed to init '%s':\n %s",
                                        woolang.path.get_path().c_str(),
                                        wo_get_runtime_error(basevm));

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }

                                if ((info.create_func = wo_extern_symb(basevm, "create")) == 0)
                                {
                                    jeecs::debug::logerr("Failed to find 'create' function in '%s'.",
                                        woolang.path.get_path().c_str());

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }
                                if ((info.update_func = wo_extern_symb(basevm, "update")) == 0)
                                {
                                    jeecs::debug::logerr("Failed to find 'update' function in '%s'.",
                                        woolang.path.get_path().c_str());

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }
                                info.vm = basevm;
                                found_base_vm_info = &info;
                            }
                            else
                                found_base_vm_info = &fnd->second;

                            assert(found_base_vm_info != nullptr);

                            if (found_base_vm_info->vm == nullptr)
                            {
                                woolang._vm_failed = true;
                                return;
                            }

                            woolang._vm_instance = wo_borrow_vm(found_base_vm_info->vm);
                            woolang._vm_create_func = found_base_vm_info->create_func;
                            woolang._vm_update_func = found_base_vm_info->update_func;

                            jeecs::game_entity* entity = new jeecs::game_entity();
                            *entity = e;
                            wo_push_gchandle(woolang._vm_instance, entity,
                                nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });

                            wo_value ctx = wo_invoke_rsfunc(woolang._vm_instance, woolang._vm_create_func, 1);
                            if (ctx == nullptr)
                            {
                                jeecs::debug::logerr("Failed to create context for '%s':\n %s",
                                    woolang.path.get_path().c_str(),
                                    wo_get_runtime_error(woolang._vm_instance));

                                woolang._vm_failed = true;
                                return;
                            }
                            woolang._vm_context = wo_push_val(woolang._vm_instance, ctx);
                        }
                        assert(woolang._vm_instance != nullptr);
                        jeecs::game_entity* entity = new jeecs::game_entity();
                        *entity = e;
                        wo_push_gchandle(woolang._vm_instance, entity,
                            nullptr, [](void* ptr) {delete (jeecs::game_entity*)ptr; });
                        wo_push_val(woolang._vm_instance, woolang._vm_context);
                        if (wo_invoke_rsfunc(woolang._vm_instance, woolang._vm_update_func, 2) == nullptr)
                        {
                            jeecs::debug::logerr("Failed to update for '%s':\n %s",
                                woolang.path.get_path().c_str(),
                                wo_get_runtime_error(woolang._vm_instance));

                            woolang._vm_failed = true;
                            return;
                        }
                    }
            );
            std::list<wo_vm> current_co_list;

            std::lock_guard sg1(_coroutine_list_mx);
            current_co_list.swap(_coroutine_list);

            assert(_coroutine_list.empty());
            for (auto co_vm : current_co_list)
            {
                wo_value result = wo_dispatch(co_vm);
                if (result == nullptr)
                {
                    jeecs::debug::logerr("Coroutine %p failed: '%s':\n %s",
                        co_vm,
                        wo_get_runtime_error(co_vm),
                        wo_debug_trace_callstack(co_vm, 8));
                }
                else if (result == WO_CONTINUE)
                {
                    _coroutine_list.push_back(co_vm);
                }
                else
                {
                    wo_release_vm(co_vm);
                }
            }

            system_instance = nullptr;
        }
    };
}
