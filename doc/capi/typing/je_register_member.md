# je_register_member

## 函数签名

```c
JE_API void je_register_member(
    const jeecs::typing::type_info* _classtype,
    const jeecs::typing::type_info* _membertype,
    const char* _member_name,
    const char* _woovalue_type_may_null,
    wo_value _woovalue_init_may_null,
    ptrdiff_t _member_offset);
```

## 描述

向引擎的类型管理器注册指定类型的成员信息。用于描述组件或其他类型内部的字段结构。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_classtype` | `const jeecs::typing::type_info*` | 所属类型的类型信息指针 |
| `_membertype` | `const jeecs::typing::type_info*` | 成员的类型信息指针 |
| `_member_name` | `const char*` | 成员的名称 |
| `_woovalue_type_may_null` | `const char*` | Woolang 脚本中对应的类型名称；可以为 `nullptr` |
| `_woovalue_init_may_null` | `wo_value` | Woolang 脚本中的初始值；可以为 `nullptr` |
| `_member_offset` | `ptrdiff_t` | 成员在类型中的偏移量（字节） |

## 返回值

无返回值。

## 用法

此函数用于注册类型的成员字段，使得引擎能够识别类型的内部结构，支持序列化、编辑器显示等功能。

### 示例

```cpp
struct MyComponent {
    float x;
    float y;
    int count;
};

// 注册 MyComponent 类型后，注册其成员
const jeecs::typing::type_info* my_type = /* 已注册的类型 */;
const jeecs::typing::type_info* float_type = je_typing_get_info_by_name("float");
const jeecs::typing::type_info* int_type = je_typing_get_info_by_name("int");

// 注册成员 x
je_register_member(
    my_type,
    float_type,
    "x",
    "real",   // Woolang 类型
    nullptr,  // 使用默认初始值
    offsetof(MyComponent, x)
);

// 注册成员 y
je_register_member(
    my_type,
    float_type,
    "y",
    "real",
    nullptr,
    offsetof(MyComponent, y)
);

// 注册成员 count
je_register_member(
    my_type,
    int_type,
    "count",
    "int",
    nullptr,
    offsetof(MyComponent, count)
);
```

## 注意事项

- 使用本地 typeinfo，而非全局通用 typeinfo
- 成员偏移量应使用 `offsetof` 宏计算
- 调用 `je_typing_reset` 会清除之前注册的成员信息

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_typing_reset](je_typing_reset.md) - 重置类型信息
- [je_register_script_parser](je_register_script_parser.md) - 注册脚本转换方法
