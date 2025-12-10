# je_main_script_entry

## 函数签名

```cpp
JE_API bool je_main_script_entry();
```

## 描述

运行入口脚本。阻塞直到入口脚本运行完毕。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| bool | 脚本执行成功返回 true，失败返回 false |

## 执行逻辑

1. 尝试带缓存地加载 `@/builtin/editor/main.wo`
2. 如果未能加载 `@/builtin/editor/main.wo`，则尝试 `@/builtin/main.wo`

## 用法示例

```cpp
int main() {
    // 初始化引擎
    je_init(0, nullptr);
    
    // 运行入口脚本
    bool success = je_main_script_entry();
    
    if (!success) {
        printf("脚本执行失败\n");
    }
    
    // 清理
    je_finish();
    
    return success ? 0 : 1;
}
```

## 注意事项

- 此函数会阻塞直到脚本执行完毕
- 通常是引擎的主入口点
- 脚本文件使用 Woolang 语言编写

## 相关接口

- [je_init](../core/je_init.md) - 初始化引擎
- [je_finish](../core/je_finish.md) - 清理引擎
