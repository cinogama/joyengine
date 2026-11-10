#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/prefix.hpp"

namespace jeecs::woolang_runtime
{
    struct PinValue
    {
        woort_GCPin* m_pin_value;

        PinValue();
        ~PinValue();
        PinValue(const PinValue& val);
        PinValue(PinValue&& val);
        PinValue& operator=(const PinValue& val);
        PinValue& operator=(PinValue&& val);

        static const char* JEScriptTypeName();
        static const char* JEScriptTypeDeclare();
        void JEParseFromScriptType(woort_value v);
        void JEParseToScriptType(woort_value v) const;
    };
}
