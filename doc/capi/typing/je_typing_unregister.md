# je_typing_unregister

## 函数签名

```c
JE_API void je_typing_unregister(const jeecs::typing::type_info* tinfo);
```

## 描述

向引擎的类型管理器要求解除注册指定的类型信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `tinfo` | `const jeecs::typing::type_info*` | 指向需要取消注册的类型信息的指针 |

## 返回值

无返回值。

## 用法

此函数用于取消注册先前通过 `je_typing_register` 注册的类型。

### 示例

```cpp
// 注册类型
const jeecs::typing::type_info* my_type = je_typing_register(
    "MyComponent",
    typeid(MyComponent).hash_code(),
    sizeof(MyComponent),
    alignof(MyComponent),
    JE_COMPONENT,
    my_construct, my_destruct, my_copy, my_move
);

// 使用类型...

// 取消注册
je_typing_unregister(my_type);
```

## 注意事项

- 一般而言引擎推荐遵循"谁注册谁释放"原则
- 请确保释放的类型是当前模块通过 `je_typing_register` 成功注册的类型
- 若释放的类型不合法，则给出错误级别的日志信息
- 取消注册之前，确保没有实体正在使用该类型的组件

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
