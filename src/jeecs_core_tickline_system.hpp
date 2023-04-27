#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

#include <unordered_map>

// NOTE: 此处用于实现Tickline系统
//       详情见 Tickline.md


namespace jeecs
{
    namespace Tickline
    {
        struct Anchor
        {
            typing::uid_t uid = je_uid_generate();
            wo_vm executor_vm = nullptr;

            ~Anchor()
            {
                if (executor_vm != nullptr)
                    wo_release_vm(executor_vm);
            }
            static void JERefRegsiter()
            {
                typing::register_member(&Anchor::uid, "uid");
            }
        };
    }
    struct TicklineSystem :public game_system
    {
        // ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE 
        // 由虚拟机自主注册，若此虚拟机为空，则不支持TicklineSystem.
        inline static wo_vm ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE = nullptr;

        wo_integer_t m_externed_execute_func = 0;
        wo_vm m_current_woolang_virtual_machine = nullptr;
        std::unordered_map<typing::uid_t, std::vector<game_entity>> m_anchored_entities;

        TicklineSystem(game_world world) :game_system(world)
        {
            if (ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE != nullptr)
            {
                m_externed_execute_func = wo_extern_symb(ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE, "Tickline::Execute");
                if (m_externed_execute_func == 0)
                    jeecs::debug::logwarn("Failed to find function 'Tickline::Execute ' in Tickline.");
                else
                    m_current_woolang_virtual_machine = wo_borrow_vm(ENTRY_TICKLINE_WOOLANG_VIRTUAL_MACHINE);
            }

            if (m_current_woolang_virtual_machine == nullptr)
            {
                // 未获支持，立即移除此系统。
                world.remove_system<TicklineSystem>();
                jeecs::debug::logwarn("No available virtual machine for TicklineSystem.");
            }

        }

        ~TicklineSystem()
        {
            if (m_current_woolang_virtual_machine != nullptr)
                wo_release_vm(m_current_woolang_virtual_machine);
        }

        void PreUpdate()
        {
            // PreUpdate阶段，收集Anchor和实体，建立映射
            if (m_current_woolang_virtual_machine == nullptr)
                return;

            m_anchored_entities.clear();

            select_from(get_world()).exec(
                [this](game_entity e, Tickline::Anchor& anchor)
                {
                    m_anchored_entities[anchor.uid].push_back(e);
                }
            );
        }

        void Update()
        {
            if (m_current_woolang_virtual_machine == nullptr)
                return;

            // 此处调用Tickline的Execute函数！
            wo_push_pointer(m_current_woolang_virtual_machine, this);
            if (wo_invoke_rsfunc(m_current_woolang_virtual_machine, m_externed_execute_func, 1) == nullptr)
            {
                // 有异常发生！将此系统从世界中移除
                jeecs::debug::logfatal("TicklineSystem: '%p' failed with reason: '%s'.",
                    this, wo_get_runtime_error(m_current_woolang_virtual_machine));
                get_world().remove_system<TicklineSystem>();
            }
        }

        void LateUpdate()
        {
            if (m_current_woolang_virtual_machine == nullptr)
                return;
        }
    };
}
