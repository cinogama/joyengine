#pragma once

#define MODULE_LIST                             \
    JE_MODULE_MACRO(woolang_runtime)            \
    JE_MODULE_MACRO(time)                       \
    JE_MODULE_MACRO(file)                       \
    JE_MODULE_MACRO(log)                        \
    JE_MODULE_MACRO(audio)                      \
    JE_MODULE_MACRO(engine_woolang_api)         \
    JE_MODULE_MACRO(external_woolang_packages)  \
    JE_MODULE_MACRO(external_modules)           \
    JE_MODULE_MACRO(internal_script)            \
    JE_MODULE_MACRO(graphic::shader_wrapper)    \
    JE_MODULE_MACRO(core_components_and_systems)\
    JE_MODULE_MACRO(woolang_runtime::towoo)     \
    JE_MODULE_MACRO(graphic)                    \
    JE_MODULE_MACRO(ecs)

namespace jeecs
{
#define JE_MODULE_MACRO(m) namespace m{void bootup(int, char**);}
    MODULE_LIST
#undef JE_MODULE_MACRO

#define JE_MODULE_MACRO(m) namespace m{void shutdown();}
        MODULE_LIST
#undef JE_MODULE_MACRO

        ;;

    [[maybe_unused]] static void bootup(int argc, char** argv)
    {
#define JE_MODULE_MACRO(m) m::bootup(argc, argv);
        MODULE_LIST
#undef JE_MODULE_MACRO
    }

    [[maybe_unused]] static void shutdown()
    {
        using callback_t = void(*)(void);
        const callback_t callbacks[] = {
#define JE_MODULE_MACRO(m) &m::shutdown,
            MODULE_LIST
#undef JE_MODULE_MACRO
        };

        for (size_t i = sizeof(callbacks) / sizeof(callback_t); i > 0; --i)
            callbacks[i - 1]();
    }
}

#undef MODULE_LIST