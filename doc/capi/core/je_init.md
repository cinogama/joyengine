# je_init

## 函数签名

```c
JE_API void je_init(int argc, char** argv);
```

## 描述

引擎的基本初始化操作。此函数应当在所有其他引擎相关的操作之前调用，用于初始化引擎的各个子系统。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `argc` | `int` | 命令行参数的数量；若不希望提供，可以传入 `0` |
| `argv` | `char**` | 命令行参数的字符串数组；若不希望提供，可以传入 `nullptr` |

## 返回值

无返回值。

## 用法

此函数是引擎启动的入口点，负责初始化以下子系统：
- 日志系统
- 着色器生成器
- Woolang 脚本引擎
- 文件系统路径
- 外部库接口
- 音频系统
- 核心类型和系统注册

### 示例

```cpp
int main(int argc, char** argv) {
    // 初始化引擎
    je_init(argc, argv);
    
    // 使用引擎功能...
    
    // 退出引擎
    je_finish();
    return 0;
}

// 或者不传递命令行参数
int main() {
    je_init(0, nullptr);
    
    // 使用引擎功能...
    
    je_finish();
    return 0;
}
```

### 命令行参数

引擎支持以下命令行参数：

| 参数 | 值 | 描述 |
|------|-----|------|
| `-gapi` | `dx11` | 使用 DirectX 11 图形接口 |
| `-gapi` | `gl3` | 使用 OpenGL 3.3 / OpenGL ES 3.0 图形接口 |
| `-gapi` | `vk120` | 使用 Vulkan 1.2 图形接口 |
| `-gapi` | `metal` | 使用 Metal 图形接口 |
| `-gapi` | `none` | 不使用图形接口 |

## 注意事项

- 必须在使用任何其他引擎 API 之前调用此函数
- 引擎退出完毕后（调用 `je_finish` 之后）可以重新调用 `je_init` 重新启动引擎
- 初始化过程中会重置图形接口设置、同步回调等全局状态

## 相关接口

- [je_finish](je_finish.md) - 引擎退出操作
- [je_build_version](je_build_version.md) - 获取引擎版本信息
- [je_build_commit](je_build_commit.md) - 获取引擎提交哈希
