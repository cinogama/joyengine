#pragma once

// ============================================================================
// jeecs/engine.hpp —— 引擎上下文（C++）：jeecs::game_engine_context（init/loop/frame 生命周期）
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/core.hpp"
#include "jeecs/logging.hpp"
#include "jeecs/graphic.hpp"
#include "jeecs/entry.hpp"

#ifdef __cplusplus
namespace jeecs
{
    class game_engine_context
    {
        JECS_DISABLE_MOVE_AND_COPY(game_engine_context);

        typing::type_unregister_guard types;

        enum class graphic_state
        {
            GRAPHIC_CONTEXT_NOT_READY,
            GRAPHIC_CONTEXT_READY,
        };
        graphic_state graphic_context_state_for_update_manually;
        // 未调用 prepare_graphic 就析构时也会被读取，必须初始化为空
        graphic::graphic_syncer_host* graphic_syncer = nullptr;

    public:
        game_engine_context(int argc, char** argv)
            : graphic_context_state_for_update_manually(
                graphic_state::GRAPHIC_CONTEXT_NOT_READY)
        {
            je_core_init(argc, argv);
            entry::module_entry(&types);
        }
        ~game_engine_context()
        {
            if (graphic_syncer != nullptr)
                delete graphic_syncer;

            entry::module_leave(&types);
            je_core_finish();
        }
        void loop()
        {
            (void)prepare_graphic(true);
            graphic_syncer->loop();
        }
    protected:
        enum class frame_update_result
        {
            FRAME_UPDATE_NOT_READY,
            FRAME_UPDATE_READY,
            FRAME_UPDATE_CLOSE_REQUESTED,
        };
        graphic::graphic_syncer_host* prepare_graphic(bool execute_entry)
        {
            graphic_syncer = new graphic::graphic_syncer_host(execute_entry);
            return graphic_syncer;
        }
        void mark_graphic_context_ready_manually()
        {
            assert(
                graphic_syncer != nullptr
                && graphic_context_state_for_update_manually == graphic_state::GRAPHIC_CONTEXT_NOT_READY);

            graphic_context_state_for_update_manually = graphic_state::GRAPHIC_CONTEXT_READY;
        }
        frame_update_result frame()
        {
            assert(graphic_syncer != nullptr);

            switch (graphic_context_state_for_update_manually)
            {
            case graphic_state::GRAPHIC_CONTEXT_NOT_READY:
                if (!graphic_syncer->check_context_ready_noblock(true))
                    break;

                mark_graphic_context_ready_manually();
                /* FALL THROUGH */
                [[fallthrough]];
            case graphic_state::GRAPHIC_CONTEXT_READY:
                if (graphic_syncer->frame())
                    return frame_update_result::FRAME_UPDATE_READY;
                else
                    return frame_update_result::FRAME_UPDATE_CLOSE_REQUESTED;
            default:
                debug::logfatal(
                    "Unknown graphic context state: %d",
                    (int)graphic_context_state_for_update_manually);
            }
            return frame_update_result::FRAME_UPDATE_NOT_READY;
        }
    };
}
#endif // __cplusplus
