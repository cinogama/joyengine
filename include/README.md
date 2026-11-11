# include/

JoyECS 公共头文件目录 / JoyECS public headers.

## jeecs/

按域拆分的公共头，一域一头（`graphic.hpp` 同时包含 graphic 域的 C API 与 C++ 包装，不再细分）。
Consumers include the specific header(s) they need — do **not** re-unify them into an umbrella header.

**C API**（`JE_API` / `extern "C"` 声明全部在此层）：

`version` `prefix` `core` `logging` `typing` `ecs` `clock` `file` `cache` `animation` `atomic` `module` `debug` `graphic` `gui` `io` `audio` `tilemap`

**C++ 层**（对 C API 的包装）：

`basic` `game` `math` `components` `ray` `entry` `input` `engine`（graphic 域的 C++ 包装在 `graphic.hpp` 内）

新增对外功能时，在对应的域头里暴露 `JE_API` 函数，而不是新建 C++ 类 API；命名与类型规则见根目录 `AGENTS.md`。
When exposing new engine functionality externally, declare a `JE_API` function in the matching domain header instead of a C++ class API; naming conventions live in the root `AGENTS.md`.
