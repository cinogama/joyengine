/*
* JoyECS
* -----------------------------------
* JoyECS is a interesting ecs-impl.
*
*/
#define JE_IMPL
#include "jeecs.hpp"

#include <iostream>

struct position
{
    float x, y, z;
};

struct renderer
{
    float x, y, z;
};

struct rigidbory
{
    float x, y, z;
};


struct example_system :jeecs::game_system
{
    void executing_with_pos_1(const position* pos)
    {
        std::cout << je_clock_time() << std::endl;
    }
    void executing_with_pos_2(const position* pos, const renderer* renderer)
    {
        std::cout << "B: " << pos << std::endl;
    }
    void executing_with_pos_3(const rigidbory* pos, position* renderer)
    {
        std::cout << "C: " << pos << std::endl;
    }
    void executing_with_pos_4(rigidbory* pos, position* renderer)
    {
        std::cout << "D: " << pos << std::endl;
    }

    example_system(jeecs::game_world gw)
        : jeecs::game_system(gw)
    {
        register_system_func(&example_system::executing_with_pos_1);
        register_system_func(&example_system::executing_with_pos_2);
        register_system_func(&example_system::executing_with_pos_3);
        register_system_func(&example_system::executing_with_pos_4);
    }
};

int main()
{
    using namespace jeecs;
    using namespace std;

    game_universe universe = game_universe::create_universe();

    game_world world = universe.create_world();
    world.add_system<example_system>();

    world.add_entity<position>();

    je_clock_sleep_for(12000000000.);
}
