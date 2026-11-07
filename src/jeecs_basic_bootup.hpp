#pragma once

#define MODULE_LIST                             \
    JE_MODULE_MACRO(time)                       \
    JE_MODULE_MACRO(file)                       \
    JE_MODULE_MACRO(log)                        \
    JE_MODULE_MACRO(woolang_runtime)            \
    JE_MODULE_MACRO(engine_woolang_api)         \
    JE_MODULE_MACRO(external_woolang_packages)  \
    JE_MODULE_MACRO(external_modules)           \
    JE_MODULE_MACRO(internal_script)

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
        struct shutdown_guard {
            using callback_t = void(*)(void);
            callback_t m_callback;

            shutdown_guard(callback_t c)
                : m_callback(c)
            {
            }
            ~shutdown_guard()
            {
                m_callback();
            }
        };

#define JE_MODULE_MACRO(m) shutdown_guard m(&m::shutdown);
        MODULE_LIST
#undef JE_MODULE_MACRO
    }
}

#undef MODULE_LIST