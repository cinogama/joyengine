#pragma once

#define MODULE_LIST \
    JE_MODULE_MACRO(file)

namespace jeecs
{
#define JE_MODULE_MACRO(m) namespace m{void bootup();}
    MODULE_LIST
#undef JE_MODULE_MACRO

#define JE_MODULE_MACRO(m) namespace m{void shutdown();}
        MODULE_LIST
#undef JE_MODULE_MACRO

        ;;

    static void bootup()
    {
#define JE_MODULE_MACRO(m) m::bootup();
        MODULE_LIST
#undef JE_MODULE_MACRO
    }

    static void shutdown()
    {
#define JE_MODULE_MACRO(m) m::shutdown();
        MODULE_LIST
#undef JE_MODULE_MACRO
    }
}

#undef MODULE_LIST;