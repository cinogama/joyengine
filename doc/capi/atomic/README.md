# Atomic API 概述

## 描述

JoyEngine 提供了一组原子操作 API，用于多线程环境下的安全数据访问。这些 API 通过宏 `JE_DECL_ATOMIC_OPERATOR_API` 为多种数据类型生成。

## 支持的数据类型

- `int8_t` / `uint8_t`
- `int16_t` / `uint16_t`
- `int32_t` / `uint32_t`
- `int64_t` / `uint64_t`
- `size_t`
- `intptr_t`

## 生成的函数

对于每种类型 `TYPE`，会生成以下函数：

### je_atomic_exchange_TYPE

```cpp
JE_API TYPE je_atomic_exchange_TYPE(TYPE* aim, TYPE value);
```

原子地将 `aim` 指向的值替换为 `value`，并返回原来的值。

### je_atomic_cas_TYPE

```cpp
JE_API bool je_atomic_cas_TYPE(TYPE* aim, TYPE* comparer, TYPE value);
```

比较并交换（Compare-And-Swap）。如果 `*aim == *comparer`，则将 `*aim` 设为 `value` 并返回 `true`；否则将 `*comparer` 设为当前 `*aim` 的值并返回 `false`。

### je_atomic_fetch_add_TYPE

```cpp
JE_API TYPE je_atomic_fetch_add_TYPE(TYPE* aim, TYPE value);
```

原子地将 `value` 加到 `*aim` 上，并返回原来的值。

### je_atomic_fetch_sub_TYPE

```cpp
JE_API TYPE je_atomic_fetch_sub_TYPE(TYPE* aim, TYPE value);
```

原子地从 `*aim` 减去 `value`，并返回原来的值。

### je_atomic_fetch_TYPE

```cpp
JE_API TYPE je_atomic_fetch_TYPE(TYPE* aim);
```

原子地读取 `*aim` 的值。

### je_atomic_store_TYPE

```cpp
JE_API void je_atomic_store_TYPE(TYPE* aim, TYPE value);
```

原子地将 `value` 存储到 `*aim`。

## 用法示例

```cpp
// 原子计数器
int32_t counter = 0;

// 增加计数
int32_t old_value = je_atomic_fetch_add_int32_t(&counter, 1);

// 减少计数
old_value = je_atomic_fetch_sub_int32_t(&counter, 1);

// 原子读取
int32_t current = je_atomic_fetch_int32_t(&counter);

// 原子存储
je_atomic_store_int32_t(&counter, 100);

// 原子交换
int32_t previous = je_atomic_exchange_int32_t(&counter, 200);

// CAS 操作
int32_t expected = 200;
if (je_atomic_cas_int32_t(&counter, &expected, 300)) {
    // 交换成功，counter 现在是 300
} else {
    // 交换失败，expected 现在包含 counter 的实际值
}
```

## 注意事项

- 原子操作保证在多线程环境下的数据一致性
- CAS 操作是实现无锁数据结构的基础
- 使用适当大小的类型以获得最佳性能
