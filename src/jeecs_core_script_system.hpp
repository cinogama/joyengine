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
            woort_GCPin* m_pin_value;
            woovalue()
            {
                m_pin_value = woort_GC_Pin_create(1);
            }
            ~woovalue()
            {
                if (m_pin_value != nullptr)
                    woort_GC_Pin_destroy(m_pin_value);
            }
            woovalue(const woovalue &val)
                : woovalue()
            {
                woort_Value tmpval;

                woort_GC_Pin_get_internal_value(&tmpval, val.m_pin_value, 0);
                woort_GC_Pin_set_internal_value(m_pin_value, 0, &tmpval);
            }
            woovalue(woovalue &&val)
            {
                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;
            }
            woovalue &operator=(const woovalue &val)
            {
                woort_Value tmpval;

                woort_GC_Pin_get_internal_value(&tmpval, val.m_pin_value, 0);
                woort_GC_Pin_set_internal_value(m_pin_value, 0, &tmpval);

                return *this;
            }
            woovalue &operator=(woovalue &&val)
            {
                if (m_pin_value != nullptr)
                    woort_GC_Pin_destroy(m_pin_value);

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
            void JEParseFromScriptType(woort_value v)
            {
                woort_GC_Pin_set_value(m_pin_value, 0, v);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_GC_Pin_get_value(v, m_pin_value, 0);
            }
        };
    }
}
