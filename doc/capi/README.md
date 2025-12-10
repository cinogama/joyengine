# JoyEngine C-API 文档

C-API 是 JoyEngine 的外部实现与引擎核心的交互接口。出于规避编译防火墙的目的，引擎核心与外界只能通过 C-API 进行交互。所有其他功能都依靠对 C-API 的二次封装来实现。

## 目录结构

### [core/](core/) - 核心接口
引擎初始化、内存管理、构建信息等基础接口。

- `je_init` / `je_finish` - 引擎初始化与关闭
- `je_mem_alloc` / `je_mem_realloc` / `je_mem_free` - 内存管理
- `je_build_version` / `je_build_commit` - 构建信息
- `je_main_script_entry` - 脚本入口

### [log/](log/) - 日志接口
日志输出与回调管理。

- `je_log` - 日志输出
- `je_log_register_callback` / `je_log_unregister_callback` - 回调管理

### [typing/](typing/) - 类型系统
组件和系统类型的注册与查询。

- `je_typing_register` / `je_typing_unregister` / `je_typing_reset` - 类型注册
- `je_typing_get_info_by_*` - 类型查询
- `je_register_member` / `je_register_script_parser` / `je_register_system_updater` - 成员注册
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

- `jeecs_file_*` - 文件操作
- `jeecs_file_image_*` - 文件镜像打包
- `jeecs_*_cache_file` - 缓存文件管理

### [graphic/](graphic/) - 图形接口
图形渲染相关的底层接口。

- `jegl_start_graphic_thread` / `jegl_terminate_graphic_thread` - 图形线程
- `jegl_load_*` / `jegl_create_*` / `jegl_close_*` - 资源管理
- `jegl_rchain_*` - 渲染链操作
- `jegl_uhost_*` - 图形上下文管理
- `jegl_using_*_apis` - 图形 API 选择
- `je_font_*` - 字体管理

### [gui/](gui/) - GUI 接口
图形用户界面支持。

- `jegui_init_basic` / `jegui_update_basic` / `jegui_shutdown_basic` - GUI 生命周期
- `jegui_set_font` - 字体设置

### [io/](io/) - 输入输出
键盘、鼠标、手柄等输入设备管理。

- `je_io_*_key_*` / `je_io_*_mouse_*` - 键盘鼠标输入
- `je_io_*_window_*` - 窗口管理
- `je_io_*_gamepad_*` - 手柄支持

### [audio/](audio/) - 音频接口
音频播放与效果处理。

- `jeal_create_source` / `jeal_*_source` - 音频源管理
- `jeal_create_buffer` / `jeal_load_buffer_wav` - 音频数据
- `jeal_create_effect_*` - 音频效果
- `jeal_create_filter` - 音频滤波器
- `jeal_*_listener` - 监听者

### [module/](module/) - 模块加载
动态库加载与函数调用。

- `je_module_load` / `je_module_func` / `je_module_unload`

### [atomic/](atomic/) - 原子操作
原子类型与操作（通过宏生成）。

## 使用说明

所有 C-API 函数均以 `JE_API` 宏导出，声明在 `include/jeecs.hpp` 中。

