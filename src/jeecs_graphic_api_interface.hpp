namespace jeecs::graphic
{
    class basic_interface
    {
        JECS_DISABLE_MOVE_AND_COPY(basic_interface);
    public:
        size_t m_interface_width;
        size_t m_interface_height;

    public:
        basic_interface()
            : m_interface_width(0)
            , m_interface_height(0)
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

        virtual void create_interface(jegl_context* thread, const jegl_interface_config* config) = 0;
        virtual void swap_for_opengl() = 0;
        virtual update_result update() = 0;
        virtual void shutdown(bool reboot) = 0;

        virtual void* interface_handle() const = 0;
    };
}