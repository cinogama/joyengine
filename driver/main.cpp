/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/

#include "jeecs.hpp"

extern "C"
{
    JE_EXPORT int NvOptimusEnablement = 0x00000001;
    JE_EXPORT int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace jeecs
{
    class main_thread_graphic_syncer_host
    {
        JECS_DISABLE_MOVE_AND_COPY(main_thread_graphic_syncer_host);

        std::mutex mx;
        std::condition_variable cv;

        bool entry_script_ended = false;
        std::optional<jegl_context*> graphic_context;

        static void callback(jegl_context* context, void* p)
        {
            main_thread_graphic_syncer_host* self =
                reinterpret_cast<main_thread_graphic_syncer_host*>(p);

            std::lock_guard g(self->mx);
            self->graphic_context = context;
            self->cv.notify_one();
        }
    public:
        main_thread_graphic_syncer_host() = default;

        void loop()
        {
            for (;;)
            {
                std::unique_lock ug(mx);
                cv.wait(ug, [this]() {return entry_script_ended || graphic_context.has_value(); });

                if (entry_script_ended)
                    break;

                jegl_context* gctx = graphic_context.value();
                graphic_context.reset();
                ug.unlock();

                jegl_sync_init(gctx, false);
                for (;;)
                {
                    switch (jegl_sync_update(gctx))
                    {
                    case jegl_sync_state::JEGL_SYNC_COMPLETE:
                        // Normal frame end, do nothing.
                        break;
                    case jegl_sync_state::JEGL_SYNC_REBOOT:
                        // Require to reboot graphic thread.
                        jegl_sync_shutdown(gctx, true);
                        jegl_sync_init(gctx, true);
                        break;
                    case jegl_sync_state::JEGL_SYNC_SHUTDOWN:
                        // Graphic thread want to shutdown, exit the loop.
                        goto _label_rend_context_ended;
                    }
                }
            _label_rend_context_ended:
                jegl_sync_shutdown(gctx, false);
            }
        }
        void entry()
        {
            jegl_register_sync_thread_callback(
                jeecs::main_thread_graphic_syncer_host::callback, this);

            std::thread entry_script(
                [this]()
                {
                    je_main_script_entry();

                    std::lock_guard g(mx);
                    entry_script_ended = true;
                    cv.notify_one();
                });

            loop();

            entry_script.join();
        }
    };
}

int main(int argc, char** argv)
{
    using namespace jeecs;

    je_init(argc, argv);

    jeecs::main_thread_graphic_syncer_host host;
    do
    {
        jeecs::typing::type_unregister_guard guard;
        entry::module_entry(&guard);
        {
            host.entry();
        }
        entry::module_leave(&guard);

    } while (0);

    je_finish();
}
