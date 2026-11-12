# JoyEngine C-API 文档

C-API 是 JoyEngine 的外部实现与引擎核心的交互接口。出于规避编译防火墙的目的，引擎核心与外界只能通过 C-API 进行交互。所有其他功能都依靠对 C-API 的二次封装来实现。

## 目录结构

### [core/](core/) - 核心接口
引擎初始化、内存管理、构建信息等基础接口。

- `je_core_init` / `je_core_finish` - 引擎初始化与关闭
- `je_mem_alloc` / `je_mem_realloc` / `je_mem_free` - 内存管理
- `je_build_version` / `je_build_commit` - 构建信息
- `je_core_main_script_entry` - 脚本入口

### [logging/](logging/) - 日志接口
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
- `je_ecs_requirements_collect` / `je_ecs_world_collection_update` - 切片查询需求收集与刷新
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

### [animation/](animation/) - 动画（轨道/关键帧）
动画集资源（v2 轨道/关键帧模型）、采样与实体播放控制。

- `je_animation_open` / `je_animation_close` - 打开与关闭动画集
- `je_animation_action_find` - 按名称查找动作
- `je_animation_cache_open_or_create` - 打开或生成动画集缓存
- `je_animation_sample_buffer_create` / `_destroy` / `_clear` - 采样累加缓冲管理
- `je_animation_action_sample` - 按权重采样动作（支持多动作混合）
- `je_animation_sample_buffer_apply` - 把采样结果写入实体
- `je_animation_entity_play` / `_crossfade` / `_stop` / `_pause` / `_resume` - 实体播放控制（crossfade 淡入淡出）
- `je_animation_entity_is_playing` / `_action_get` / `_duration_get` / `_progress_get` - 播放状态查询
- `je_animation_entity_events_get` - 获取本帧触发的动画事件

### [graphic/](graphic/) - 图形接口
图形渲染相关的底层接口。

- `je_graphic_thread_start` / `je_graphic_thread_terminate` - 图形线程
- `je_graphic_texture_*` / `je_graphic_shader_*` / `je_graphic_vertex_*` / `je_graphic_framebuf_*` / `je_graphic_uniformbuf_*` - 资源管理
- `je_graphic_cmd_*` / `je_graphic_pipeline_*` / `je_graphic_sampler_*` - 新渲染 API（命令缓冲区/pipeline/采样器），见 [render_api_v2.md](graphic/render_api_v2.md)
- `je_graphic_cmd_update_*` / `je_graphic_cmd_upload_texture_*` / `je_graphic_branch_new_update_command` - 资源更新（本帧内容 + 持久纹理上传），见 [render_api_v2.md](graphic/render_api_v2.md)
- `je_graphic_branch_*` / `je_graphic_uhost_*` / `je_graphic_sync_*` - 绘制组、图形上下文与同步
- `je_graphic_using_*_apis` - 图形 API 选择
- `je_image_texture_*` - 图片纹理缓存（颜色空间 + 解码后的像素）
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
- `je_audio_cache_open_or_create` - 打开或生成音频缓存
- `je_audio_shared_buffer_mark_outdated` - 失效内存共享的音频缓冲
- `je_audio_effect_*` - 音频效果
- `je_audio_filter_*` - 音频滤波器
- `je_audio_listener_*` - 监听者

### [tilemap/](tilemap/) - 瓦片地图
地图/瓦片集文档、图层、瓦片、源与逐格属性的读写。

- `je_tilemap_map_*` / `je_tilemap_layer_*` / `je_tilemap_tile_*` - 地图、图层与瓦片操作
- `je_tilemap_tileset_*` / `je_tileset_*` - 瓦片集与瓦片集缓存
- `je_tilemap_cache_open_or_create` - 打开或生成地图缓存

### [module/](module/) - 模块加载
动态库加载与函数调用。

- `je_module_load` / `je_module_func` / `je_module_unload`

## 使用说明

所有 C-API 函数均以 `JE_API` 宏导出，按领域声明在 `include/jeecs/` 目录下的各头文件中（如 `ecs/` 文档对应 `include/jeecs/ecs.hpp`）；`include/jeecs.hpp` 仅是迁移提示存根。

命名格式为 `je_<domain>_<subject...>_<action>`：

- `domain` 是子系统，必须最先出现，例如 `ecs`、`tilemap`、`graphic`、`audio`、`io`、`file`、`cache`、`typing`、`debug`。
- `subject` 是对象/模块路径，从大到小排列，例如 `ecs_world_entity`、`cache_file`、`graphic_texture_2d`。
- `action` 收尾，例如 `je_cache_file_load`、`je_tilemap_layer_texture_get`。
- 纯名词访问器保持名词形式（如 `je_tilemap_layer_z`）；谓词使用 `_is_*`（如 `je_io_gamepad_is_active`）。
- `je_gui_*` 与 `wojeapi_*` 是 Woolang 绑定/ImGui 中间层，不遵循此命名约定。

### 类型命名

类型声明（`typedef struct/enum/union`、类型别名）统一为 `je_<domain>_<subject...>_t`，并为每个规范类型提供 `je_<DomainSubject>` 大驼峰别名，二者完全等价：

- `je_game_entity_t` / `je_GameEntity`
- `je_gl_shader_t` / `je_GlShader`
- `je_audio_source_t` / `je_AudioSource`
- `je_tilemap_document_t` / `je_TilemapDocument`

具名嵌套类型（枚举、结构体、union、别名）一律提升为全局类型，不使用 `Owner::Nested` 形式；拆分出的未作用域枚举常量使用 `JE_<DOMAIN>_...` 前缀（如 `JE_GL_TEXTURE_FORMAT_RGBA`、`JE_AUDIO_EFFECT_CHORUS_WAVEFORM_SINUSOID`）。

