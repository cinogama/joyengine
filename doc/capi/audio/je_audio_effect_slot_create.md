# je_audio_effect_slot_create

## 函数签名

```cpp
JE_API je_audio_effect_slot_t* je_audio_effect_slot_create();
```

## 描述

创建一个效果槽。效果槽是一个用于存放音频效果的容器。

## 参数

无

## 返回值

| 类型 | 描述 |
|------|------|
| je_audio_effect_slot_t* | 效果槽实例指针 |

### je_audio_effect_slot_t 结构体

```cpp
struct je_audio_effect_slot_t {
    je_audio_native_effect_slot_instance_t* m_effect_slot_instance;
    size_t m_references; // 引用计数
    float m_gain;        // 增益，默认值为 1.0，范围 [0.0, 1.0]
};
```

## 用法示例

```cpp
// 创建效果槽
je_audio_effect_slot_t* slot = je_audio_effect_slot_create();

// 创建混响效果
je_audio_effect_reverb_t* reverb = je_audio_effect_reverb_create();

// 绑定效果到效果槽
je_audio_effect_slot_bind(slot, reverb);

// 将效果槽应用到音频源
je_audio_source_effect_slot_and_filter_set(source, 0, slot, nullptr);

// 清理
je_audio_effect_close(reverb);
je_audio_effect_slot_close(slot);
```

## 注意事项

- 一个效果槽可以被添加到多个声源上
- 一个声源亦可附加多个效果槽，但通常会受到平台限制，单个声源只能附加 1-4 个效果槽
- 使用完毕后需要调用 `je_audio_effect_slot_close` 释放资源

## 相关接口

- [je_audio_effect_slot_close](je_audio_effect_slot_close.md) - 关闭效果槽
- [je_audio_effect_slot_bind](je_audio_effect_slot_bind.md) - 绑定效果到效果槽
- [je_audio_effect_slot_update](je_audio_effect_slot_update.md) - 更新效果槽
