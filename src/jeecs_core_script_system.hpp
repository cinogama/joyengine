#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

#include <shared_mutex>
#include <unordered_map>
#include <list>
#include <mutex>

namespace jeecs
{
    namespace script
    {
        struct woovalue
        {
            wo_pin_value m_pin_value;
            woovalue()
            {
                m_pin_value = wo_create_pin_value();
            }
            ~woovalue()
            {
                if (m_pin_value != nullptr)
                    wo_close_pin_value(m_pin_value);
            }
            woovalue(const woovalue& val)
                : woovalue()
            {
                _wo_value tmpval;

                wo_pin_value_get(&tmpval, val.m_pin_value);
                wo_pin_value_set(m_pin_value, &tmpval);
            }
            woovalue(woovalue&& val)
            {
                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;
            }
            woovalue& operator=(const woovalue& val)
            {
                _wo_value tmpval;

                wo_pin_value_get(&tmpval, val.m_pin_value);
                wo_pin_value_set(m_pin_value, &tmpval);

                return *this;
            }
            woovalue& operator=(woovalue&& val)
            {
                if (m_pin_value != nullptr)
                    wo_close_pin_value(m_pin_value);

                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;

                return *this;
            }

            static const char* JEScriptTypeName()
            {
                return "dynamic";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "";
            }
            void JEParseFromScriptType(wo_vm vm, wo_value v)
            {
                wo_pin_value_set(m_pin_value, v);
            }
            void JEParseToScriptType(wo_vm vm, wo_value v) const
            {
                wo_pin_value_get(v, m_pin_value);
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

        std::recursive_mutex _coroutine_list_mx;
        std::list<wo_vm> _coroutine_list;
        
        void dispatch_coroutine_vm(wo_vm vmm)
        {
            std::lock_guard sg1(_coroutine_list_mx);
            _coroutine_list.push_back(vmm);
        }

        inline static thread_local ScriptRuntimeSystem* system_instance = nullptr;

        ScriptRuntimeSystem(game_world w)
            : game_system(w)
        {

        }
        ~ScriptRuntimeSystem()
        {
            for (auto* co_vm : _coroutine_list)
            {
                wo_release_vm(co_vm);
            }
        }

        void CommitUpdate(jeecs::selector&)
        {
            system_instance = this;

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
