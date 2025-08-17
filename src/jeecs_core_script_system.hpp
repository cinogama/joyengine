#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
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
        inline thread_local game_system* current_script_game_system_instance = nullptr;

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
            woovalue(const woovalue &val)
                : woovalue()
            {
                _wo_value tmpval;

                wo_pin_value_get(&tmpval, val.m_pin_value);
                wo_pin_value_set(m_pin_value, &tmpval);
            }
            woovalue(woovalue &&val)
            {
                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;
            }
            woovalue &operator=(const woovalue &val)
            {
                _wo_value tmpval;

                wo_pin_value_get(&tmpval, val.m_pin_value);
                wo_pin_value_set(m_pin_value, &tmpval);

                return *this;
            }
            woovalue &operator=(woovalue &&val)
            {
                if (m_pin_value != nullptr)
                    wo_close_pin_value(m_pin_value);

                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;

                return *this;
            }

            static const char *JEScriptTypeName()
            {
                return "dynamic";
            }
            static const char *JEScriptTypeDeclare()
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
}
