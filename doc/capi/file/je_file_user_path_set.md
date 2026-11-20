# je_file_user_path_set

## 函数签名

```c
JE_API void je_file_user_path_set(const char* path);
```

## 描述

设置当前引擎的用户数据路径，用于存放用户状态（编辑器窗口布局 imgui.ini、项目打开历史等）。

此接口只记录路径，不会创建目录。确定最终路径后应调用 [je_file_user_path_assure](je_file_user_path_assure.md) 创建目录。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `path` | `const char*` | 用户数据路径（必须可写） |

## 返回值

无返回值。

## 用法

初始化时引擎按操作系统标准位置自动推导，并以可执行文件名作为应用子目录（避免多个使用本引擎的程序互相覆盖用户状态）：

| 平台 | 默认路径 |
|------|----------|
| Windows | `%APPDATA%\JoyEngine\<可执行文件名>` |
| macOS | `~/Library/Application Support/JoyEngine/<可执行文件名>` |
| Linux | `$XDG_DATA_HOME/JoyEngine/<可执行文件名>`，未设置时 `~/.local/share/JoyEngine/<可执行文件名>` |

推导完成后由入口点在进入主循环前调用 `je_file_user_path_assure` 创建该目录（`driver/main.cpp` 已内置此调用）。环境变量不可用时回退为可执行文件所在路径；Android/WebGL 不做环境推导，由端口自行设置并调用 `je_file_user_path_assure`。

### 示例

```cpp
// 将用户数据路径指定到自定义目录
je_file_user_path_set("/custom/user/data");
je_file_user_path_assure();
```

## 注意事项

- 这个目录必须是可写的：只读的安装布局（如 macOS .app 的 `Contents/Resources`）不应作为此路径
- 宿主路径（`!` 前缀）指向的目录在打包安装后可能只读，用户状态必须落在用户数据路径下
- 特殊平台（Android/WebGL）不适用环境推导，需要在端口初始化时显式设置路径并调用 `je_file_user_path_assure`

## 相关接口

- [je_file_user_path_assure](je_file_user_path_assure.md) - 创建用户数据路径
- [je_file_user_path_get](je_file_user_path_get.md) - 获取用户数据路径
- [je_file_host_path_set](je_file_host_path_set.md) - 设置宿主环境路径
