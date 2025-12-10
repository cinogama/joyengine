# je_ecs_world_destroy

## 函数签名

```c
JE_API void je_ecs_world_destroy(void* world);
```

## 描述

从 Universe 中销毁一个世界。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `world` | `void*` | 指向需要销毁的世界的指针 |

## 返回值

无返回值。

## 用法

此函数用于请求销毁一个世界。销毁不会立即生效，而是在下一次逻辑更新时执行。

### 销毁顺序

1. 世界被标记为即将销毁
2. 销毁所有系统
3. 销毁所有实体
4. 执行最后命令缓冲区更新
5. 从 Universe 的世界列表中移除

### 示例

```cpp
void* universe = je_ecs_universe_create();
void* world = je_ecs_world_create(universe);

// 使用世界...

// 请求销毁世界
je_ecs_world_destroy(world);
// 注意：此时世界尚未真正销毁，会在下一次更新时处理
```

## 注意事项

- 销毁操作不会立即生效，而是延迟到下一次逻辑更新
- 向一个正在销毁中的世界创建实体或添加系统是无效的
- 世界销毁时会自动销毁其中的所有系统和实体

## 相关接口

- [je_ecs_world_create](je_ecs_world_create.md) - 创建世界
- [je_ecs_world_set_able](je_ecs_world_set_able.md) - 设置世界激活状态
