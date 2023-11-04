#pragma once

#ifndef JE_IMPL
#   define JE_IMPL
#   define JE_ENABLE_DEBUG_API
#   include "jeecs.h"
#endif

namespace jeecs
{
    struct AudioUpdatingSystem :game_system
    {
        AudioUpdatingSystem(game_world w)
            : game_system(w)
        {

        }

        void CommitUpdate()
        {
            select_from(get_world());
            
            select().exec([this](Audio::Listener& listener, Transform::Translation& trans) 
                {
                    audio::listener::set_position(trans.world_position);
                    audio::listener::set_direction(trans.world_rotation);
                    audio::listener::set_volume(listener.volume);

                    auto velocity = (trans.world_position - listener.last_position) / std::max(deltatime(), 0.0001f);
                    listener.last_position = trans.world_position;
                    audio::listener::set_velocity(velocity);
                });
            select().exec([this](Audio::Source& source, Transform::Translation& trans)
                {
                    source.source->set_position(trans.world_position);
                    source.source->set_pitch(source.pitch);
                    source.source->set_volume(source.volume);

                    auto velocity = (trans.world_position - source.last_position) / std::max(deltatime(), 0.0001f);
                    source.last_position = trans.world_position;
                    source.source->set_velocity(velocity);
                });
            select().exec([this](Audio::Source& source, Audio::Playing& playing)
                {
                    if (!playing.buffer.has_resource())
                    {
                        playing.is_playing = false;
                        playing.play = false;
                        if (source.source->get_state() != jeal_state::STOPPED)
                            source.source->stop();
                    }
                    else
                    {
                        source.source->set_playing_buffer(playing.buffer.get_resource());
                        source.source->set_loop(playing.loop);

                        if (playing.is_playing && source.source->get_state() == jeal_state::STOPPED)
                        {
                            playing.is_playing = false;
                            playing.play = false;
                        }
                        else
                        {
                            if (source.source->get_state() != jeal_state::STOPPED)
                                playing.is_playing = true;

                            if (playing.play && source.source->get_state() != jeal_state::PLAYING)
                                source.source->play();
                            else if (!playing.play && source.source->get_state() == jeal_state::PLAYING)
                                source.source->pause();
                        }
                    }
                });
        }
    };
}