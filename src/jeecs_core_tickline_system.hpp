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

            static void JERefRegsiter()
            {
                typing::register_member(&Anchor::uid, "uid");
            }
        };
    }
    struct TicklineSystem :public game_system
    {
        struct TicklineGlobalResourceHolder
        {
            // 此类型用于储存全局资源实例，包括原始Tickline控制器虚拟机实例，
            // 所有的TicklineSystem实例都需要从此处复制产生新的虚拟机实例
            // 并共享全局区数据。

            wo_vm _m_global_tickline_vm;

            TicklineGlobalResourceHolder()
                : _m_global_tickline_vm(nullptr)
            {

            }

            wo_vm borrow_vm()
            {
                if (_m_global_tickline_vm != nullptr)
                    return wo_borrow_vm(_m_global_tickline_vm);
                return nullptr;
            }

            static TicklineGlobalResourceHolder& instance()
            {
                static TicklineGlobalResourceHolder _instance;
                return _instance;
            }
        };

        wo_vm _m_vm;

        std::unordered_map<typing::uid_t, game_entity> _m_anchored_entities;

        TicklineSystem(game_world world) :game_system(world)
        {
            _m_vm = TicklineGlobalResourceHolder::instance().borrow_vm();
            if (_m_vm == nullptr)
                jeecs::debug::logfatal("Failed to create new tickline system instance: create vm-state failed.");
        }

        void PreUpdate()
        {
            // PreUpdate阶段，收集Anchor和实体，建立映射
            _m_anchored_entities.clear();

            select_from(get_world()).exec(
                [this](game_entity e, Tickline::Anchor& anchor)
                {
                    _m_anchored_entities[anchor.uid] = e;
                }
            );
        }

        void Update()
        {
        }

        void LateUpdate()
        {
        }
    };
}