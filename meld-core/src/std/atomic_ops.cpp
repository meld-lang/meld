#include "meld/std/atomic_ops.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

namespace meld::stdx {

// Explicit instantiations for common atomic types
template class Atomic<bool>;
template class Atomic<int>;
template class Atomic<long>;
template class Atomic<float>;
template class Atomic<double>;
template class Atomic<std::size_t>;
template class Atomic<std::ptrdiff_t>;

template class Atomic<int*>;
template class Atomic<double*>;

template class AtomicPtr<int>;
template class AtomicPtr<std::string>;

// Utility functions for atomic operations
namespace atomic_utils {

// Lock-free stack implementation using atomic operations
template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        
        Node(T data) : data(std::move(data)), next(nullptr) {}
    };
    
    Atomic<Node*> head_;
    
public:
    LockFreeStack() : head_(nullptr) {}
    
    ~LockFreeStack() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head;
        }
    }
    
    void push(T item) {
        Node* new_node = new Node(std::move(item));
        new_node->next = head_.load();
        
        while (!head_.compare_exchange_weak(new_node->next, new_node)) {
            // Retry with updated next pointer
        }
    }
    
    bool pop(T& result) {
        Node* old_head = head_.load();
        
        while (old_head && !head_.compare_exchange_weak(old_head, old_head->next)) {
            // Retry
        }
        
        if (old_head) {
            result = std::move(old_head->data);
            delete old_head;
            return true;
        }
        
        return false;
    }
    
    bool empty() const {
        return head_.load() == nullptr;
    }
};

// Lock-free queue implementation (simplified)
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        Atomic<T*> data;
        Atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    Atomic<Node*> head_;
    Atomic<Node*> tail_;
    
public:
    LockFreeQueue() {
        Node* dummy = new Node;
        head_.store(dummy);
        tail_.store(dummy);
    }
    
    ~LockFreeQueue() {
        while (Node* old_head = head_.load()) {
            head_.store(old_head->next);
            delete old_head;
        }
    }
    
    void enqueue(T item) {
        Node* new_node = new Node;
        T* data = new T(std::move(item));
        new_node->data.store(data);
        
        while (true) {
            Node* last = tail_.load();
            Node* next = last->next.load();
            
            if (last == tail_.load()) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, new_node)) {
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(last, next);
                }
            }
        }
        
        tail_.compare_exchange_weak(tail_.load(), new_node);
    }
    
    bool dequeue(T& result) {
        while (true) {
            Node* first = head_.load();
            Node* last = tail_.load();
            Node* next = first->next.load();
            
            if (first == head_.load()) {
                if (first == last) {
                    if (next == nullptr) {
                        return false; // Queue is empty
                    }
                    tail_.compare_exchange_weak(last, next);
                } else {
                    if (next == nullptr) {
                        continue;
                    }
                    
                    T* data = next->data.load();
                    if (data == nullptr) {
                        continue;
                    }
                    
                    if (head_.compare_exchange_weak(first, next)) {
                        result = *data;
                        delete data;
                        delete first;
                        return true;
                    }
                }
            }
        }
    }
};

// Atomic counter with statistics
class AtomicCounter {
private:
    AtomicU64 value_;
    AtomicU64 increments_;
    AtomicU64 decrements_;
    
public:
    AtomicCounter(uint64_t initial = 0) 
        : value_(initial), increments_(0), decrements_(0) {}
    
    uint64_t increment() {
        increments_.fetch_add(1, MemoryOrdering::Relaxed);
        return value_.fetch_add(1, MemoryOrdering::AcqRel) + 1;
    }
    
    uint64_t decrement() {
        decrements_.fetch_add(1, MemoryOrdering::Relaxed);
        return value_.fetch_sub(1, MemoryOrdering::AcqRel) - 1;
    }
    
    uint64_t get() const {
        return value_.load(MemoryOrdering::Acquire);
    }
    
    uint64_t get_increments() const {
        return increments_.load(MemoryOrdering::Relaxed);
    }
    
    uint64_t get_decrements() const {
        return decrements_.load(MemoryOrdering::Relaxed);
    }
    
    void reset() {
        value_.store(0, MemoryOrdering::Release);
        increments_.store(0, MemoryOrdering::Relaxed);
        decrements_.store(0, MemoryOrdering::Relaxed);
    }
};

// Demonstration functions
void demonstrate_basic_atomics() {
    std::cout << "=== Basic Atomic Operations Demo ===" << std::endl;
    
    AtomicI32 counter(0);
    AtomicBool flag(false);
    
    std::cout << "Initial counter: " << counter.load() << std::endl;
    std::cout << "Initial flag: " << flag.load() << std::endl;
    
    // Basic operations
    counter.store(42);
    flag.store(true);
    
    std::cout << "After store - counter: " << counter.load() << ", flag: " << flag.load() << std::endl;
    
    // Arithmetic operations
    int old_value = counter.fetch_add(10);
    std::cout << "fetch_add(10) returned: " << old_value << ", new value: " << counter.load() << std::endl;
    
    counter += 5;
    std::cout << "After += 5: " << counter.load() << std::endl;
    
    ++counter;
    std::cout << "After ++: " << counter.load() << std::endl;
    
    // Exchange
    int exchanged = counter.exchange(100);
    std::cout << "exchange(100) returned: " << exchanged << ", new value: " << counter.load() << std::endl;
    
    // Compare and exchange
    int expected = 100;
    bool success = counter.compare_exchange_strong(expected, 200);
    std::cout << "compare_exchange_strong(100, 200) success: " << success 
              << ", value: " << counter.load() << std::endl;
    
    expected = 150;
    success = counter.compare_exchange_strong(expected, 300);
    std::cout << "compare_exchange_strong(150, 300) success: " << success 
              << ", expected was: " << expected << ", value: " << counter.load() << std::endl;
}

void demonstrate_memory_ordering() {
    std::cout << "\n=== Memory Ordering Demo ===" << std::endl;
    
    AtomicI32 data(0);
    AtomicBool ready(false);
    
    // Producer thread
    std::thread producer([&]() {
        data.store(42, MemoryOrdering::Relaxed);
        ready.store(true, MemoryOrdering::Release); // Release ensures data write is visible
        std::cout << "Producer: Data written and ready flag set" << std::endl;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        while (!ready.load(MemoryOrdering::Acquire)) { // Acquire ensures we see data write
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        int value = data.load(MemoryOrdering::Relaxed);
        std::cout << "Consumer: Read data value: " << value << std::endl;
    });
    
    producer.join();
    consumer.join();
}

void demonstrate_lock_free_stack() {
    std::cout << "\n=== Lock-Free Stack Demo ===" << std::endl;
    
    LockFreeStack<int> stack;
    
    // Multiple producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < 3; ++i) {
        producers.emplace_back([&stack, i]() {
            for (int j = 0; j < 5; ++j) {
                int value = i * 10 + j;
                stack.push(value);
                std::cout << "Producer " << i << " pushed: " << value << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Multiple consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&stack, i]() {
            int value;
            int count = 0;
            while (count < 7) { // Each consumer tries to get ~7 items
                if (stack.pop(value)) {
                    std::cout << "Consumer " << i << " popped: " << value << std::endl;
                    count++;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        });
    }
    
    for (auto& p : producers) p.join();
    for (auto& c : consumers) c.join();
    
    // Clean up remaining items
    int remaining;
    std::cout << "Remaining items: ";
    while (stack.pop(remaining)) {
        std::cout << remaining << " ";
    }
    std::cout << std::endl;
}

void demonstrate_atomic_flag() {
    std::cout << "\n=== Atomic Flag Demo ===" << std::endl;
    
    AtomicFlag flag;
    AtomicI32 shared_resource(0);
    
    std::vector<std::thread> threads;
    
    // Multiple threads trying to access a shared resource
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&flag, &shared_resource, i]() {
            for (int j = 0; j < 3; ++j) {
                // Acquire the lock
                while (flag.test_and_set(MemoryOrdering::Acquire)) {
                    // Spin wait
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                }
                
                // Critical section
                int old_value = shared_resource.load();
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate work
                shared_resource.store(old_value + 1);
                std::cout << "Thread " << i << " incremented resource to: " 
                          << shared_resource.load() << std::endl;
                
                // Release the lock
                flag.clear(MemoryOrdering::Release);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final resource value: " << shared_resource.load() << std::endl;
}

void demonstrate_spin_wait() {
    std::cout << "\n=== Spin Wait Demo ===" << std::endl;
    
    AtomicBool ready(false);
    AtomicI32 data(0);
    
    std::thread producer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        data.store(12345);
        ready.store(true);
        std::cout << "Producer: Data is ready" << std::endl;
    });
    
    std::thread consumer([&]() {
        SpinWait spin_wait;
        
        while (!ready.load(MemoryOrdering::Acquire)) {
            spin_wait.spin_once();
        }
        
        int value = data.load();
        std::cout << "Consumer: Got data: " << value << std::endl;
    });
    
    producer.join();
    consumer.join();
}

} // namespace atomic_utils

// Explicit instantiations of utility classes
template class atomic_utils::LockFreeStack<int>;
template class atomic_utils::LockFreeStack<std::string>;
template class atomic_utils::LockFreeQueue<int>;
template class atomic_utils::LockFreeQueue<std::string>;

} // namespace meld::stdx