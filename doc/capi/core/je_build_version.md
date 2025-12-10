# je_build_version

## 函数签名

```c
JE_API const char* je_build_version();
```

## 描述

获取引擎的版本信息字符串，包含引擎名称、版本号和编译时间戳。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 指向版本信息字符串的指针，格式为 `"JoyEngine X.Y.Z 编译时间"` |

## 用法

此函数用于获取当前引擎的版本信息，通常用于显示在程序启动界面、关于对话框或日志中。

### 示例

```cpp
#include <cstdio>

int main() {
    je_init(0, nullptr);
    
    // 输出引擎版本信息
    printf("Engine Version: %s\n", je_build_version());
    // 输出示例: "Engine Version: JoyEngine 4.9.0 Dec 10 2025 14:30:00"
    
    je_finish();
    return 0;
}
```

## 注意事项

- 返回的字符串是静态存储的，不需要释放
- 版本号格式为 `主版本.次版本.修订版本`
- 编译时间戳使用 `__TIMESTAMP__` 宏生成

## 相关接口

- [je_build_commit](je_build_commit.md) - 获取引擎提交哈希
- [je_init](je_init.md) - 引擎初始化操作
