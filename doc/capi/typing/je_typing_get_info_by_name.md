# je_typing_get_info_by_name

## 函数签名

```c
JE_API const jeecs::typing::type_info* je_typing_get_info_by_name(
    const char* type_name);
```

## 描述

通过类型的名称获取类型信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `type_name` | `const char*` | 类型的名称字符串 |

## 返回值

| 类型 | 描述 |
|------|------|
| `const jeecs::typing::type_info*` | 指向类型信息的指针；若给定的类型名不存在，返回 `nullptr` |

## 用法

此函数用于通过类型名称字符串查询已注册的类型信息。这是最常用的类型查询方式。

### 示例

```cpp
// 通过名称查找类型信息
const jeecs::typing::type_info* info = je_typing_get_info_by_name("Transform::Translation");
if (info != nullptr) {
    printf("Type ID: %zu\n", info->m_id);
    printf("Type size: %zu bytes\n", info->m_size);
}

// 检查某个组件类型是否已注册
if (je_typing_get_info_by_name("MyCustomComponent") == nullptr) {
    printf("MyCustomComponent is not registered!\n");
}
```

## 注意事项

- 类型名称必须与注册时使用的名称完全一致（区分大小写）
- 类型名称是唯一的，不同类型必须使用不同名称
- 返回的指针指向引擎内部管理的数据，不需要释放
- 对于内置组件，名称通常是 `命名空间::类型名` 格式

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_typing_get_info_by_id](je_typing_get_info_by_id.md) - 通过 ID 获取类型信息
- [je_typing_get_info_by_hash](je_typing_get_info_by_hash.md) - 通过哈希获取类型信息
