#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.hpp"
#endif

// NOTE: 此处用于实现Timeline系统，需要注意的是，Timeline系统与传统意义上的Timeline并无关系
//       而是引擎架设在上层的游戏逻辑和流程控制系统。
//       详情见 Timeline.md


namespace jeecs
{
    namespace Timeline
    {
        struct Anchor
        {
            // TODO;
            // 考虑对象是跟着单例走，还是分类走
        };
    }

    struct TimelineSystem :public game_system
    {
        TimelineSystem(game_world world) :game_system(world)
        {
        }

        void PreUpdate()
        {

        }

        void Update()
        {
        }

        void LateUpdate()
        {
        }
    };
}