#ifndef JE_IMPL
#   define JE_IMPL
#endif
#include "jeecs/graphic.hpp"
#include "jeecs/prefix.hpp"

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
            const je_gl_interface_config_t *config) = 0;

        virtual update_result update() = 0;
        virtual void shutdown(bool reboot) = 0;

        virtual void swap_for_opengl() = 0;
        virtual void *interface_handle() const = 0;
    };
}
