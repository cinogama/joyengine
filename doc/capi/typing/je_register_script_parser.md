# je_register_script_parser

## 函数签名

```c
JE_API void je_register_script_parser(
    const jeecs::typing::type_info* _type,
    jeecs::typing::parse_c2w_func_t c2w,
    jeecs::typing::parse_w2c_func_t w2c,
    const char* woolang_typename,
    const char* woolang_typedecl);
```

## 描述

向引擎的类型管理器注册指定类型的脚本转换方法，使得 C++ 类型能够与 Woolang 脚本之间进行数据转换。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_type` | `const jeecs::typing::type_info*` | 类型信息指针 |
| `c2w` | `parse_c2w_func_t` | C++ 到 Woolang 的转换函数 |
| `w2c` | `parse_w2c_func_t` | Woolang 到 C++ 的转换函数 |
| `woolang_typename` | `const char*` | 在 Woolang 脚本中使用的类型名称 |
| `woolang_typedecl` | `const char*` | Woolang 类型声明语句 |

### 函数指针类型

```cpp
using parse_c2w_func_t = void (*)(const void* c_value, wo_vm vm, wo_value woo_value);
using parse_w2c_func_t = void (*)(void* c_value, wo_vm vm, wo_value woo_value);
```

## 返回值

无返回值。

## 用法

此函数用于注册类型与 Woolang 脚本之间的转换方法，使得组件数据可以在脚本中被访问和修改。

### 示例

```cpp
struct Vec3 {
    float x, y, z;
};

// C++ 到 Woolang 转换
void vec3_c2w(const void* c_value, wo_vm vm, wo_value woo_value) {
    const Vec3* v = (const Vec3*)c_value;
    // 创建 Woolang 的 vec3 值
    wo_set_struct(woo_value, vm, 3);
    wo_struct_set(woo_value, 0, wo_float(v->x));
    wo_struct_set(woo_value, 1, wo_float(v->y));
    wo_struct_set(woo_value, 2, wo_float(v->z));
}

// Woolang 到 C++ 转换
void vec3_w2c(void* c_value, wo_vm vm, wo_value woo_value) {
    Vec3* v = (Vec3*)c_value;
    v->x = wo_float(wo_struct_get(woo_value, 0));
    v->y = wo_float(wo_struct_get(woo_value, 1));
    v->z = wo_float(wo_struct_get(woo_value, 2));
}

// 注册脚本转换器
const jeecs::typing::type_info* vec3_type = /* 已注册的类型 */;
je_register_script_parser(
    vec3_type,
    vec3_c2w,
    vec3_w2c,
    "vec3",
    "public struct vec3 { x: real, y: real, z: real }"
);
```

## 注意事项

- 使用本地 typeinfo，而非全局通用 typeinfo
- 转换函数应确保数据的正确性和完整性
- woolang_typedecl 应该是有效的 Woolang 类型声明语句

## 相关接口

- [je_typing_register](je_typing_register.md) - 注册类型
- [je_register_member](je_register_member.md) - 注册类型成员
- [je_register_system_updater](je_register_system_updater.md) - 注册系统更新方法
