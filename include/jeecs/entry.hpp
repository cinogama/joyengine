#pragma once

// ============================================================================
// jeecs/entry.hpp —— 模块入口（C++）：jeecs::entry::module_entry / module_leave —— 注册全部内置组件、基础类型与脚本解析器
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/typing.hpp"
#include "jeecs/components.hpp"

#ifdef __cplusplus
namespace jeecs
{
    namespace entry
    {
        inline void module_entry(jeecs::typing::type_unregister_guard* guard)
        {
            // 0. register built-in components
            using namespace typing;

            // color 会被组件成员声明引用（如 Particle::color_gradient），
            // 先于组件注册，保证生成的 types.wo 中类型声明位于使用之前。
            jeecs::typing::register_type<math::color>(guard, nullptr);

            jeecs::typing::register_type<Transform::LocalPosition>(guard, "Transform::LocalPosition");
            jeecs::typing::register_type<Transform::LocalRotation>(guard, "Transform::LocalRotation");
            jeecs::typing::register_type<Transform::LocalScale>(guard, "Transform::LocalScale");
            jeecs::typing::register_type<Transform::Anchor>(guard, "Transform::Anchor");
            jeecs::typing::register_type<Transform::LocalToWorld>(guard, "Transform::LocalToWorld");
            jeecs::typing::register_type<Transform::LocalToParent>(guard, "Transform::LocalToParent");
            jeecs::typing::register_type<Transform::Translation>(guard, "Transform::Translation");

            jeecs::typing::register_type<UserInterface::Element>(guard, "UserInterface::Element");
            jeecs::typing::register_type<UserInterface::WorldLayout>(guard, "UserInterface::WorldLayout");
            jeecs::typing::register_type<UserInterface::Rotation>(guard, "UserInterface::Rotation");
            jeecs::typing::register_type<UserInterface::Text>(guard, "UserInterface::Text");
            jeecs::typing::register_type<UserInterface::MouseOn>(guard, "UserInterface::MouseOn");

            jeecs::typing::register_type<Renderer::Rendqueue>(guard, "Renderer::Rendqueue");
            jeecs::typing::register_type<Renderer::Shape>(guard, "Renderer::Shape");
            jeecs::typing::register_type<Renderer::Shaders>(guard, "Renderer::Shaders");
            jeecs::typing::register_type<Renderer::Textures2D>(guard, "Renderer::Textures2D");
            jeecs::typing::register_type<Renderer::Textures2DArray>(guard, "Renderer::Textures2DArray");
            jeecs::typing::register_type<Renderer::Color>(guard, "Renderer::Color");

            jeecs::typing::register_type<Animation::Animator>(guard, "Animation::Animator");
            jeecs::typing::register_type<Particle::Emitter>(guard, "Particle::Emitter");

            jeecs::typing::register_type<Camera::FrustumCulling>(guard, "Camera::FrustumCulling");
            jeecs::typing::register_type<Camera::Projection>(guard, "Camera::Projection");
            jeecs::typing::register_type<Camera::OrthoProjection>(guard, "Camera::OrthoProjection");
            jeecs::typing::register_type<Camera::PerspectiveProjection>(guard, "Camera::PerspectiveProjection");
            jeecs::typing::register_type<Camera::Viewport>(guard, "Camera::Viewport");
            jeecs::typing::register_type<Camera::RendToFramebuffer>(guard, "Camera::RendToFramebuffer");
            jeecs::typing::register_type<Camera::Clear>(guard, "Camera::Clear");

            jeecs::typing::register_type<Light2D::TopDown>(guard, "Light2D::TopDown");
            jeecs::typing::register_type<Light2D::Gain>(guard, "Light2D::Gain");
            jeecs::typing::register_type<Light2D::Point>(guard, "Light2D::Point");
            jeecs::typing::register_type<Light2D::Range>(guard, "Light2D::Range");
            jeecs::typing::register_type<Light2D::Parallel>(guard, "Light2D::Parallel");
            jeecs::typing::register_type<Light2D::ShadowOffset>(guard, "Light2D::ShadowOffset");
            jeecs::typing::register_type<Light2D::ShadowBuffer>(guard, "Light2D::ShadowBuffer");
            jeecs::typing::register_type<Light2D::CameraPostPass>(guard, "Light2D::CameraPostPass");
            jeecs::typing::register_type<Light2D::BlockShadow>(guard, "Light2D::BlockShadow");
            jeecs::typing::register_type<Light2D::ShapeShadow>(guard, "Light2D::ShapeShadow");
            jeecs::typing::register_type<Light2D::SpriteShadow>(guard, "Light2D::SpriteShadow");
            jeecs::typing::register_type<Light2D::WallShadow>(guard, "Light2D::WallShadow");
            jeecs::typing::register_type<Light2D::SelfShadow>(guard, "Light2D::SelfShadow");
            jeecs::typing::register_type<Light2D::PerBlockSelfShadow>(guard, "Light2D::PerBlockSelfShadow");

            jeecs::typing::register_type<Physics2D::Scene>(guard, "Physics2D::Scene");

            jeecs::typing::register_type<Physics2D::Rigidbody>(guard, "Physics2D::Rigidbody");
            jeecs::typing::register_type<Physics2D::DynamicBody>(guard, "Physics2D::DynamicBody");
            jeecs::typing::register_type<Physics2D::KinematicBody>(guard, "Physics2D::KinematicBody");
            jeecs::typing::register_type<Physics2D::Bullet>(guard, "Physics2D::Bullet");

            jeecs::typing::register_type<Physics2D::LockTranslationX>(guard, "Physics2D::LockTranslationX");
            jeecs::typing::register_type<Physics2D::LockTranslationY>(guard, "Physics2D::LockTranslationY");
            jeecs::typing::register_type<Physics2D::LockRotation>(guard, "Physics2D::LockRotation");

            jeecs::typing::register_type<Physics2D::LinearVelocity>(guard, "Physics2D::LinearVelocity");
            jeecs::typing::register_type<Physics2D::AngularVelocity>(guard, "Physics2D::AngularVelocity");
            jeecs::typing::register_type<Physics2D::LinearDamping>(guard, "Physics2D::LinearDamping");
            jeecs::typing::register_type<Physics2D::AngularDamping>(guard, "Physics2D::AngularDamping");
            jeecs::typing::register_type<Physics2D::GravityScale>(guard, "Physics2D::GravityScale");

            jeecs::typing::register_type<Physics2D::Collider::Box>(guard, "Physics2D::Collider::Box");
            jeecs::typing::register_type<Physics2D::Collider::Circle>(guard, "Physics2D::Collider::Circle");
            jeecs::typing::register_type<Physics2D::Collider::Capsule>(guard, "Physics2D::Collider::Capsule");
            jeecs::typing::register_type<Physics2D::Collider::Mesh>(guard, "Physics2D::Collider::Mesh");

            jeecs::typing::register_type<Physics2D::Density>(guard, "Physics2D::Density");
            jeecs::typing::register_type<Physics2D::Friction>(guard, "Physics2D::Friction");
            jeecs::typing::register_type<Physics2D::Restitution>(guard, "Physics2D::Restitution");
            jeecs::typing::register_type<Physics2D::IsTrigger>(guard, "Physics2D::IsTrigger");

            jeecs::typing::register_type<Physics2D::Offset::Position>(guard, "Physics2D::Offset::Position");
            jeecs::typing::register_type<Physics2D::Offset::Rotation>(guard, "Physics2D::Offset::Rotation");

            jeecs::typing::register_type<Physics2D::CollisionResult>(guard, "Physics2D::CollisionResult");

            jeecs::typing::register_type<Audio::Source>(guard, "Audio::Source");
            jeecs::typing::register_type<Audio::Listener>(guard, "Audio::Listener");
            jeecs::typing::register_type<Audio::Occluder>(guard, "Audio::Occluder");
            jeecs::typing::register_type<Audio::ReverbZone>(guard, "Audio::ReverbZone");

            jeecs::typing::register_type<Input::VirtualGamepad>(guard, "Input::VirtualGamepad");

            jeecs::typing::register_type<Tilemap::Map>(guard, "Tilemap::Map");
            jeecs::typing::register_type<Tilemap::Wall>(guard, "Tilemap::Wall");
            jeecs::typing::register_type<Tilemap::WallPart>(guard, "Tilemap::WallPart");
            jeecs::typing::register_type<Tilemap::RenderPart>(guard, "Tilemap::RenderPart");

            // 1. register basic types
            jeecs::typing::register_type<basic::file_resource<>>(guard, nullptr);
            jeecs::typing::register_type<math::ivec2>(guard, nullptr);
            typing::register_script_parser<bool>(
                guard,
                [](const bool* v, woort_value value)
                {
                    woort_set_bool(value, *v);
                },
                [](bool* v, woort_value value)
                {
                    *v = woort_bool(value);
                },
                "bool", "");
            auto integer_uniform_parser_c2w = [](const auto* v, woort_value value)
                {
                    woort_set_int(value, (woort_Int)*v);
                };
            auto integer_uniform_parser_w2c = [](auto* v, woort_value value)
                {
                    *v = (typename std::remove_reference<decltype(*v)>::type)woort_int(value);
                };
            typing::register_script_parser<int8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int8", "public alias int8 = int;");
            typing::register_script_parser<int16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int16", "public alias int16 = int;");
            typing::register_script_parser<int32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int32", "public alias int32 = int;");
            typing::register_script_parser<int64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "int64", "public alias int64 = int;");
            typing::register_script_parser<uint8_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint8", "public alias uint8 = int;");
            typing::register_script_parser<uint16_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint16", "public alias uint16 = int;");
            typing::register_script_parser<uint32_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint32", "public alias uint32 = int;");
            typing::register_script_parser<uint64_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                "uint64", "public alias uint64 = int;");

            if constexpr (!std::is_same_v<size_t, uint32_t> && !std::is_same_v<size_t, uint64_t>)
            {
                // In webgl, size_t is a individual type, not a alias of uint32_t or uint64_t.
                static_assert(!std::is_signed_v<size_t>);
                if constexpr (sizeof(size_t) == sizeof(uint32_t))
                {
                    typing::register_script_parser<size_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                        "uint32", "public alias uint32 = int;");
                }
                else if constexpr (sizeof(size_t) == sizeof(uint64_t))
                {
                    typing::register_script_parser<size_t>(guard, integer_uniform_parser_c2w, integer_uniform_parser_w2c,
                        "uint64", "public alias uint64 = int;");
                }
                else
                {
                    // Unknown size_t type? assert and support it if found.
                    static_assert(sizeof(size_t) == sizeof(uint64_t) || sizeof(size_t) == sizeof(uint32_t));
                }
            }

            // Or size_t is same as uint32_t or uint64_t, skip.
            typing::register_script_parser<float>(
                guard,
                [](const float* v, woort_value value)
                {
                    woort_set_float(value, *v);
                },
                [](float* v, woort_value value)
                {
                    *v = woort_float(value);
                },
                "float", "public alias float = real;");
            typing::register_script_parser<double>(
                guard,
                [](const double* v, woort_value value)
                {
                    woort_set_real(value, *v);
                },
                [](double* v, woort_value value)
                {
                    *v = woort_real(value);
                },
                "real", "");

            typing::register_script_parser<jeecs::basic::string>(
                guard,
                [](const jeecs::basic::string* v, woort_value value)
                {
                    woort_set_string(value, v->c_str());
                },
                [](jeecs::basic::string* v, woort_value value)
                {
                    *v = woort_string(value);
                },
                "string", "");

            typing::register_script_parser<std::string>(
                guard,
                [](const std::string* v, woort_value value)
                {
                    woort_set_string(value, v->c_str());
                },
                [](std::string* v, woort_value value)
                {
                    *v = woort_string(value);
                },
                "string", "");

            typing::register_script_parser<UserInterface::Element::alignment>(
                guard,
                [](const UserInterface::Element::alignment* v, woort_value value)
                {
                    woort_set_int(value, *v);
                },
                [](UserInterface::Element::alignment* v, woort_value value)
                {
                    *v = static_cast<UserInterface::Element::alignment>(woort_int(value));
                },
                "UserInterface::Element::alignment",
                "namespace UserInterface::Element\n"
                "{\n"
                "    public enum alignment\n"
                "    {\n"
                "        center  = 0,\n"
                "\n"
                "        left    = 1,\n"
                "        right   = 2,\n"
                "        top     = 4,\n"
                "        bottom  = 8,\n"
                "    }\n"
                "}\n");

            typing::register_script_parser<UserInterface::Element::ratio_unit>(
                guard,
                [](const UserInterface::Element::ratio_unit* v, woort_value value)
                {
                    woort_set_int(value, *v);
                },
                [](UserInterface::Element::ratio_unit* v, woort_value value)
                {
                    *v = static_cast<UserInterface::Element::ratio_unit>(woort_int(value));
                },
                "UserInterface::Element::ratio_unit",
                "namespace UserInterface::Element\n"
                "{\n"
                "    public enum ratio_unit\n"
                "    {\n"
                "        height_unit = 0,\n"
                "\n"
                "        width_unit  = 1,\n"
                "        per_axis    = 2,\n"
                "    }\n"
                "}\n");

            typing::register_script_parser<UserInterface::Text::wrap_mode>(
                guard,
                [](const UserInterface::Text::wrap_mode* v, woort_value value)
                {
                    woort_set_int(value, *v);
                },
                [](UserInterface::Text::wrap_mode* v, woort_value value)
                {
                    *v = static_cast<UserInterface::Text::wrap_mode>(woort_int(value));
                },
                "UserInterface::Text::wrap_mode",
                "namespace UserInterface::Text\n"
                "{\n"
                "    public enum wrap_mode\n"
                "    {\n"
                "        none = 0,\n"
                "        word = 1,\n"
                "    }\n"
                "}\n");

            typing::register_script_parser<Particle::Emitter::shape_type>(
                guard,
                [](const Particle::Emitter::shape_type* v, woort_value value)
                {
                    woort_set_int(value, *v);
                },
                [](Particle::Emitter::shape_type* v, woort_value value)
                {
                    *v = static_cast<Particle::Emitter::shape_type>(woort_int(value));
                },
                "Particle::Emitter::shape_type",
                "namespace Particle::Emitter\n"
                "{\n"
                "    public enum shape_type\n"
                "    {\n"
                "        point   = 0,\n"
                "\n"
                "        sphere  = 1,\n"
                "        circle  = 2,\n"
                "        box     = 3,\n"
                "    }\n"
                "}\n");

            typing::register_script_parser<Particle::Emitter::simulation_space_type>(
                guard,
                [](const Particle::Emitter::simulation_space_type* v, woort_value value)
                {
                    woort_set_int(value, *v);
                },
                [](Particle::Emitter::simulation_space_type* v, woort_value value)
                {
                    *v = static_cast<Particle::Emitter::simulation_space_type>(woort_int(value));
                },
                "Particle::Emitter::simulation_space_type",
                "namespace Particle::Emitter\n"
                "{\n"
                "    public enum simulation_space_type\n"
                "    {\n"
                "        world   = 0,\n"
                "        local   = 1,\n"
                "    }\n"
                "}\n");

            typing::register_script_parser<je_audio_distance_model_t>(
                guard,
                [](const je_audio_distance_model_t* v, woort_value value)
                {
                    woort_set_int(value, (woort_Int)*v);
                },
                [](je_audio_distance_model_t* v, woort_value value)
                {
                    *v = static_cast<je_audio_distance_model_t>(woort_int(value));
                },
                "Audio::Listener::distance_model",
                "namespace Audio::Listener\n"
                "{\n"
                "    public enum distance_model\n"
                "    {\n"
                "        none              = 0,\n"
                "        inverse           = 1,\n"
                "        inverse_clamped   = 2,\n"
                "        linear            = 3,\n"
                "        linear_clamped    = 4,\n"
                "        exponent          = 5,\n"
                "        exponent_clamped  = 6,\n"
                "    }\n"
                "}\n");

            je_towoo_api_update();
        }

        inline void module_leave(jeecs::typing::type_unregister_guard* guard)
        {
            // 0. ungister this module components
            guard->unregister_all_types();
        }
    }
}
#endif // __cplusplus
