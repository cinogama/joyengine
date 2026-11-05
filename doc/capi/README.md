# JoyEngine C-API 文档

C-API 是 JoyEngine 的外部实现与引擎核心的交互接口。出于规避编译防火墙的目的，引擎核心与外界只能通过 C-API 进行交互。所有其他功能都依靠对 C-API 的二次封装来实现。

## 目录结构

### [core/](core/) - 核心接口
引擎初始化、内存管理、构建信息等基础接口。

- `je_core_init` / `je_core_finish` - 引擎初始化与关闭
- `je_mem_alloc` / `je_mem_realloc` / `je_mem_free` - 内存管理
- `je_build_version` / `je_build_commit` - 构建信息
- `je_core_main_script_entry` - 脚本入口

### log - 日志接口
日志输出与回调管理。

- `je_log` - 日志输出
- `je_log_callback_register` / `je_log_callback_unregister` - 回调管理

### [typing/](typing/) - 类型系统
组件和系统类型的注册与查询。

- `je_typing_register` / `je_typing_unregister` / `je_typing_reset` - 类型注册
- `je_typing_info_get_by_*` - 类型查询
- `je_typing_member_register` / `je_typing_script_parser_register` / `je_typing_system_updater_register` - 成员注册
- `je_towoo_*` - Woolang 脚本系统支持

### [ecs/](ecs/) - ECS 系统
实体组件系统的核心接口，包括 Universe、World、Entity 管理。

- `je_ecs_universe_*` - Universe 生命周期与任务管理
- `je_ecs_world_*` - World 管理与实体操作
- `je_arch_*` - 原型（Archetype）遍历

### [clock/](clock/) - 时钟接口
时间管理相关接口。

- `je_clock_time` / `je_clock_time_stamp` - 时间获取
- `je_clock_sleep_*` - 线程休眠
- `je_uid_generate` - UID 生成

### [file/](file/) - 文件系统
文件读写与缓存管理。

- `je_file_*` - 文件操作
- `je_fimg_*` - 资源镜像（打开、查询、打包与逐文件压缩）
- `je_cache_file_*` / `je_cache_binary_*` - 缓存文件与二进制缓存文档

### [graphic/](graphic/) - 图形接口
图形渲染相关的底层接口。

- `je_graphic_thread_start` / `je_graphic_thread_terminate` - 图形线程
- `je_graphic_texture_*` / `je_graphic_shader_*` / `je_graphic_vertex_*` / `je_graphic_framebuf_*` / `je_graphic_uniformbuf_*` - 资源管理
- `je_graphic_cmd_*` / `je_graphic_pipeline_*` / `je_graphic_sampler_*` - 新渲染 API（命令缓冲区/pipeline/采样器），见 [render_api_v2.md](graphic/render_api_v2.md)
- `je_graphic_uhost_*` - 图形上下文管理
- `je_graphic_using_*_apis` - 图形 API 选择
- `je_font_*` - 字体管理

### [gui/](gui/) - GUI 接口
图形用户界面支持。

- `je_gui_backend_init` / `je_gui_backend_update` / `je_gui_backend_shutdown` - GUI 生命周期
- `je_gui_backend_font_set` - 字体设置

### [io/](io/) - 输入输出
键盘、鼠标、手柄等输入设备管理。

- `je_io_key_*` / `je_io_mouse_*` - 键盘鼠标输入
- `je_io_window_*` - 窗口管理
- `je_io_gamepad_*` - 手柄支持

### [audio/](audio/) - 音频接口
音频播放与效果处理。

- `je_audio_source_*` - 音频源管理
- `je_audio_buffer_*` - 音频数据
- `je_audio_effect_*` - 音频效果
- `je_audio_filter_*` - 音频滤波器
- `je_audio_listener_*` - 监听者

### [module/](module/) - 模块加载
动态库加载与函数调用。

- `je_module_load` / `je_module_func` / `je_module_unload`

### [atomic/](atomic/) - 原子操作
原子类型与操作（通过宏生成）。

## 使用说明

所有 C-API 函数均以 `JE_API` 宏导出，声明在 `include/jeecs.hpp` 中。

命名格式为 `je_<domain>_<subject...>_<action>`：

- `domain` 是子系统，必须最先出现，例如 `ecs`、`tilemap`、`graphic`、`audio`、`io`、`file`、`cache`、`typing`、`debug`。
- `subject` 是对象/模块路径，从大到小排列，例如 `ecs_world_entity`、`cache_file`、`graphic_texture_2d`。
- `action` 收尾，例如 `je_cache_file_load`、`je_tilemap_layer_texture_get`。
- 纯名词访问器保持名词形式（如 `je_tilemap_layer_z`）；谓词使用 `_is_*`（如 `je_io_gamepad_is_active`）。
- `je_gui_*` 与 `wojeapi_*` 是 Woolang 绑定/ImGui 中间层，不遵循此命名约定。

