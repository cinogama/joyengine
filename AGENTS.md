# AGENTS.md

JoyEngineECS (JoyECS) — an ECS-architecture game engine: C++20 core (`joyecs` library + `jedriver` executable) with Woolang (`.wo`) as the scripting language. The editor and most high-level engine features are themselves written in Woolang. Docs (README, `doc/`) are bilingual Chinese/English.

## Layout

- `src/` — engine core C++ (`jeecs_*.cpp/.hpp`); graphic API impls `jeecs_graphic_api_impl_{dx11,opengl3,vk120,metal,none}.cpp` with matching imgui backends; core systems in `jeecs_core_*_system.hpp`.
- `include/jeecs.hpp` — the single large public header (~13.8k lines). All C-API (`JE_API`, `extern "C"`) declarations live here.
- `driver/` — `jedriver` executable entry (`main.cpp` → `jeecs::game_engine_context::loop()`).
- `je/` — engine-side Woolang runtime scripts (typeinfo/ECS/gui/towoo bindings; `extern("libjoyecs", "func")` binds C-API into Woolang).
- `builtin/` — built-in Woolang content shipped with the engine, including the whole editor (`builtin/editor/main.wo`).
- `pkg/` — woolang packages installed by baozi; dependencies declared in root `baozi.toml` (some sources under `3rd/pkg`).
- `module/` — optional native engine modules; directory named `<name>.je4module` with a `CMakeLists.txt`, auto-discovered and static-linked.
- `3rd/` — git submodules (self-hosted at `git.cinogama.net` — run `git submodule update --init --recursive` after clone) plus `3rd/pkg` woolang package sources.
- `script/prebuild/` — Woolang prebuild script executed automatically during CMake configure (shader-wrap generation etc.).
- `doc/capi/` — C-API reference; read before changing any core boundary.
- `build/` — CMake binary/output dir; `out/` is git-ignored scratch. `android/`, `ios/`, `webgl/` hold platform projects.

## Build

Requires a C++20 compiler, CMake ≥ 3.17.2, and `baozi` on PATH (`baozi.bat` on Windows). CMake configure automatically runs `baozi install` and `baozi run ./prebuild.wo` — both must succeed or configure fails. Set `-DJE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD=OFF` to skip package install.

Windows (matches CI and CMakeSettings presets — DX11 + OpenGL + Vulkan, shared core, static pkgs):

```shell
git submodule update --init --recursive
cmake -B build -A x64 -DJE4_ENABLE_DX11_GAPI=ON -DJE4_ENABLE_OPENGL330_GAPI=ON -DJE4_ENABLE_VULKAN120_GAPI=ON
cmake --build build --config=RELWITHDEBINFO --target jedriver --parallel
```

Key CMake options:
- Graphic API toggles: `JE4_ENABLE_{DX11,OPENGL330,OPENGLES300,WEBGL20,VULKAN120,METAL}_GAPI`. OpenGL 3.3, OpenGLES 3.0 and WebGL 2.0 are mutually exclusive (ImGui limitation); DX11/Vulkan/Metal combine freely with them.
- `JE4_STATIC_LINK_MODULE_AND_PKGS`, `JE4_BUILD_SHARED_CORE`, `JE4_ENABLE_SHADER_WRAP_GENERATOR` (glslang + spirv-cross), `JE4_ENABLE_WIN32_CONSOLE`.
- `jedriver` is a WIN32 GUI app (no console) unless `JE4_ENABLE_WIN32_CONSOLE=ON`.

There is no unit-test infrastructure; `script/prebuild/test/test_prebuild.wo` only tests the prebuild script. CI (`.gitlab-ci.yml`) is the build-correctness gate for Linux/macOS/Windows.

## Architecture rules

- The engine core interacts with the outside only through the C-API to avoid compile firewalls — everything else is a wrapper over it. New engine functionality exposed externally must be a `JE_API` function in `include/jeecs.hpp`, not a C++ class API.
- C-API prefix conventions: `je_` core/ECS, `jegl_` graphic, `jegui_` GUI, `jeal_` audio, `je_io_` input/window, `je_typing_`/`je_towoo_` type system. Keep them.
- Graphic work goes through the abstraction in `jeecs_graphic_api_interface.hpp`; a new backend means a new `jeecs_graphic_api_impl_*.cpp` plus an imgui backend file.
- openal-soft must stay a **shared** library (LGPL compliance) — never static-link it.
- Woolang ↔ C++: C side implements `wojeapi_*` functions (see `src/jeecs_woolang_api.cpp`); Woolang side declares them with `extern("libjoyecs", ...)` (see `je/typeinfo.wo`).

## Woolang work

Use the locally available skills `woolang-script` (writing/fixing `.wo` code), `woolang-baozi` (package management), and `woolang-c-api` (C API / native module integration) for `.wo` and binding work. Packages are managed with baozi (`baozi.toml`, lockfile `baozi.lock.toml`); `no_compile = true` at root — this repo is not itself compiled as a woolang package.

## Conventions

- Sources are UTF-8; MSVC builds pass `/utf-8`.
- Commits follow Conventional Commits with scopes, e.g. `fix(ui): ...`, `feat(graphic): ...`, `refactor(editor): ...`.
