//
// Created by tate on 8/2/26.
//

#ifndef FIY_POOL_HPP
#define FIY_POOL_HPP

#include <deque>
#include <mutex>
#include <unordered_set>
#include <set>
#include <vector>

/// Use this macro to change the container used
#ifndef FIY_POOL_CONTAINER_T
#define FIY_POOL_CONTAINER_T std::deque
#endif

/// Set this macro to 1 to make the container thread safe with a mutex
#ifndef FIY_POOL_USE_MUTEX
#define FIY_POOL_USE_MUTEX 0
#endif

template <typename T>
struct Node {
    union {
        T value;
        Node* next_free = nullptr;
    };

    template<typename ...Args>
    Node(Args&&... args): value(std::forward<Args>(args)...) {

    }
};

/// Memory pool
template <class T>
class Pool {
    struct Node {
        union {
            T value;
            Node* next_free;
        };

        template<typename ...Args>
        Node(Args&&... args): value(std::forward<Args>(args)...) {}

        // I don't think this is needed
        // template<typename ...Args>
        // Node(Args&&... args) {
        //     ::new(&value) T(std::forward<Args>(args)...);
        // }

        ~Node() {}
    };

    using Container = FIY_POOL_CONTAINER_T<Node>;

    Container m_store;
    Node* m_first_free{nullptr};

#if FIY_POOL_USE_MUTEX
    std::mutex m_mtx;
#endif

public:

    Pool() = default;
    explicit Pool(const size_t n) {

#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif
        m_store.reserve(n);
    }

    ~Pool() {
        clear();
    }

    template<typename ...Args>
    T* emplace(Args&&... args) {
#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif
        if (m_first_free == nullptr) {
            auto it = m_store.emplace_back(std::forward<Args>(args)...);
            return &*it;
        } else {
            Node* p = m_first_free;
            m_first_free = p->next_free;
            ::new(p) T(std::forward<Args>(args)...);
            return &p->value;
        }
    }

    void remove(T* entity) {
        assert(entity != nullptr);

#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif

        Node* n = static_cast<Node*>(entity);
        n->value.~T();
        n->next_free = m_first_free;
        m_first_free = n;
    }

    void clear() {
        std::unordered_set<Node*> free_nodes;
#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif

        // Get a set of all free_nodes
        while (m_first_free != nullptr) {
            free_nodes.emplace(m_first_free);
            m_first_free = m_first_free->next_free;
        }
        // this leaves m_first_free == nullptr

        // Call destructor on all the nodes
        for (Node& n : m_store)
            if (!free_nodes.contains(&n))
                n.value.~T();

        m_store.clear();
    }

    /**
     * Attempt to reduce memory consumption
     * @remark should not invalidate pointers
     * @remark generally not recommended unless you know what you're doing
     */
    void squeeze() {
        std::unordered_set<Node*> free_nodes;
#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif

        // Get a set of all free_nodes
        Node* n = m_first_free;
        if (n == nullptr)
            return;
        while (n != nullptr) {
            free_nodes.emplace(n);
            n = n->next_free;
        }

        // All free
        if (free_nodes.size() == m_store.size()) {
            m_store.clear();
            m_first_free = nullptr;
            return;
        }

        std::deque<Node*> removed_nodes;

        // Find first node that's not free
        size_t first_in_use = 0;
        for (; first_in_use < m_store.size(); ++first_in_use)
            if (!free_nodes.contains(&m_store[first_in_use]))
                break;
            else
                removed_nodes.emplace(&m_store[first_in_use]);

        // Find last node that's not free
        size_t last_in_use = m_store.size() - 1;
        for (; last_in_use > first_in_use; --last_in_use)
            if (!free_nodes.contains(&m_store[last_in_use]))
                break;
            else
                removed_nodes.emplace(&m_store[last_in_use]);

        // Fix free list
        auto next_usable = [&](Node* node) {
            // if (node == nullptr)
            //     return nullptr;
            do {
                node = node->next_free;
            } while (node != nullptr && removed_nodes.contains(node));
            return node;
        };
        if (removed_nodes.contains(m_first_free))
            m_first_free = next_usable(m_first_free);
        n = m_first_free;
        while (n != nullptr) {
            if (removed_nodes.contains(n->next_free))
                n->next_free = next_usable(n->next_free);
            n = n->next_free;
        }

        // This should not invalidate pointers because only poping from front and back
        if (first_in_use != 0)
            m_store.erase(m_store.begin(), std::next(m_store.begin(), first_in_use));
        if (last_in_use != m_store.size() - 1)
            m_store.erase(std::next(m_store.begin(), last_in_use), m_store.end());

    }

    /**
     * Remove all unused space to save memory, invalidates pointers
     */
    void coalesce_unsafe() {
        Container new_container;
        std::unordered_set<Node*> free_nodes;
#if FIY_POOL_USE_MUTEX
        std::lock_guard lock(m_mtx);
#endif

        // Get a set of all free_nodes
        while (m_first_free != nullptr) {
            free_nodes.emplace(m_first_free);
            m_first_free = m_first_free->next_free;
        }
        // this leaves m_first_free == nullptr

        // Replace container with new one without holes
        for (Node& n : new_container)
            if (!free_nodes.contains(&n))
                new_container.emplace_back(std::move(n));
        m_store = new_container;
    }
};

#endif //FIY_POOL_HPP
