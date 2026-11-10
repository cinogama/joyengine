#define JE_IMPL
#include "jeecs/prefix.hpp"
#include "jeecs_woolang_value.hpp"

namespace jeecs::woolang_runtime
{
    PinValue::PinValue()
    {
        const bool entry_tmp_gc_guard = woort_GC_sync_marking_lock();
        {
            m_pin_value = woort_GCPin_create(1);
        }
        if (entry_tmp_gc_guard)
            woort_GC_sync_marking_unlock();

    }
    PinValue::~PinValue()
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
    PinValue::PinValue(const PinValue& val)
        : PinValue()
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
    PinValue::PinValue(PinValue&& val)
    {
        m_pin_value = val.m_pin_value;
        val.m_pin_value = nullptr;
    }
    PinValue& PinValue::operator=(const PinValue& val)
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
    PinValue& PinValue::operator=(PinValue&& val)
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

    const char* PinValue::JEScriptTypeName()
    {
        return "dynamic";
    }
    const char* PinValue::JEScriptTypeDeclare()
    {
        return "";
    }
    void PinValue::JEParseFromScriptType(woort_value v)
    {
        woort_GCPin_set(m_pin_value, 0, v);
    }
    void PinValue::JEParseToScriptType(woort_value v) const
    {
        woort_GCPin_get(v, m_pin_value, 0);
    }
}
