# je_typing_get_info_by_id

## 函数签名

```c
JE_API const jeecs::typing::type_info* je_typing_get_info_by_id(
    jeecs::typing::typeid_t _id);
```

## 描述

通过类型 ID 获取类型信息。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_id` | `jeecs::typing::typeid_t` | 类型的 ID |

## 返回值

| 类型 | 描述 |
|------|------|
| `const jeecs::typing::type_info*` | 指向类型信息的指针；若给定的 ID 不合法，返回 `nullptr` |

## 用法

此函数用于通过类型 ID 查询已注册的类型信息。类型 ID 是引擎内部分配的顺序编号。

### 示例

```cpp
// 假设已知某个组件的类型 ID
jeecs::typing::typeid_t component_id = 5;

const jeecs::typing::type_info* info = je_typing_get_info_by_id(component_id);
if (info != nullptr) {
    printf("Type name: %s\n", info->m_typename);
    printf("Type size: %zu\n", info->m_size);
}
```

## 注意事项

- 类型 ID 是引擎内部动态分配的，不同运行时可能不同
- `jeecs::typing::INVALID_TYPE_ID` (SIZE_MAX) 是无效的类型 ID
- 返回的指针指向引擎内部管理的数据，不需要释放

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_typing_get_info_by_hash](je_typing_get_info_by_hash.md) - 通过哈希获取类型信息
- [je_typing_get_info_by_name](je_typing_get_info_by_name.md) - 通过名称获取类型信息
