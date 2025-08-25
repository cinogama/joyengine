#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

namespace jeecs::graphic
{
    class basic_interface
    {
        JECS_DISABLE_MOVE_AND_COPY(basic_interface);

    public:
        basic_interface()
        {
        }
        virtual ~basic_interface() = default;

    public:
        enum update_result : uint8_t
        {
            NORMAL = 0,
            PAUSE,
            RESIZE,
            CLOSE,
        };

        virtual void create_interface(
            jegl_context *thread,
            const jegl_interface_config *config) = 0;

        virtual update_result update() = 0;
        virtual void shutdown(bool reboot) = 0;

        virtual void swap_for_opengl() = 0;
        virtual void *interface_handle() const = 0;
    };
}