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
        virtual void create_interface(jegl_thread* thread, const jegl_interface_config* config) = 0;
        virtual void swap() = 0;
        virtual bool update() = 0;
        virtual void shutdown(bool reboot) = 0;

        virtual void* native_handle() = 0;
    };
}