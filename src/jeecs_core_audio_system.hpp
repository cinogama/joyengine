#pragma once

#ifndef JE_IMPL
#   error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#   error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

namespace jeecs
{
    struct AudioUpdatingSystem :game_system
    {
        AudioUpdatingSystem(game_world w)
            : game_system(w)
        {

        }

        void CommitUpdate(jeecs::selector& selector)
        {
            selector.exec([this](Audio::Listener& listener, Transform::Translation& trans)
                {
                    auto velocity = (trans.world_position - listener.last_position) / std::max(deltatime(), 0.0001f);
                    listener.last_position = trans.world_position;

                    audio::listener::update(
                        [&](jeal_listener* listener_data)
                        {
                            listener_data->m_location[0] = trans.world_position.x;
                            listener_data->m_location[1] = trans.world_position.y;
                            listener_data->m_location[2] = trans.world_position.z;

                            auto face = trans.world_rotation * listener.face;
                            auto up = trans.world_rotation * listener.up;

                            listener_data->m_orientation[0][0] = face.x;
                            listener_data->m_orientation[0][1] = face.y;
                            listener_data->m_orientation[0][2] = face.z;
                            listener_data->m_orientation[1][0] = up.x;
                            listener_data->m_orientation[1][1] = up.y;
                            listener_data->m_orientation[1][2] = up.z;

                            listener_data->m_gain = listener.volume;

                            listener_data->m_velocity[0] = velocity.x;
                            listener_data->m_velocity[1] = velocity.y;
                            listener_data->m_velocity[2] = velocity.z;
                        });
                });

            selector.exec([this](Audio::Source& source, Transform::Translation& trans)
                {
                    auto velocity = (trans.world_position - source.last_position) / std::max(deltatime(), 0.0001f);
                    source.last_position = trans.world_position;

                    source.source->update(
                        [&](jeal_source* source_data) 
                        {
                            source_data->m_gain = source.volume;
                            source_data->m_pitch = source.pitch;

                            source_data->m_location[0] = trans.world_position.x;
                            source_data->m_location[1] = trans.world_position.y;
                            source_data->m_location[2] = trans.world_position.z;

                            source_data->m_velocity[0] = velocity.x;
                            source_data->m_velocity[1] = velocity.y;
                            source_data->m_velocity[2] = velocity.z;
                        });
                });

            selector.exec([this](Audio::Source& source, Audio::Playing& playing)
                {
                    if (!playing.buffer.has_resource())
                    {
                        playing.is_playing = false;
                        playing.play = false;
                        if (source.source->get_state() != jeal_state::JE_AUDIO_STATE_STOPPED)
                            source.source->stop();
                    }
                    else
                    {
                        source.source->set_playing_buffer(playing.buffer.get_resource());
                        source.source->update(
                            [&](jeal_source* source_data) 
                            {
                                source_data->m_loop = playing.loop;
                            });

                        if (playing.is_playing && source.source->get_state() == jeal_state::JE_AUDIO_STATE_STOPPED)
                        {
                            playing.is_playing = false;
                            playing.play = false;
                        }
                        else
                        {
                            if (source.source->get_state() != jeal_state::JE_AUDIO_STATE_STOPPED)
                                playing.is_playing = true;

                            if (playing.play && source.source->get_state() != jeal_state::JE_AUDIO_STATE_PLAYING)
                                source.source->play();
                            else if (!playing.play && source.source->get_state() == jeal_state::JE_AUDIO_STATE_PLAYING)
                                source.source->pause();
                        }
                    }
                });
        }
    };
}