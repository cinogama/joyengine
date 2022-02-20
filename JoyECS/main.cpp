/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>

struct ecs_system
{
    enum dependence_type : uint8_t
    {
        READ_FROM_LAST_FRAME,
        WRITE,
        READ_AFTER_WRITE,

        NONE,
    };
    enum sequence
    {
        CAN_HAPPEND_SAME_TIME,
        ONLY_HAPPEND_BEFORE,
        ONLY_HAPPEND_AFTER,
        UNABLE_DETERMINE,
    };

    using dependences_t = std::unordered_map<jeecs::typing::typeid_t, dependence_type>;

    dependences_t m_dependence_list;

    sequence check_dependence(const dependences_t& depend) const
    {
        bool not_same_time_flag = false;
        sequence result = sequence::CAN_HAPPEND_SAME_TIME;

        auto decided_depend_seq = [&](sequence seq) {
            if (seq == sequence::UNABLE_DETERMINE || result == sequence::UNABLE_DETERMINE)
                return sequence::UNABLE_DETERMINE;
            if (result == sequence::CAN_HAPPEND_SAME_TIME)
                return seq;
            if (result == sequence::ONLY_HAPPEND_BEFORE)
            {
                if (seq == sequence::ONLY_HAPPEND_AFTER)
                    return sequence::UNABLE_DETERMINE;
                return sequence::ONLY_HAPPEND_BEFORE;
            }
            if (result == sequence::ONLY_HAPPEND_AFTER)
            {
                if (seq == sequence::ONLY_HAPPEND_BEFORE)
                    return sequence::UNABLE_DETERMINE;
                return sequence::ONLY_HAPPEND_AFTER;
            }
            return sequence::UNABLE_DETERMINE;
        };

        for (auto [depend_tid, depend_type] : m_dependence_list)
        {
            // Check dependence
            auto fnd = depend.find(depend_tid);

            if (fnd == depend.end())
            {
                // NO REQUIRE, JUST GOON..
            }
            else
            {
                auto aim_require_type = fnd->second;

                if ((depend_type == dependence_type::NONE
                    || aim_require_type == dependence_type::NONE)
                    && depend_type != aim_require_type)
                {
                    // These two set will not meet at same time, just skip them
                    return sequence::CAN_HAPPEND_SAME_TIME;
                }
                else if (aim_require_type == dependence_type::NONE)
                {
                    result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                }
                else if (aim_require_type == dependence_type::WRITE)
                {
                    if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                        result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                    else if (depend_type == dependence_type::WRITE)
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME); //XX
                    else if (depend_type == dependence_type::READ_AFTER_WRITE)
                        result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                    else
                        assert(false);
                }
                else if (aim_require_type == dependence_type::READ_FROM_LAST_FRAME)
                {
                    if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    else if (depend_type == dependence_type::WRITE)
                        result = decided_depend_seq(sequence::ONLY_HAPPEND_AFTER);
                    else if (depend_type == dependence_type::READ_AFTER_WRITE)
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    else
                        assert(false);
                }
                else if (aim_require_type == dependence_type::READ_AFTER_WRITE)
                {
                    if (depend_type == dependence_type::READ_FROM_LAST_FRAME)
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    else if (depend_type == dependence_type::WRITE)
                        result = decided_depend_seq(sequence::ONLY_HAPPEND_BEFORE);
                    else if (depend_type == dependence_type::READ_AFTER_WRITE)
                        result = decided_depend_seq(sequence::CAN_HAPPEND_SAME_TIME);
                    else
                        assert(false);
                }
                else
                    assert(false);
            }
        }
        return result;
    }
};

class ecs_world
{
    JECS_DISABLE_MOVE_AND_COPY(ecs_world);

    jeecs::arch::arch_manager _m_arch_manager;
    std::vector<ecs_system*> _m_registed_system;
    std::list<std::list<ecs_system*>> _m_execute_seq;

public:
    ecs_world() = default;
    void build_dependence_graph()
    {
        std::list<ecs_system*>dependence_system_chain;

        // Walk throw all registered system. build dependence chain.
        for (ecs_system* registed_system : _m_registed_system)
        {
            std::list<ecs_system*>::const_iterator insert_aim_place
                = dependence_system_chain.cbegin();
            bool append_at_spcify_place = false;
            for (auto node = dependence_system_chain.cbegin();
                node != dependence_system_chain.cend();
                ++node)
            {
                auto seq = registed_system->check_dependence((*node)->m_dependence_list);

                if (seq == ecs_system::sequence::ONLY_HAPPEND_BEFORE)
                {
                    append_at_spcify_place = true;
                    break;
                }
                else if (seq == ecs_system::sequence::UNABLE_DETERMINE)
                {
                    append_at_spcify_place = true;

                    // TODO: give warning
                    printf("[warning]...");
                    break;
                }
                else if (seq == ecs_system::sequence::ONLY_HAPPEND_AFTER)
                {
                    insert_aim_place = node;
                    ++insert_aim_place;
                }
                else
                    insert_aim_place = node;
            }
            if (append_at_spcify_place)
                dependence_system_chain.insert(insert_aim_place, registed_system);
            else
                dependence_system_chain.push_back(registed_system);
        }

        std::list<std::list<ecs_system*>> output_layer;

        // Begin to merge chain to layer..
        for (auto* esystem : dependence_system_chain)
        {
            if (output_layer.size())
            {
                for (auto* last_layer_system : output_layer.back())
                {
                    ecs_system::sequence seq = esystem->check_dependence(last_layer_system->m_dependence_list);
                    switch (seq)
                    {
                    case ecs_system::CAN_HAPPEND_SAME_TIME:
                        // Do nothing
                        break;
                    case ecs_system::ONLY_HAPPEND_AFTER:
                        // Create new layer to place this system;
                        goto jmp_here_to_insert_next_system;
                        break;
                    case ecs_system::ONLY_HAPPEND_BEFORE:
                    case ecs_system::UNABLE_DETERMINE:
                        // ERROR HAPPEND
                         // TODO: give warning
                        printf("[warning]...");
                        break;
                    default:
                        assert(false);
                        break;
                    }
                }

                // OK, Can happend at same time,
                output_layer.back().push_back(esystem);
            }
            else
            {
                // Only new layer can be here
            jmp_here_to_insert_next_system:;
                output_layer.push_back({ esystem });
            }

        }

        _m_execute_seq = std::move(output_layer);
    }

    void display_execute_seq()
    {
        std::cout << "This world has " << _m_registed_system.size() << " system(s) to work." << std::endl;

        size_t count = 1;
        for (auto& systems : _m_execute_seq)
        {
            std::cout << "seq " << (count++) << std::endl;
            for (auto* sys : systems)
            {
                std::cout << "    system " << sys << std::endl;
                for (auto& [dep_id, dep_type] : sys->m_dependence_list)
                {
                    std::cout << "        ";
                    switch (dep_type)
                    {
                    case ecs_system::dependence_type::NONE:
                        std::cout << "EXCEPT"; break;
                    case ecs_system::dependence_type::READ_FROM_LAST_FRAME:
                        std::cout << "READ(LF)"; break;
                    case ecs_system::dependence_type::READ_AFTER_WRITE:
                        std::cout << "READ(AF)"; break;
                    case ecs_system::dependence_type::WRITE:
                        std::cout << "WRITE"; break;
                    default:
                        assert(false);
                    }
                    std::cout << " " << jeecs::typing::type_info::of(dep_id)->m_typename << std::endl;
                }
            }
        }

    }

    // TEST
    void regist_system(ecs_system* sys)
    {
        _m_registed_system.push_back(sys);
    }
};

struct custom
{
    int random_value;
    custom()
    {
        srand(time(0));
        random_value = rand();

        std::cout << "random_value : " << random_value << std::endl;
    }
};

int main()
{
    using namespace jeecs;

    ecs_system* sys1 = new ecs_system();
    sys1->m_dependence_list[typing::type_info::of<int>()->m_id]
        = ecs_system::dependence_type::READ_FROM_LAST_FRAME;
    sys1->m_dependence_list[typing::type_info::of<custom>()->m_id]
        = ecs_system::dependence_type::WRITE;
    sys1->m_dependence_list[typing::type_info::of<std::string>()->m_id]
        = ecs_system::dependence_type::NONE;

    ecs_system* sys2 = new ecs_system();
    sys2->m_dependence_list[typing::type_info::of<int>()->m_id]
        = ecs_system::dependence_type::WRITE;
    sys2->m_dependence_list[typing::type_info::of<custom>()->m_id]
        = ecs_system::dependence_type::READ_FROM_LAST_FRAME;
    sys2->m_dependence_list[typing::type_info::of<std::string>()->m_id]
        = ecs_system::dependence_type::READ_FROM_LAST_FRAME;

    ecs_system* sys3 = new ecs_system();
    sys3->m_dependence_list[typing::type_info::of<custom>()->m_id]
        = ecs_system::dependence_type::WRITE;

    ecs_system* sys4 = new ecs_system();
    sys4->m_dependence_list[typing::type_info::of<int>()->m_id]
        = ecs_system::dependence_type::READ_AFTER_WRITE;
    sys4->m_dependence_list[typing::type_info::of<custom>()->m_id]
        = ecs_system::dependence_type::READ_AFTER_WRITE;
    sys4->m_dependence_list[typing::type_info::of<std::string>()->m_id]
        = ecs_system::dependence_type::READ_AFTER_WRITE;

    ecs_system* sys5 = new ecs_system();
    sys5->m_dependence_list[typing::type_info::of<int>()->m_id]
        = ecs_system::dependence_type::WRITE;
    sys5->m_dependence_list[typing::type_info::of<custom>()->m_id]
        = ecs_system::dependence_type::READ_AFTER_WRITE;

    ecs_world ew;
    ew.regist_system(sys1);
    ew.regist_system(sys2);
    ew.regist_system(sys3);
    ew.regist_system(sys4);
    ew.regist_system(sys5);

    ew.build_dependence_graph();
    ew.display_execute_seq();

    /*
    auto* tinfo = jeecs::typing::type_info::of<jeecs::typing::type_info>("type_info");

    arch::arch_manager am;
    arch::command_buffer cb;

    auto entity_1 = am.create_an_entity_with_component({ typing::type_info::id<int>(), typing::type_info::id<std::string>() });
    cb.append_component(entity_1, typing::type_info::of<custom>());
    cb.update();
    */

}
