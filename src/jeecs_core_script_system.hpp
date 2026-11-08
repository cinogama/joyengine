#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs.hpp"

namespace jeecs::script
{
    inline thread_local game_system* current_script_game_system_instance = nullptr;

    struct woovalue
    {
        woort_GCPin* m_pin_value;

        woovalue();
        ~woovalue();
        woovalue(const woovalue& val);
        woovalue(woovalue&& val);
        woovalue& operator=(const woovalue& val);
        woovalue& operator=(woovalue&& val);

        static const char* JEScriptTypeName();
        static const char* JEScriptTypeDeclare();
        void JEParseFromScriptType(woort_value v);
        void JEParseToScriptType(woort_value v) const;
    };
}
