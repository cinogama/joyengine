# jeal_create_effect_slot

## 函数签名

```cpp
JE_API jeal_effect_slot* jeal_create_effect_slot();
```

## 描述

创建一个效果槽。效果槽是一个用于存放音频效果的容器。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| jeal_effect_slot* | 效果槽实例指针 |

### jeal_effect_slot 结构体

```cpp
struct jeal_effect_slot {
    jeal_native_effect_slot_instance* m_effect_slot_instance;
    size_t m_references; // 引用计数
    float m_gain;        // 增益，默认值为 1.0，范围 [0.0, 1.0]
};
```

## 用法示例

```cpp
// 创建效果槽
jeal_effect_slot* slot = jeal_create_effect_slot();

// 创建混响效果
jeal_effect_reverb* reverb = jeal_create_effect_reverb();

// 绑定效果到效果槽
jeal_effect_slot_bind(slot, reverb);

// 将效果槽应用到音频源
jeal_set_source_effect_slot_and_filter(source, 0, slot, nullptr);

// 清理
jeal_close_effect(reverb);
jeal_close_effect_slot(slot);
```

## 注意事项

- 一个效果槽可以被添加到多个声源上
- 一个声源亦可附加多个效果槽，但通常会受到平台限制，单个声源只能附加 1-4 个效果槽
- 使用完毕后需要调用 `jeal_close_effect_slot` 释放资源

## 相关接口

- [jeal_close_effect_slot](jeal_close_effect_slot.md) - 关闭效果槽
- [jeal_effect_slot_bind](jeal_effect_slot_bind.md) - 绑定效果到效果槽
- [jeal_update_effect_slot](jeal_update_effect_slot.md) - 更新效果槽
