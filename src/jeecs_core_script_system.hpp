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
                const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                {
                    m_pin_value = woort_GCPin_create(1);
                }
                if (entry_tmp_gc_guard)
                    woort_GC_sync_marking_unlock();

            }
            ~woovalue()
            {
                if (m_pin_value != nullptr)
                {
                    const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                    {
                        woort_GCPin_destroy(m_pin_value);
                    }
                    if (entry_tmp_gc_guard)
                        woort_GC_sync_marking_unlock();
                }
            }
            woovalue(const woovalue& val)
                : woovalue()
            {
                woort_Value tmpval;
                const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                {
                    woort_GCPin_get_internal(&tmpval, val.m_pin_value, 0);
                    woort_GCPin_set_internal(m_pin_value, 0, &tmpval);
                }
                if (entry_tmp_gc_guard)
                    woort_GC_sync_marking_unlock();
            }
            woovalue(woovalue&& val)
            {
                m_pin_value = val.m_pin_value;
                val.m_pin_value = nullptr;
            }
            woovalue& operator=(const woovalue& val)
            {
                woort_Value tmpval;

                const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                {
                    woort_GCPin_get_internal(&tmpval, val.m_pin_value, 0);
                    woort_GCPin_set_internal(m_pin_value, 0, &tmpval);
                }
                if (entry_tmp_gc_guard)
                    woort_GC_sync_marking_unlock();

                return *this;
            }
            woovalue& operator=(woovalue&& val)
            {
                if (m_pin_value != nullptr)
                {
                    const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
                    {
                        woort_GCPin_destroy(m_pin_value);
                    }
                    if (entry_tmp_gc_guard)
                        woort_GC_sync_marking_unlock();
                }

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
            void JEParseFromScriptType(woort_value v)
            {
                woort_GCPin_set(m_pin_value, 0, v);
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_GCPin_get(v, m_pin_value, 0);
            }
        };
    }
}
