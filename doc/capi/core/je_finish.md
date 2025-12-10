# je_finish

## 函数签名

```c
JE_API void je_finish(void);
```

## 描述

引擎的退出操作。此函数应当在所有其他引擎相关的操作之后调用，用于清理引擎的各个子系统并释放资源。

## 参数

无参数。

## 返回值

无返回值。

## 用法

此函数是引擎关闭的出口点，负责按顺序清理以下子系统：
- ECS 系统
- 音频系统
- 图形系统
- 脚本系统
- Woolang 运行时
- 外部库和模块
- 着色器生成器
- 日志系统

### 示例

```cpp
int main(int argc, char** argv) {
    // 初始化引擎
    je_init(argc, argv);
    
    // 创建 Universe 并运行游戏逻辑...
    void* universe = je_ecs_universe_create();
    // ...
    je_ecs_universe_destroy(universe);
    
    // 退出引擎
    je_finish();
    return 0;
}
```

## 注意事项

- 必须在所有引擎资源释放完毕后调用此函数
- 引擎退出完毕之后可以重新调用 `je_init` 重新启动引擎
- 确保所有 Universe 和 World 已经正确销毁
- 调用此函数后，不应再使用任何引擎 API（除非重新调用 `je_init`）

## 相关接口

- [je_init](je_init.md) - 引擎初始化操作
