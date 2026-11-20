# je_file_user_path_set

## 函数签名

```c
JE_API void je_file_user_path_set(const char* path);
```

## 描述

设置当前引擎的用户数据路径，用于存放用户状态（编辑器窗口布局 imgui.ini、项目打开历史等）。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 用户数据路径（必须可写） |

## 返回值

无返回值。

## 用法

设置时会尽力创建该目录（失败时记录警告）。初始化时引擎按操作系统标准位置自动推导：

| 平台 | 默认路径 |
|------|----------|
| Windows | `%APPDATA%\JoyEngine` |
| macOS | `~/Library/Application Support/JoyEngine` |
| Linux | `$XDG_DATA_HOME/JoyEngine`，未设置时 `~/.local/share/JoyEngine` |

环境变量不可用时回退为可执行文件所在路径；Android/WebGL 不做环境推导，由端口自行设置。

### 示例

```cpp
// 将用户数据路径指定到自定义目录
je_file_user_path_set("/custom/user/data");

// 编辑器窗口布局与项目历史将写入该目录
```

## 注意事项

- 这个目录必须是可写的：只读的安装布局（如 macOS .app 的 `Contents/Resources`）不应作为此路径
- 宿主路径（`!` 前缀）指向的目录在打包安装后可能只读，用户状态必须落在用户数据路径下
- 特殊平台（Android/WebGL）不适用环境推导，需要在端口初始化时显式设置

## 相关接口

- [je_file_user_path_get](je_file_user_path_get.md) - 获取用户数据路径
- [je_file_host_path_set](je_file_host_path_set.md) - 设置宿主环境路径
