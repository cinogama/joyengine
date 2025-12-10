# je_typing_get_info_by_hash

## 函数签名

```c
JE_API const jeecs::typing::type_info* je_typing_get_info_by_hash(
    jeecs::typing::typehash_t _hash);
```

## 描述

通过类型的哈希值获取类型信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_hash` | `jeecs::typing::typehash_t` | 类型的哈希值，通常使用 `typeid(T).hash_code()` |

## 返回值

| 类型 | 描述 |
|------|------|
| `const jeecs::typing::type_info*` | 指向类型信息的指针；若给定的哈希值不合法，返回 `nullptr` |

## 用法

此函数用于通过 C++ RTTI 生成的类型哈希值查询已注册的类型信息。

### 示例

```cpp
// 通过类型哈希查找类型信息
jeecs::typing::typehash_t hash = typeid(MyComponent).hash_code();

const jeecs::typing::type_info* info = je_typing_get_info_by_hash(hash);
if (info != nullptr) {
    printf("Found type: %s\n", info->m_typename);
    printf("Type ID: %zu\n", info->m_id);
}
```

## 注意事项

- 哈希值通常在注册时使用 `typeid(T).hash_code()` 生成
- 哈希冲突理论上可能存在，但在实际使用中极为罕见
- 返回的指针指向引擎内部管理的数据，不需要释放

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_typing_get_info_by_id](je_typing_get_info_by_id.md) - 通过 ID 获取类型信息
- [je_typing_get_info_by_name](je_typing_get_info_by_name.md) - 通过名称获取类型信息
