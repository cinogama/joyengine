# jeal_get_listener

## 函数签名

```cpp
JE_API jeal_listener* jeal_get_listener();
```

## 描述

获取监听者的实例。监听者代表游戏中"听者"的位置和朝向，通常对应玩家的耳朵位置。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_listener* | 监听者实例指针 |

### jeal_listener 结构体

```cpp
struct jeal_listener {
    float m_gain;        // 音量增益，最终起效的是 m_gain * m_global_gain
    float m_global_gain; // 全局增益，最终起效的是 m_gain * m_global_gain
    float m_location[3]; // 监听器位置
    float m_velocity[3]; // 监听器自身速度（非播放速度）
    float m_forward[3];  // 监听器朝向（前方向）
    float m_upward[3];   // 监听器朝向（顶方向）
};
```

## 用法示例

```cpp
jeal_listener* listener = jeal_get_listener();

// 设置监听器位置
listener->m_location[0] = 0.0f;  // X
listener->m_location[1] = 0.0f;  // Y
listener->m_location[2] = 0.0f;  // Z

// 设置监听器朝向（面向 -Z 方向）
listener->m_forward[0] = 0.0f;
listener->m_forward[1] = 0.0f;
listener->m_forward[2] = -1.0f;

listener->m_upward[0] = 0.0f;
listener->m_upward[1] = 1.0f;
listener->m_upward[2] = 0.0f;

// 设置音量
listener->m_gain = 1.0f;
listener->m_global_gain = 1.0f;

// 应用更改
jeal_update_listener();
```

## 注意事项

- 修改监听器属性后需要调用 `jeal_update_listener` 使更改生效
- 全局只有一个监听器实例

## 相关接口

- [jeal_update_listener](jeal_update_listener.md) - 应用监听器属性更改
