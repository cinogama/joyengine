# jegl_uhost_get_or_create_for_universe

## 函数签名

```c
JE_API jeecs::graphic_uhost* jegl_uhost_get_or_create_for_universe(
    void* universe,
    const jegl_interface_config* config);
```

## 描述

获取或创建指定 Universe 的可编程图形上下文接口 (uhost)。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `universe` | `void*` | Universe 实例指针 |
| `config` | `const jegl_interface_config*` | 图形配置（可为 `nullptr`） |

## 返回值

| 类型 | 描述 |
|------|------|
| `jeecs::graphic_uhost*` | 可编程图形上下文接口指针 |

## 用法

此函数用于为 Universe 获取或创建图形上下文管理器。

### 示例

```cpp
je_universe* universe = je_ecs_universe_create();

// 使用默认配置
jeecs::graphic_uhost* host = jegl_uhost_get_or_create_for_universe(universe, nullptr);

// 或指定配置
jegl_interface_config config = {};
config.m_display_mode = jegl_interface_config::WINDOWED;
config.m_width = 1920;
config.m_height = 1080;

jeecs::graphic_uhost* host = jegl_uhost_get_or_create_for_universe(universe, &config);
```

## 注意事项

- `config` 用于指示图形配置，若首次创建图形接口则使用此设置
- 若图形接口已经创建，则调用 `jegl_reboot_graphic_thread` 以应用图形配置
- 若需要使用默认配置或不改变图形设置，请传入 `nullptr`
- 创建出的 uhost 将通过 `je_ecs_universe_register_exit_callback` 注册退出回调

## 相关接口

- [jegl_uhost_get_context](jegl_uhost_get_context.md) - 获取图形上下文
- [jegl_uhost_alloc_branch](jegl_uhost_alloc_branch.md) - 分配绘制组
