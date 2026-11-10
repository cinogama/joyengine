# je_core_main_script_entry

## 函数签名

```cpp
JE_API bool je_core_main_script_entry();
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

1. 先尝试加载游戏入口 `@/builtin/main.wo`（不带缓存）
2. 若游戏入口不存在，则带缓存地加载编辑器入口 `@/builtin/editor/main.wo`

## 用法示例

```cpp
int main() {
    // 初始化引擎
    je_core_init(0, nullptr);
    
    // 运行入口脚本
    bool success = je_core_main_script_entry();
    
    if (!success) {
        printf("脚本执行失败\n");
    }
    
    // 清理
    je_core_finish();
    
    return success ? 0 : 1;
}
```

## 注意事项

- 此函数会阻塞直到脚本执行完毕
- 通常是引擎的主入口点
- 脚本文件使用 Woolang 语言编写

## 相关接口

- [je_core_init](../core/je_core_init.md) - 初始化引擎
- [je_core_finish](../core/je_core_finish.md) - 清理引擎
