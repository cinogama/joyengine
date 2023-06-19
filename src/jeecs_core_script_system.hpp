#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <shared_mutex>
#include <unordered_map>

namespace jeecs
{
    namespace Script
    {
        struct Woolang
        {
            struct filepath
            {
                std::string path = {};

                std::string to_string()const
                {
                    return std::string("#file#") + path;
                }
                void parse(const std::string& str)
                {
                    path = str.substr(6);
                }
            };
            filepath    path;

            bool        _vm_failed = false;
            wo_vm       _vm_instance = nullptr;

            wo_integer_t _vm_create_func = 0;
            wo_integer_t _vm_update_func = 0;
            wo_value     _vm_context = nullptr;

            Woolang() = default;
            Woolang(const Woolang&) = delete;
            Woolang(Woolang&& woolang)
            {
                path = woolang.path;
                _vm_failed = woolang._vm_failed;
                _vm_instance = woolang._vm_instance;
                _vm_create_func = woolang._vm_create_func;
                _vm_update_func = woolang._vm_update_func;
                _vm_context = woolang._vm_context;
                woolang._vm_instance = nullptr;
            }
            ~Woolang()
            {
                if (_vm_instance != nullptr)
                {
                    wo_release_vm(_vm_instance);
                    _vm_instance = nullptr;
                }
            }
          
            static void JERefRegsiter()
            {
                typing::register_member(&Woolang::path, "path");
            }
        };
    }

    struct ScriptRuntimeSystem :public game_system
    {
        struct vm_info
        {
            wo_vm vm;
            wo_integer_t create_func;
            wo_integer_t update_func;
        };
        std::unordered_map<std::string, vm_info> _compiled_vms;

        ScriptRuntimeSystem(game_world w)
            : game_system(w)
        {

        }
        ~ScriptRuntimeSystem()
        {
            for (auto & [_, v] : _compiled_vms)
            {
                if (v.vm != nullptr)
                    wo_close_vm(v.vm);
            }
        }

        void Update()
        {
            select_from(get_world()).
                exec(
                    [this](game_entity e, Script::Woolang& woolang) 
                    {
                        if (woolang._vm_failed == true)
                            return;

                        if (woolang.path.path == "")
                        {
                            woolang._vm_failed = true;
                            return;
                        }

                        if (woolang._vm_instance == nullptr)
                        {
                            auto fnd = _compiled_vms.find(woolang.path.path);
                            const vm_info* found_base_vm_info = nullptr;
                            if (fnd == _compiled_vms.end())
                            {
                                auto &info = _compiled_vms[woolang.path.path];

                                auto* file = jeecs_file_open(woolang.path.path.c_str());
                                if (file == nullptr)
                                {
                                    jeecs::debug::logerr("Failed to open '%s' when loading script for entity.",
                                        woolang.path.path.c_str());

                                    woolang._vm_failed = true;
                                    return;
                                }

                                std::vector<char> srcdata(file->m_file_length);
                                auto readed_len = jeecs_file_read(srcdata.data(), sizeof(char), file->m_file_length, file);
                                assert(readed_len == file->m_file_length);

                                jeecs_file_close(file);

                                wo_vm basevm = wo_create_vm();
                                if (false == wo_load_binary(basevm, woolang.path.path.c_str(), srcdata.data(), srcdata.size()))
                                {
                                    jeecs::debug::logerr("Failed to load '%s':\n %s",
                                        woolang.path.path.c_str(),
                                        wo_get_compile_error(basevm, WO_DEFAULT));

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }
                                if (nullptr == wo_run(basevm))
                                {
                                    jeecs::debug::logerr("Failed to init '%s':\n %s",
                                        woolang.path.path.c_str(),
                                        wo_get_runtime_error(basevm));

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }

                                if ((info.create_func = wo_extern_symb(basevm, "create")) == 0)
                                {
                                    jeecs::debug::logerr("Failed to find 'create' function in '%s'.",
                                        woolang.path.path.c_str());

                                    wo_close_vm(basevm);

                                    woolang._vm_failed = true;
                                    return;
                                }
                                if ((info.update_func = wo_extern_symb(basevm, "update")) == 0)
                                {
                                    jeecs::debug::logerr("Failed to find 'update' function in '%s'.",
                                        woolang.path.path.c_str());

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
                            
                            wo_push_pointer(woolang._vm_instance, &e);
                            wo_value ctx = wo_invoke_rsfunc(woolang._vm_instance, woolang._vm_create_func, 1);
                            if (ctx == nullptr)
                            {
                                jeecs::debug::logerr("Failed to create context for '%s':\n %s",
                                    woolang.path.path.c_str(),
                                    wo_get_runtime_error(woolang._vm_instance));

                                woolang._vm_failed = true;
                                return;
                            }
                            woolang._vm_context = wo_push_val(woolang._vm_instance, ctx);
                        }

                        assert(woolang._vm_instance != nullptr);
                        wo_push_pointer(woolang._vm_instance, &e);
                        wo_push_val(woolang._vm_instance, woolang._vm_context);
                        if (wo_invoke_rsfunc(woolang._vm_instance, woolang._vm_update_func, 2) == nullptr)
                        {
                            jeecs::debug::logerr("Failed to create context for '%s':\n %s",
                                woolang.path.path.c_str(),
                                wo_get_runtime_error(woolang._vm_instance));

                            woolang._vm_failed = true;
                            return;
                        }
                    }
                );
        }
    };
}