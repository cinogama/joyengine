#pragma once

// ============================================================================
// jeecs/basic.hpp —— 基础工具（C++）：jeecs::parallel_foreach 与 jeecs::basic 命名空间（singleton/vector/map/string/hash/atomic_list/shared_pointer/optional/resource/file_resource）
// =============================================================================

#include "jeecs/prefix.hpp"
#include "jeecs/core.hpp"
#include "jeecs/atomic.hpp"
#include "jeecs/logging.hpp"

#ifdef __cplusplus
namespace jeecs
{
    /*
    jeecs::parallel_foreach [函数模板]
    统一的并行遍历入口：当 JE_PARALLEL_FOREACH_ENABLED 时使用
    std::for_each(par_unseq, ...)，否则退化为串行 std::for_each。
    用函数模板而非宏实现，是为了避免调用方 lambda 体内的预处理指令
    (#ifndef NDEBUG 等) 落入函数式宏实参列表导致的未定义行为。
    */
    template <typename _Iter, typename _Fn>
    inline void parallel_foreach(_Iter _first, _Iter _last, _Fn _fn)
    {
#if JE_PARALLEL_FOREACH_ENABLED
        ::std::for_each(::std::execution::par_unseq, _first, _last, ::std::move(_fn));
#else
        ::std::for_each(_first, _last, ::std::move(_fn));
#endif
    }
    /*
    jeecs::basic [命名空间]
    用于存放引擎的基本工具
    */
    namespace basic
    {
        /*
        jeecs::basic::singleton [类型]
        用于管理系统之间共享的单例实例，使用acquire方法获取和创建reference
            * 当最后一个reference析构时，单例实例会被释放
            * 首次（或之前的单例释放之后的首次）获取时，会调用默认构造函数创建实例
        */
        template <typename T>
        class singleton
        {
            std::shared_mutex
                m_singleton_mutex;
            T* m_instance;
            size_t m_ref_count;

            JECS_DISABLE_MOVE_AND_COPY(singleton);

        public:
            singleton()
                : m_instance(nullptr), m_ref_count(0)
            {
            }
            ~singleton()
            {
                assert(m_instance == nullptr);
                assert(m_ref_count == 0);
            }

            class reference
            {
                singleton* m_singleton;
                T* m_instance;

            public:
                reference(singleton* s, T* inst)
                    : m_singleton(s), m_instance(inst)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);
                }
                reference(const reference& r)
                    : m_singleton(r.m_singleton), m_instance(r.m_instance)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    std::lock_guard g(m_singleton->m_singleton_mutex);
                    m_singleton->m_ref_count++;
                }
                reference(reference&& mr)
                    : m_singleton(mr.m_singleton), m_instance(mr.m_instance)
                {
                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    mr.m_singleton = nullptr;
                    mr.m_instance = nullptr;
                }

                reference& operator=(const reference& r)
                {
                    m_singleton = r.m_singleton;
                    m_instance = r.m_instance;

                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    std::lock_guard g(m_singleton->m_singleton_mutex);
                    m_singleton->m_ref_count++;

                    return *this;
                }
                reference& operator=(reference&& mr)
                {
                    m_singleton = mr.m_singleton;
                    m_instance = mr.m_instance;

                    assert(m_singleton != nullptr);
                    assert(m_instance != nullptr);

                    mr.m_singleton = nullptr;
                    mr.m_instance = nullptr;

                    return *this;
                }

                void lock()
                {
                    m_singleton->m_singleton_mutex.lock();
                }
                bool try_lock()
                {
                    return m_singleton->m_singleton_mutex.try_lock();
                }
                void unlock()
                {
                    m_singleton->m_singleton_mutex.unlock();
                }

                void lock_shared()
                {
                    m_singleton->m_singleton_mutex.lock_shared();
                }
                bool try_lock_shared()
                {
                    return m_singleton->m_singleton_mutex.try_lock_shared();
                }
                void unlock_shared()
                {
                    m_singleton->m_singleton_mutex.unlock_shared();
                }

                ~reference()
                {
                    if (m_singleton != nullptr)
                    {
                        assert(m_instance == m_singleton->m_instance);
                        m_singleton->_release();
                    }
                }

                T* operator->()
                {
                    return m_instance;
                }
                T* get()
                {
                    return m_instance;
                }
            };

            reference acquire()
            {
                std::unique_lock g(m_singleton_mutex);

                if (0 == m_ref_count++)
                {
                    // Create instance.
                    m_instance = new T();
                }

                g.unlock();
                return reference(this, m_instance);
            }
            void _release()
            {
                std::lock_guard g(m_singleton_mutex);
                if (0 == --m_ref_count)
                {
                    delete m_instance;
                    m_instance = nullptr;
                }
            }
        };

        /*
        jeecs::basic::vector [类型]
        用于存放大小可变的连续存储容器
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::vector
        */
        template <typename ElemT>
        class vector
        {
            ElemT* _elems_ptr_begin = nullptr;
            ElemT* _elems_ptr_end = nullptr;
            ElemT* _elems_buf_end = nullptr;

            static constexpr size_t _single_elem_size = sizeof(ElemT);

            inline static size_t _move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end; ++origin_elem)
                {
                    new (to_begin++) ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _r_move(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_end; origin_elem > from_begin;)
                {
                    size_t offset = (size_t)((--origin_elem) - from_begin);
                    new (to_begin + offset) ElemT(std::move(*origin_elem));
                    origin_elem->~ElemT();
                }
                return (size_t)(from_end - from_begin);
            }
            inline static size_t _copy(ElemT* to_begin, ElemT* from_begin, ElemT* from_end) noexcept
            {
                for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                {
                    new (to_begin++) ElemT(*(origin_elem++));
                }

                return (size_t)(from_end - from_begin);
            }
            inline static size_t _erase(ElemT* from_begin, ElemT* from_end) noexcept
            {
                if constexpr (!std::is_trivial_v<ElemT>)
                {
                    for (ElemT* origin_elem = from_begin; origin_elem < from_end;)
                    {
                        (origin_elem++)->~ElemT();
                    }
                }

                return (size_t)(from_end - from_begin);
            }
            inline void _reserve(size_t elem_reserving_count)
            {
                const size_t _pre_reserved_count = (size_t)(_elems_buf_end - _elems_ptr_begin);
                if (elem_reserving_count > _pre_reserved_count)
                {
                    ElemT* new_reserved_begin = (ElemT*)je_mem_alloc(elem_reserving_count * _single_elem_size);
                    _elems_buf_end = new_reserved_begin + elem_reserving_count;

                    _elems_ptr_end = new_reserved_begin + _move(new_reserved_begin, _elems_ptr_begin, _elems_ptr_end);
                    je_mem_free(_elems_ptr_begin);
                    _elems_ptr_begin = new_reserved_begin;
                }
            }
            inline void _assure(size_t assure_sz)
            {
                if (assure_sz > reserved_size())
                    _reserve(2 * assure_sz);
            }

        public:
            vector() noexcept
            {
            }
            ~vector() noexcept
            {
                clear();
                je_mem_free(_elems_ptr_begin);
            }

            vector(const vector& another_list) noexcept
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());
            }
            vector(const std::initializer_list<ElemT>& another_list) noexcept
            {
                for (auto& elem : another_list)
                    push_back(elem);
            }
            vector(ElemT* ptr, size_t length) noexcept
            {
                _elems_ptr_begin = ptr;
                _elems_ptr_end = _elems_buf_end = _elems_ptr_begin + length;
            }
            vector(vector&& another_list) noexcept
            {
                _elems_ptr_begin = another_list._elems_ptr_begin;
                _elems_ptr_end = another_list._elems_ptr_end;
                _elems_buf_end = another_list._elems_buf_end;

                another_list._elems_ptr_begin =
                    another_list._elems_ptr_end =
                    another_list._elems_buf_end = nullptr;
            }

            inline vector& operator=(const vector& another_list) noexcept
            {
                _reserve(another_list.size());
                _elems_ptr_end += _copy(_elems_ptr_begin, another_list.begin(), another_list.end());

                return *this;
            }
            inline vector& operator=(vector&& another_list) noexcept
            {
                clear();
                je_mem_free(_elems_ptr_begin);

                _elems_ptr_begin = another_list._elems_ptr_begin;
                _elems_ptr_end = another_list._elems_ptr_end;
                _elems_buf_end = another_list._elems_buf_end;

                another_list._elems_ptr_begin =
                    another_list._elems_ptr_end =
                    another_list._elems_buf_end = nullptr;

                return *this;
            }

            inline size_t size() const noexcept
            {
                return _elems_ptr_end - _elems_ptr_begin;
            }
            inline bool empty() const noexcept
            {
                return _elems_ptr_end == _elems_ptr_begin;
            }
            inline ElemT& front() noexcept
            {
                return *_elems_ptr_begin;
            }
            inline ElemT& back() noexcept
            {
                return *(_elems_ptr_end - 1);
            }
            inline size_t reserved_size() const noexcept
            {
                return _elems_buf_end - _elems_ptr_begin;
            }
            inline void clear() noexcept
            {
                _erase(_elems_ptr_begin, _elems_ptr_end);
                _elems_ptr_end = _elems_ptr_begin;
            }
            inline void push_front(const ElemT& _e) noexcept
            {
                _assure(size() + 1);
                _r_move(_elems_ptr_begin + 1, _elems_ptr_begin, _elems_ptr_end++);
                new (_elems_ptr_begin) ElemT(_e);
            }
            inline void push_back(const ElemT& _e) noexcept
            {
                _assure(size() + 1);
                new (_elems_ptr_end++) ElemT(_e);
            }
            inline ElemT& emplace_back()
            {
                _assure(size() + 1);
                new (_elems_ptr_end) ElemT();
                return *(_elems_ptr_end++);
            }
            inline ElemT& emplace_back(ElemT&& _e)
            {
                _assure(size() + 1);
                new (_elems_ptr_end) ElemT(std::move(_e));
                return *(_elems_ptr_end++);
            }
            inline void pop_back() noexcept
            {
                if constexpr (!std::is_trivial_v<ElemT>)
                    (_elems_ptr_end--)->~ElemT();
                else
                    _elems_ptr_end--;
            }
            inline auto begin() const noexcept -> ElemT*
            {
                return _elems_ptr_begin;
            }
            inline auto end() const noexcept -> ElemT*
            {
                return _elems_ptr_end;
            }
            inline auto front() const noexcept -> ElemT&
            {
                return *_elems_ptr_begin;
            }
            inline auto back() const noexcept -> ElemT&
            {
                return *(_elems_ptr_end - 1);
            }
            inline void erase(size_t index) noexcept
            {
                _elems_ptr_begin[index].~ElemT();
                _move(_elems_ptr_begin + index, _elems_ptr_begin + index + 1, _elems_ptr_end--);
            }
            inline void erase(ElemT* index) noexcept
            {
                index->~ElemT();
                _move(index, index + 1, _elems_ptr_end--);
            }
            inline void erase_data(const ElemT& data) noexcept
            {
                auto fnd_place = std::find(begin(), end(), data);
                if (fnd_place != end())
                    erase(fnd_place - begin());
            }
            ElemT* data() const noexcept
            {
                return _elems_ptr_begin;
            }
            ElemT& at(size_t index) const noexcept
            {
                return _elems_ptr_begin[index];
            }
            ElemT& operator[](size_t index) const noexcept
            {
                return _elems_ptr_begin[index];
            }
        };

        class string;

        namespace _detail
        {
            template <typename T>
            inline size_t default_hash(const T& key) noexcept
            {
                return std::hash<T>{}(key);
            }
        }

        constexpr je_type_hash_t prime = (je_type_hash_t)0x100000001B3ull;
        constexpr je_type_hash_t basis = (je_type_hash_t)0xCBF29CE484222325ull;

        /*
        jeecs::basic::hash_compile_time [函数]
        可在编译时计算字符串的哈希值的哈希函数
        */
        constexpr je_type_hash_t hash_compile_time(
            char const* str, je_type_hash_t last_value = basis)
        {
            return *str ? hash_compile_time(str + 1, (*str ^ last_value) * prime) : last_value;
        }

        /*
        jeecs::basic::map [类型]
        用于存放大小可变的唯一键值对
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::map
            * 采用开放寻址（线性探测）的哈希表实现
        */
        template <typename KeyT, typename ValT>
        class map
        {
        public:
            struct pair
            {
                KeyT k;
                ValT v;
            };

        private:
            enum class slot_state : uint8_t
            {
                EMPTY = 0,
                OCCUPIED = 1,
                TOMBSTONE = 2,
            };

            struct slot
            {
                slot_state state;
                size_t cached_hash;
                alignas(pair) unsigned char storage[sizeof(pair)];
            };

            slot* m_slots = nullptr;
            size_t m_capacity = 0;
            size_t m_size = 0;
            size_t m_tombstones = 0;

            static constexpr size_t MIN_CAPACITY = 16;
            static constexpr size_t LOAD_FACTOR_NUM = 3;
            static constexpr size_t LOAD_FACTOR_DEN = 4;

            inline static pair* _slot_pair(slot& s) noexcept
            {
                return std::launder(reinterpret_cast<pair*>(s.storage));
            }
            inline static const pair* _slot_pair(const slot& s) noexcept
            {
                return std::launder(reinterpret_cast<const pair*>(s.storage));
            }

            inline static size_t _hash_key(const KeyT& k) noexcept
            {
                if constexpr (std::is_same_v<std::decay_t<KeyT>, basic::string>)
                {
                    return (size_t)hash_compile_time(k.c_str());
                }
                else
                {
                    return _detail::default_hash(k);
                }
            }

            void _deallocate() noexcept
            {
                if (m_slots != nullptr)
                {
                    for (size_t i = 0; i < m_capacity; ++i)
                    {
                        if (m_slots[i].state == slot_state::OCCUPIED)
                            _slot_pair(m_slots[i])->~pair();
                    }
                    je_mem_free(m_slots);
                    m_slots = nullptr;
                }
                m_capacity = 0;
                m_size = 0;
                m_tombstones = 0;
            }

            void _alloc_slots(size_t cap) noexcept
            {
                m_slots = (slot*)je_mem_alloc(cap * sizeof(slot));
                m_capacity = cap;
                m_size = 0;
                m_tombstones = 0;
                for (size_t i = 0; i < cap; ++i)
                    m_slots[i].state = slot_state::EMPTY;
            }

            void _rehash(size_t new_cap) noexcept
            {
                slot* old_slots = m_slots;
                size_t old_cap = m_capacity;

                m_slots = (slot*)je_mem_alloc(new_cap * sizeof(slot));
                m_capacity = new_cap;
                m_size = 0;
                m_tombstones = 0;

                for (size_t i = 0; i < new_cap; ++i)
                    m_slots[i].state = slot_state::EMPTY;

                for (size_t i = 0; i < old_cap; ++i)
                {
                    if (old_slots[i].state == slot_state::OCCUPIED)
                    {
                        const size_t h = old_slots[i].cached_hash;
                        size_t idx = h & (new_cap - 1);
                        while (m_slots[idx].state == slot_state::OCCUPIED)
                            idx = (idx + 1) & (new_cap - 1);

                        new (m_slots[idx].storage) pair(std::move(*_slot_pair(old_slots[i])));
                        _slot_pair(old_slots[i])->~pair();
                        m_slots[idx].state = slot_state::OCCUPIED;
                        m_slots[idx].cached_hash = h;
                        ++m_size;
                    }
                }

                if (old_slots != nullptr)
                    je_mem_free(old_slots);
            }

            inline void _ensure_cap_for_insert() noexcept
            {
                const size_t used_after = m_size + m_tombstones + 1;
                const size_t threshold = (m_capacity * LOAD_FACTOR_NUM) / LOAD_FACTOR_DEN;

                if (m_capacity == 0 || used_after > threshold)
                {
                    if (m_capacity == 0)
                    {
                        _alloc_slots(MIN_CAPACITY);
                    }
                    else if (m_tombstones > m_size)
                    {
                        // 多数浪费来自墓碑，原地 rehash 即可
                        _rehash(m_capacity);
                    }
                    else
                    {
                        _rehash(m_capacity * 2);
                    }
                }
            }

            const slot* _find_slot(const KeyT& k) const noexcept
            {
                if (m_capacity == 0)
                    return nullptr;

                const size_t h = _hash_key(k);
                const size_t mask = m_capacity - 1;
                size_t idx = h & mask;
                const size_t start = idx;

                do
                {
                    slot& s = m_slots[idx];
                    if (s.state == slot_state::EMPTY)
                        return nullptr;
                    if (s.state == slot_state::OCCUPIED
                        && s.cached_hash == h
                        && _slot_pair(s)->k == k)
                    {
                        return &s;
                    }
                    idx = (idx + 1) & mask;
                } while (idx != start);

                return nullptr;
            }

            slot* _find_slot(const KeyT& k) noexcept
            {
                return const_cast<slot*>(
                    static_cast<const map&>(*this)._find_slot(k));
            }

        public:
            template <bool IsConst>
            class basic_iterator
            {
                friend class map;
                using slot_ptr_t = std::conditional_t<IsConst, const slot*, slot*>;

                slot_ptr_t m_cur;
                slot_ptr_t m_end_slot;

                basic_iterator(slot_ptr_t cur, slot_ptr_t end_slot, bool /*skip_to_occupied*/) noexcept
                    : m_cur(cur), m_end_slot(end_slot)
                {
                    while (m_cur < m_end_slot && m_cur->state != slot_state::OCCUPIED)
                        ++m_cur;
                }

            public:
                basic_iterator(slot_ptr_t cur, slot_ptr_t end_slot) noexcept
                    : m_cur(cur), m_end_slot(end_slot)
                {
                }

                basic_iterator& operator++() noexcept
                {
                    do
                    {
                        ++m_cur;
                    } while (m_cur < m_end_slot && m_cur->state != slot_state::OCCUPIED);
                    return *this;
                }

                auto& operator*() const noexcept
                {
                    return *_slot_pair(*m_cur);
                }
                auto* operator->() const noexcept
                {
                    return _slot_pair(*m_cur);
                }

                bool operator==(const basic_iterator& o) const noexcept { return m_cur == o.m_cur; }
                bool operator!=(const basic_iterator& o) const noexcept { return m_cur != o.m_cur; }
            };

            using iterator = basic_iterator<false>;
            using const_iterator = basic_iterator<true>;

            map() noexcept = default;

            ~map() noexcept
            {
                _deallocate();
            }

            map(const map& o) noexcept
            {
                if (o.m_capacity > 0)
                {
                    _alloc_slots(o.m_capacity);
                    for (size_t i = 0; i < o.m_capacity; ++i)
                    {
                        if (o.m_slots[i].state == slot_state::OCCUPIED)
                        {
                            slot& dst = m_slots[i];
                            dst.state = slot_state::OCCUPIED;
                            dst.cached_hash = o.m_slots[i].cached_hash;
                            new (dst.storage) pair(*_slot_pair(o.m_slots[i]));
                            ++m_size;
                        }
                    }
                }
            }

            map(map&& o) noexcept
                : m_slots(o.m_slots)
                , m_capacity(o.m_capacity)
                , m_size(o.m_size)
                , m_tombstones(o.m_tombstones)
            {
                o.m_slots = nullptr;
                o.m_capacity = 0;
                o.m_size = 0;
                o.m_tombstones = 0;
            }

            map& operator=(const map& o) noexcept
            {
                if (this != &o)
                {
                    _deallocate();
                    if (o.m_capacity > 0)
                    {
                        _alloc_slots(o.m_capacity);
                        for (size_t i = 0; i < o.m_capacity; ++i)
                        {
                            if (o.m_slots[i].state == slot_state::OCCUPIED)
                            {
                                slot& dst = m_slots[i];
                                dst.state = slot_state::OCCUPIED;
                                dst.cached_hash = o.m_slots[i].cached_hash;
                                new (dst.storage) pair(*_slot_pair(o.m_slots[i]));
                                ++m_size;
                            }
                        }
                    }
                }
                return *this;
            }

            map& operator=(map&& o) noexcept
            {
                if (this != &o)
                {
                    _deallocate();
                    m_slots = o.m_slots;
                    m_capacity = o.m_capacity;
                    m_size = o.m_size;
                    m_tombstones = o.m_tombstones;

                    o.m_slots = nullptr;
                    o.m_capacity = 0;
                    o.m_size = 0;
                    o.m_tombstones = 0;
                }
                return *this;
            }

            ValT& operator[](const KeyT& k) noexcept
            {
                if (slot* found = _find_slot(k); found != nullptr)
                    return _slot_pair(*found)->v;

                _ensure_cap_for_insert();

                const size_t h = _hash_key(k);
                const size_t mask = m_capacity - 1;
                size_t idx = h & mask;
                slot* first_tombstone = nullptr;

                while (true)
                {
                    slot& s = m_slots[idx];
                    if (s.state == slot_state::EMPTY)
                    {
                        slot* target = first_tombstone != nullptr ? first_tombstone : &s;
                        target->state = slot_state::OCCUPIED;
                        target->cached_hash = h;
                        new (target->storage) pair{ k, ValT{} };
                        if (target != &s)
                            --m_tombstones;
                        ++m_size;
                        return _slot_pair(*target)->v;
                    }
                    if (s.state == slot_state::TOMBSTONE && first_tombstone == nullptr)
                        first_tombstone = &s;

                    idx = (idx + 1) & mask;
                }
            }

            iterator find(const KeyT& k) noexcept
            {
                slot* s = _find_slot(k);
                if (s == nullptr)
                    return end();
                return iterator(s, m_slots + m_capacity, true);
            }

            const_iterator find(const KeyT& k) const noexcept
            {
                const slot* s = _find_slot(k);
                if (s == nullptr)
                    return end();
                return const_iterator(s, m_slots + m_capacity, true);
            }

            bool erase(const KeyT& k) noexcept
            {
                slot* s = _find_slot(k);
                if (s == nullptr)
                    return false;

                _slot_pair(*s)->~pair();
                s->state = slot_state::TOMBSTONE;
                --m_size;
                ++m_tombstones;
                return true;
            }

            void clear() noexcept
            {
                for (size_t i = 0; i < m_capacity; ++i)
                {
                    if (m_slots[i].state == slot_state::OCCUPIED)
                        _slot_pair(m_slots[i])->~pair();
                    m_slots[i].state = slot_state::EMPTY;
                }
                m_size = 0;
                m_tombstones = 0;
            }

            iterator begin() noexcept
            {
                if (m_capacity == 0)
                    return end();
                return iterator(m_slots, m_slots + m_capacity, true);
            }

            const_iterator begin() const noexcept
            {
                if (m_capacity == 0)
                    return end();
                return const_iterator(m_slots, m_slots + m_capacity, true);
            }

            iterator end() noexcept
            {
                return iterator(m_slots + m_capacity, m_slots + m_capacity);
            }

            const_iterator end() const noexcept
            {
                return const_iterator(m_slots + m_capacity, m_slots + m_capacity);
            }

            inline size_t size() const noexcept
            {
                return m_size;
            }

            inline bool empty() const noexcept
            {
                return m_size == 0;
            }
        };

        /*
        jeecs::basic::string [类型]
        用于存放字符串
            * 为了保证模块之间的二进制一致性，公共组件中请不要使用std::string
        */
        class string
        {
            char* _c_str = nullptr;
            size_t _str_len = 0;
            size_t _buf_len = 0;

            inline void _reserve(size_t buf_sz)
            {
                if (buf_sz > _buf_len)
                {
                    _buf_len = buf_sz + 1;
                    _c_str = (char*)je_mem_realloc(_c_str, _buf_len);
                }
            }

        public:
            ~string()
            {
                if (_c_str != nullptr)
                    je_mem_free(_c_str);
            }

            string() noexcept
            {
                _reserve(1);
            }

            string(const string& str) noexcept
                : string(str.c_str())
            {
            }

            string(const std::string& str) noexcept
                : string(str.c_str())
            {
            }

            string(string&& str) noexcept
                : _c_str(str._c_str)
                , _str_len(str._str_len)
                , _buf_len(str._buf_len)
            {
                str._c_str = 0;
                str._buf_len = 0;
                str._str_len = 0;
            }

            string(const char* str) noexcept
                : _str_len(strlen(str))
            {
                _reserve(_str_len + 1);
                memcpy(_c_str, str, _str_len);
            }

            inline bool operator==(const string& str) noexcept
            {
                return 0 == strcmp(c_str(), str.c_str());
            }
            inline bool operator!=(const string& str) noexcept
            {
                return 0 != strcmp(c_str(), str.c_str());
            }
            inline string& operator=(const string& str) noexcept
            {
                return *this = str.c_str();
            }
            inline string& operator=(string&& str) noexcept
            {
                je_mem_free(_c_str);

                _c_str = str._c_str;
                _buf_len = str._buf_len;
                _str_len = str._str_len;

                str._c_str = 0;
                str._buf_len = 0;
                str._str_len = 0;

                return *this;
            }
            inline string& operator=(const std::string& str)
            {
                return *this = str.c_str();
            }
            inline string& operator=(const char* str)
            {
                _reserve((_str_len = strlen(str)) + 1);
                memcpy(_c_str, str, _str_len);
                return *this;
            }

            size_t size() const
            {
                return _str_len;
            }
            const char* c_str() const
            {
                _c_str[_str_len] = 0;
                return _c_str;
            }
            std::string cpp_str() const
            {
                return c_str();
            }
        };

        /*
        jeecs::basic::allign_size [函数]
        用于计算满足对齐要求的下一个起始偏移量位置
        */
        constexpr static size_t allign_size(size_t _origin_sz, size_t _allign_base)
        {
            assert(_allign_base != 0 && (_allign_base & (_allign_base - 1)) == 0);
            return (_origin_sz + _allign_base - 1) & ~(_allign_base - 1);
        }

        template <typename T, typename... ArgTs>
        T* create_new(ArgTs &&...args)
        {
            static_assert(!std::is_void_v<T>);

            return new (je_mem_alloc(sizeof(T))) T(args...);
        }
        template <typename T, typename... ArgTs>
        T* create_new_n(size_t n)
        {
            static_assert(!std::is_void_v<T>);

            return new (je_mem_alloc(sizeof(T) * n)) T[n];
        }

        template <typename T>
        void destroy_free(T* address)
        {
            static_assert(!std::is_void_v<T>);

            address->~T();
            je_mem_free(address);
        }
        template <typename T>
        void destroy_free_n(T* address, size_t n)
        {
            static_assert(!std::is_void_v<T>);

            for (size_t i = 0; i < n; i++)
                address[i].~T();

            je_mem_free(address);
        }

        inline char* make_new_string(const char* _str)
        {
            size_t str_length = strlen(_str);
            char* str = (char*)je_mem_alloc(str_length + 1);
            memcpy(str, _str, str_length + 1);

            return str;
        }
        inline char* make_new_string(const std::string& _str)
        {
            return make_new_string(_str.c_str());
        }
        inline char* make_new_string(const basic::string& _str)
        {
            return make_new_string(_str.c_str());
        }

        inline std::string make_cpp_string(const char* _str)
        {
            std::string str = _str;
            je_mem_free((void*)_str);
            return str;
        }

        template <typename NodeT>
        struct atomic_list
        {
            std::atomic<NodeT*> last_node = nullptr;

            void add_one(NodeT* node) noexcept
            {
                NodeT* last_last_node = last_node.load(); // .exchange(node);
                do
                {
                    node->last = last_last_node;
                } while (!last_node.compare_exchange_weak(last_last_node, node));
            }

            NodeT* pick_all() noexcept
            {
                NodeT* result = nullptr;
                result = last_node.exchange(nullptr);

                return result;
            }

            NodeT* peek() const noexcept
            {
                return last_node;
            }

            bool empty() const noexcept
            {
                return last_node == nullptr;
            }
        };

        template <typename T>
        constexpr auto type_hash()
        {
            return typeid(T).hash_code();
        }

        template <typename... ArgTs>
        struct type_index_in_varargs
        {
            template <size_t Index, typename AimT, typename CurrentT, typename... Ts>
            constexpr static size_t _index()
            {
                if constexpr (std::is_same_v<AimT, CurrentT>)
                    return Index;
                else
                    return _index<Index + 1, AimT, Ts...>();
            }

            template <typename AimArgT>
            constexpr size_t index_of() const noexcept
            {
                return _index<0, AimArgT, ArgTs...>();
            }
        };

        /*
        jeecs::basic::shared_pointer [类型]
        线程安全的共享智能指针类型
        */
        template <typename T>
        class shared_pointer
        {
            using count_t = size_t;
            using free_func_t = void (*)(T*);

            static void _default_free_func(T* ptr)
            {
                delete ptr;
            }

            T* m_resource = nullptr;
            mutable count_t* m_count = nullptr;
            free_func_t m_freer = nullptr;

            inline const static count_t* _COUNT_USING_SPIN_LOCK_MARK = (count_t*)SIZE_MAX;

            static count_t* _alloc_counter()
            {
                return create_new<count_t>(1);
            }
            static void _free_counter(count_t* p)
            {
                destroy_free(p);
            }

            count_t* _spin_lock() const
            {
                count_t* result;
                do
                {
                    result = (count_t*)je_atomic_exchange_intptr_t(
                        (intptr_t*)&m_count, (intptr_t)_COUNT_USING_SPIN_LOCK_MARK);
                } while (result == _COUNT_USING_SPIN_LOCK_MARK);

                return result;
            }
            void _spin_unlock(count_t* p) const
            {
                je_atomic_exchange_intptr_t(
                    (intptr_t*)&m_count, (intptr_t)p);
            }

            static count_t* _release_nolock_impl(T** ref_resource, free_func_t _freer, count_t* count)
            {
                if (count != nullptr)
                {
                    if (1 == je_atomic_fetch_sub_size_t(count, 1))
                    {
                        _freer(*ref_resource);
                        _free_counter(count);
                        *ref_resource = nullptr;
                        count = nullptr;
                    }
                }
                return count;
            }

            // This function not lock!
            count_t* _release_nolock(count_t* count)
            {
                return _release_nolock_impl(&m_resource, m_freer, count);
            }
            count_t* _move_nolock(count_t* count, shared_pointer* out_ptr)
            {
                out_ptr->m_resource = m_resource;
                out_ptr->m_freer = m_freer;
                m_resource = nullptr;
                return nullptr;
            }
            count_t* _borrow_nolocks(count_t* count, shared_pointer* out_ptr) const
            {
                out_ptr->m_resource = m_resource;
                out_ptr->m_freer = m_freer;
                if (count != nullptr)
                    je_atomic_fetch_add_size_t(count, 1);

                return count;
            }

        public:
            ~shared_pointer()
            {
                _release_nolock((count_t*)je_atomic_fetch_intptr_t((intptr_t*)&m_count));
            }

            shared_pointer() = delete;
            explicit shared_pointer(T* v, void (*f)(T*) = &_default_free_func)
                : m_resource(v)
                , m_count(_alloc_counter())
                , m_freer(f)
            {
                assert(m_resource != nullptr);
            }
            shared_pointer(const shared_pointer& v) noexcept
            {
                auto* counter = v._spin_lock();
                auto* unlocker = v._borrow_nolocks(counter, this);
                je_atomic_store_intptr_t((intptr_t*)&m_count, (intptr_t)counter);
                v._spin_unlock(unlocker);
            }
            shared_pointer(shared_pointer&& v) noexcept
            {
                auto* counter = v._spin_lock();
                auto* unlocker = v._move_nolock(counter, this);
                je_atomic_store_intptr_t((intptr_t*)&m_count, (intptr_t)counter);
                v._spin_unlock(unlocker);
            }
            shared_pointer& operator=(const shared_pointer& v) noexcept
            {
                if (this != &v)
                {
                    _release_nolock(_spin_lock());
                    auto* counter = v._spin_lock();

                    auto* unlocker = v._borrow_nolocks(counter, this);

                    _spin_unlock(counter);
                    v._spin_unlock(unlocker);
                }
                return *this;
            }
            shared_pointer& operator=(shared_pointer&& v) noexcept
            {
                if (this != &v)
                {
                    _release_nolock(_spin_lock());
                    auto* counter = v._spin_lock();

                    auto* unlocker = v._move_nolock(counter, this);

                    _spin_unlock(counter);
                    v._spin_unlock(unlocker);
                }
                return *this;
            }

            T* get() const
            {
                return m_resource;
            }
            T* operator->() const noexcept
            {
                return m_resource;
            }
            T& operator*() const noexcept
            {
                return *m_resource;
            }
        };

        template <typename T>
        class optional
        {
            union
            {
                char takeplace;
                T storage;
            };

            bool has_constructed;

        public:
            ~optional()
            {
                if (has_constructed)
                    storage.~T();
            }

            optional() noexcept
                : takeplace(0), has_constructed(false)
            {
            }
            optional(const std::nullopt_t&) noexcept
                : takeplace(0), has_constructed(false)
            {
            }
            optional(const std::optional<T>& opt) noexcept
                : has_constructed(opt.has_value())
            {
                if (has_constructed)
                    new (&storage) T(opt.value());
            }
            optional(const T& v) noexcept
                : has_constructed(true)
            {
                new (&storage) T(v);
            }
            optional(T&& v) noexcept
                : has_constructed(true)
            {
                new (&storage) T(std::move(v));
            }
            optional(optional&& another) noexcept
                : has_constructed(another.has_constructed)
            {
                if (has_constructed)
                {
                    new (&storage) T(std::move(another.storage));
                    another.reset();
                }
            }
            optional(const optional& another) noexcept
                : has_constructed(another.has_constructed)
            {
                if (has_constructed)
                {
                    new (&storage) T(another.storage);
                }
            }
            optional& operator=(optional&& another) noexcept
            {
                if (this != &another)
                {
                    if (has_constructed)
                        storage.~T();

                    has_constructed = another.has_constructed;
                    if (has_constructed)
                    {
                        new (&storage) T(std::move(another.storage));
                        another.storage.~T();
                        another.has_constructed = false;
                    }
                }
                return *this;
            }
            optional& operator=(const optional& another) noexcept
            {
                if (this != &another)
                {
                    if (has_constructed)
                        storage.~T();

                    has_constructed = another.has_constructed;
                    if (has_constructed)
                    {
                        new (&storage) T(another.storage);
                    }
                }
                return *this;
            }

            bool has_value() const noexcept
            {
                return has_constructed;
            }
            T& value() noexcept
            {
                if (!has_constructed)
                {
                    jeecs::debug::logfatal(
                        "jeecs::basic::optional: value is not constructed, you should call emplace() first.");
                    abort();
                }
                return storage;
            }
            const T& value() const noexcept
            {
                if (!has_constructed)
                {
                    jeecs::debug::logfatal(
                        "jeecs::basic::optional: value is not constructed, you should call emplace() first.");
                    abort();
                }
                return storage;
            }
            void reset() noexcept
            {
                if (has_constructed)
                {
                    storage.~T();
                    has_constructed = false;
                }
            }
            template <typename... Args>
            T& emplace(Args &&...args) noexcept
            {
                if (has_constructed)
                    storage.~T();
                else
                    has_constructed = true;
                return *new (&storage) T(std::forward<Args>(args)...);
            }

            T* operator->() noexcept
            {
                return &value();
            }
            const T* operator->() const noexcept
            {
                return &value();
            }
        };

        /*
        jeecs::basic::resource [类型别名]
        智能指针的类型别名，一般用于保管需要共享和自动管理的资源类型
        */
        template <typename T>
        using resource = shared_pointer<T>;

        namespace traits
        {
            template <typename T>
            concept is_loadable_resource = requires(const std::string & path)
            {
                { T::load(path) } -> std::same_as<std::optional<basic::resource<T>>>;
            };
        }

        class file_resource_placeholder
        {
            JECS_DISABLE_MOVE_AND_COPY(file_resource_placeholder);
            file_resource_placeholder() = default;
        public:
            static std::optional<resource<file_resource_placeholder>> load(
                const std::string& path)
            {
                return resource<file_resource_placeholder>(new file_resource_placeholder());
            }
        };

        /*
        jeecs::basic::file_resource [类型]
        文件资源包装类型，用于组件内的成员变量
        */
        template <traits::is_loadable_resource T = file_resource_placeholder>
        class file_resource
        {
            struct file_content_t
            {
                basic::string m_path;
                basic::resource<T> m_resource;
            };
            basic::optional<file_content_t> m_file;
        public:
            auto reset() -> void
            {
                m_file.reset();
            }
            auto load(const std::string& path) -> bool
            {
                reset();
                if (!path.empty())
                {
                    auto res = T::load(path);
                    if (!res.has_value())
                        return false;

                    m_file.emplace(
                        file_content_t{
                            path,
                            res.value(),
                        });
                }
                return true;
            }
            auto set_resource(const basic::resource<T>& res) -> void
            {
                m_file.emplace(
                    file_content_t{
                        "<builtin>",
                        res,
                    });
            }
            auto has_resource() const -> bool
            {
                return m_file.has_value();
            }
            auto get_resource() const -> std::optional<basic::resource<T>>
            {
                if (m_file.has_value())
                    return m_file->m_resource;

                return std::nullopt;
            }
            auto get_path() const -> std::optional<std::string>
            {
                if (m_file.has_value())
                    return m_file->m_path.cpp_str();

                return std::nullopt;
            }

        public:
            static const char* JEScriptTypeName()
            {
                return "file_resource";
            }
            static const char* JEScriptTypeDeclare()
            {
                return "public using file_resource = struct{public path: option<string>};";
            }
            void JEParseFromScriptType(woort_value v)
            {
                woort_value path;
                if (!woort_push_reserve(1, &path))
                    woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                else
                {
                    woort_struct_get(path, v, 0);
                    if (woort_option_get(path, path))
                        (void)load(woort_string(path));
                    else
                        reset();

                    woort_pop(1);
                }
            }
            void JEParseToScriptType(woort_value v) const
            {
                woort_value field;
                if (!woort_push_reserve(1, &field))
                    woort_panic(WOORT_PANIC_STACK_OVERFLOW, "Stack overflow");
                else
                {
                    auto path = get_path();
                    if (path.has_value())
                        woort_set_option_string(field, path.value().c_str());
                    else
                        woort_set_option_none(field);

                    woort_set_struct(v, 1);
                    woort_struct_set(v, 0, field);

                    woort_pop(1);
                }
            }
        };
    }
}
#endif // __cplusplus
