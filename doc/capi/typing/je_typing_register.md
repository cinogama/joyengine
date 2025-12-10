# je_typing_register

## 函数签名

```c
JE_API const jeecs::typing::type_info* je_typing_register(
    const char* _name,
    jeecs::typing::typehash_t _hash,
    size_t _size,
    size_t _align,
    je_typing_class _typecls,
    jeecs::typing::construct_func_t _constructor,
    jeecs::typing::destruct_func_t _destructor,
    jeecs::typing::copy_construct_func_t _copy_constructor,
    jeecs::typing::move_construct_func_t _move_constructor);
```

## 描述

向引擎的类型管理器注册一个类型及其基本信息。此函数是引擎类型系统的核心接口，用于注册组件、系统或其他自定义类型。

## 参数

| 参数名 | 类型 | 描述 |
|--------|------|------|
| `_name` | `const char*` | 类型的唯一名称标识符 |
| `_hash` | `jeecs::typing::typehash_t` | 类型的哈希值，通常使用 `typeid(T).hash_code()` |
| `_size` | `size_t` | 类型的大小（字节） |
| `_align` | `size_t` | 类型的对齐要求（字节） |
| `_typecls` | `je_typing_class` | 类型的分类 |
| `_constructor` | `construct_func_t` | 构造函数指针 |
| `_destructor` | `destruct_func_t` | 析构函数指针 |
| `_copy_constructor` | `copy_construct_func_t` | 拷贝构造函数指针 |
| `_move_constructor` | `move_construct_func_t` | 移动构造函数指针 |

### 类型分类 (je_typing_class)

| 枚举值 | 描述 |
|--------|------|
| `JE_BASIC_TYPE` | 基本类型 |
| `JE_COMPONENT` | 组件类型 |
| `JE_SYSTEM` | 系统类型 |

### 函数指针类型

```cpp
using construct_func_t = void (*)(void* _this, void* _world, const jeecs::typing::type_info*);
using destruct_func_t = void (*)(void* _this);
using copy_construct_func_t = void (*)(void* _this, const void* _from);
using move_construct_func_t = void (*)(void* _this, void* _from);
```

## 返回值

| 类型 | 描述 |
|------|------|
| `const jeecs::typing::type_info*` | 指向注册的类型信息的指针；如果注册失败返回 `nullptr` |

## 用法

此函数用于向引擎注册自定义类型。注册后的类型可以被 ECS 系统识别和使用。

### 示例

```cpp
// 定义一个组件
struct MyComponent {
    float x, y, z;
    
    MyComponent() : x(0), y(0), z(0) {}
};

// 构造函数
void my_component_construct(void* _this, void*, const jeecs::typing::type_info*) {
    new(_this) MyComponent();
}

// 析构函数
void my_component_destruct(void* _this) {
    ((MyComponent*)_this)->~MyComponent();
}

// 拷贝构造函数
void my_component_copy(void* _this, const void* _from) {
    new(_this) MyComponent(*(const MyComponent*)_from);
}

// 移动构造函数
void my_component_move(void* _this, void* _from) {
    new(_this) MyComponent(std::move(*(MyComponent*)_from));
}

// 注册类型
const jeecs::typing::type_info* my_type = je_typing_register(
    "MyComponent",
    typeid(MyComponent).hash_code(),
    sizeof(MyComponent),
    alignof(MyComponent),
    JE_COMPONENT,
    my_component_construct,
    my_component_destruct,
    my_component_copy,
    my_component_move
);
```

## 注意事项

- 类型名称是唯一标识符，不同类型必须使用不同的名字
- 注册的类型必须通过 `je_typing_unregister` 在适当时机释放
- 建议遵循"谁注册谁释放"原则
- 构造函数、析构函数等函数指针可以为 `nullptr`（如果类型是 trivial 的）

## 相关接口

- [je_typing_unregister](je_typing_unregister.md) - 取消注册类型
- [je_typing_get_info_by_id](je_typing_get_info_by_id.md) - 通过 ID 获取类型信息
- [je_typing_get_info_by_hash](je_typing_get_info_by_hash.md) - 通过哈希获取类型信息
- [je_typing_get_info_by_name](je_typing_get_info_by_name.md) - 通过名称获取类型信息
