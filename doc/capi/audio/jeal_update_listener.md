# jeal_update_listener

## 函数签名

```cpp
JE_API void jeal_update_listener();
```

## 描述

将对 `jeal_listener` 的参数修改应用到实际的监听者上。

## 参数

无

## 返回值

无

## 用法示例

```cpp
jeal_listener* listener = jeal_get_listener();

// 更新监听器位置（跟随玩家）
listener->m_location[0] = player_x;
listener->m_location[1] = player_y;
listener->m_location[2] = player_z;

// 应用更改
jeal_update_listener();
```

## 注意事项

- 直接修改 `jeal_listener` 结构体的成员不会立即生效，需要调用此函数

## 相关接口

- [jeal_get_listener](jeal_get_listener.md) - 获取监听器实例
