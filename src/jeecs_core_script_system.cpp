#define JE_IMPL
#include "jeecs.hpp"
#include "jeecs_core_script_system.hpp"

namespace jeecs::script
{
    woovalue::woovalue()
    {
        const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
        {
            m_pin_value = woort_GCPin_create(1);
        }
        if (entry_tmp_gc_guard)
            woort_GC_sync_marking_unlock();

    }

    woovalue::~woovalue()
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

    woovalue::woovalue(const woovalue& val)
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

    woovalue::woovalue(woovalue&& val)
    {
        m_pin_value = val.m_pin_value;
        val.m_pin_value = nullptr;
    }

    woovalue& woovalue::operator=(const woovalue& val)
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

    woovalue& woovalue::operator=(woovalue&& val)
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

    const char* woovalue::JEScriptTypeName()
    {
        return "dynamic";
    }

    const char* woovalue::JEScriptTypeDeclare()
    {
        return "";
    }

    void woovalue::JEParseFromScriptType(woort_value v)
    {
        woort_GCPin_set(m_pin_value, 0, v);
    }

    void woovalue::JEParseToScriptType(woort_value v) const
    {
        woort_GCPin_get(v, m_pin_value, 0);
    }
}
