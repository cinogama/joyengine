# je_build_commit

## 函数签名

```c
JE_API const char* je_build_commit();
```

## 描述

获取当前引擎所在的 Git 提交哈希值。

## 参数

无参数。

## 返回值

| 类型 | 描述 |
|------|------|
| `const char*` | 指向提交哈希字符串的指针；如果是手动构建的引擎，返回 `"untracked"` |

## 用法

此函数用于获取当前引擎构建时的 Git 提交哈希，便于追踪和调试特定版本的引擎。

### 示例

```cpp
#include <cstdio>

int main() {
    je_init(0, nullptr);
    
    // 输出引擎版本和提交信息
    printf("Engine: %s\n", je_build_version());
    printf("Commit: %s\n", je_build_commit());
    // 输出示例:
    // "Engine: JoyEngine 4.9.0 Dec 10 2025 14:30:00"
    // "Commit: a1b2c3d4e5f6..." 或 "Commit: untracked"
    
    je_finish();
    return 0;
}
```

## 注意事项

- 返回的字符串是静态存储的，不需要释放
- 如果使用的是手动构建的引擎（非通过 CI/CD 构建），此函数将返回 `"untracked"`
- 提交哈希通常在构建过程中由构建脚本自动生成

## 相关接口

- [je_build_version](je_build_version.md) - 获取引擎版本信息
