#pragma once

#ifndef JE_IMPL
#error JE_IMPL must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#ifndef JE_ENABLE_DEBUG_API
#error JE_ENABLE_DEBUG_API must be defined, please check `jeecs_core_systems_and_components.cpp`
#endif
#include "jeecs.hpp"

namespace jeecs
{
    using namespace Audio;
    using namespace Transform;

    using namespace slice_requirement;

    struct AudioUpdatingSystem : game_system
    {
        AudioUpdatingSystem(game_world w)
            : game_system(w)
        {
        }

        void CommitUpdate()
        {
            for (auto&& [listener, trans] : query_view<Listener&, Translation&>())
            {
                auto velocity =
                    (trans.world_position - listener.last_position) / std::max(deltatime(), 0.0001f);

                listener.last_position = trans.world_position;

                const auto& current_position = trans.world_position;
                const auto& current_rotation = trans.world_rotation;

                audio::listener::update(
                    [&current_position, &current_rotation, &listener, &velocity](
                        jeal_listener* listener_data)
                    {
                        listener_data->m_location[0] = current_position.x;
                        listener_data->m_location[1] = current_position.y;
                        listener_data->m_location[2] = current_position.z;

                        auto face = current_rotation * listener.face;
                        auto up = current_rotation * listener.up;

                        listener_data->m_forward[0] = face.x;
                        listener_data->m_forward[1] = face.y;
                        listener_data->m_forward[2] = face.z;
                        listener_data->m_upward[0] = up.x;
                        listener_data->m_upward[1] = up.y;
                        listener_data->m_upward[2] = up.z;

                        listener_data->m_gain = listener.volume;

                        listener_data->m_velocity[0] = velocity.x;
                        listener_data->m_velocity[1] = velocity.y;
                        listener_data->m_velocity[2] = velocity.z;
                    });
            }
            for (auto&& [source, trans] : query_view<Source&, Translation&>())
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
            }
            for (auto&& [source, playing] : query_view<Source&, Playing&>())
            {
                if (!playing.buffer.has_resource())
                {
                    playing.state = Audio::Playing::play_state::STOPPED;
                    playing.play = false;
                    if (source.source->get_state() != jeal_state::JE_AUDIO_STATE_STOPPED)
                        source.source->stop();
                }
                else
                {
                    // Update source buffer.
                    bool buffer_updated = source.source->set_playing_buffer(playing.buffer.get_resource().value());
                    if (playing.state == Audio::Playing::play_state::STOPPED)
                    {
                        if (playing.play)
                        {
                            // Request to play.
                            source.source->update(
                                [&](jeal_source* source_data)
                                {
                                    source_data->m_loop = playing.loop;
                                });

                            if (playing.loop)
                                playing.state = Audio::Playing::play_state::LOOPED_PLAYING;
                            else
                                playing.state = Audio::Playing::play_state::PLAYING;

                            source.source->play();
                        }
                    }
                    else
                    {
                        // Update loop if needed.
                        if ((playing.state == Audio::Playing::play_state::LOOPED_PLAYING) != playing.loop)
                        {
                            source.source->update(
                                [&](jeal_source* source_data)
                                {
                                    source_data->m_loop = playing.loop;
                                });

                            if (playing.loop)
                                playing.state = Audio::Playing::play_state::LOOPED_PLAYING;
                            else
                                playing.state = Audio::Playing::play_state::PLAYING;
                        }

                        switch (source.source->get_state())
                        {
                        case jeal_state::JE_AUDIO_STATE_STOPPED:
                            if (!buffer_updated)
                            {
                                // Source has been stopped.
                                playing.state = Audio::Playing::play_state::STOPPED;
                                playing.play = false;
                                break;
                            }
                            // Buffer updated during playing, if `play`, play it immediately.
                            [[fallthrough]];
                        case jeal_state::JE_AUDIO_STATE_PAUSED:
                            if (playing.play)
                                source.source->play();
                            break;
                        case jeal_state::JE_AUDIO_STATE_PLAYING:
                            if (!playing.play)
                                source.source->pause();
                            break;
                        default:
                            abort(); // Unknown state.
                        }
                    }
                }
            }
        }
    };
}