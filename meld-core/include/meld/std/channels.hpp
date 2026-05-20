#pragma once

#include "meld/std/concurrency_traits.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include <optional>
#include <chrono>
#include <exception>

namespace meld::stdx {

// Forward declarations
template<typename T> class Sender;
template<typename T> class Receiver;
template<typename T> class Channel;

// Channel error types
class SendError : public std::exception {
public:
    enum class Kind { Closed, Full };
    
    SendError(Kind kind) : kind_(kind) {}
    
    const char* what() const noexcept override {
        switch (kind_) {
            case Kind::Closed: return "Channel is closed";
            case Kind::Full: return "Channel is full";
            default: return "Unknown send error";
        }
    }
    
    Kind kind() const { return kind_; }
    
private:
    Kind kind_;
};

class RecvError : public std::exception {
public:
    enum class Kind { Closed, Empty };
    
    RecvError(Kind kind) : kind_(kind) {}
    
    const char* what() const noexcept override {
        switch (kind_) {
            case Kind::Closed: return "Channel is closed";
            case Kind::Empty: return "Channel is empty";
            default: return "Unknown receive error";
        }
    }
    
    Kind kind() const { return kind_; }
    
private:
    Kind kind_;
};

// Result types for channel operations
template<typename T>
using SendResult = std::optional<SendError>;

template<typename T>
using RecvResult = std::optional<T>;

template<typename T>
using TryRecvResult = std::optional<T>;

// Internal channel state
template<typename T>
class ChannelState {
    static_assert(is_send_v<T>, "Channel element type must be Send");
    
public:
    ChannelState(size_t capacity = 0) 
        : capacity_(capacity), closed_(false), sender_count_(1), receiver_count_(1) {}
    
    // Send a value to the channel
    SendResult<T> send(T&& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (closed_.load()) {
            return SendError(SendError::Kind::Closed);
        }
        
        // For unbounded channels (capacity == 0), always allow sending
        if (capacity_ == 0 || queue_.size() < capacity_) {
            queue_.push(std::move(value));
            not_empty_.notify_one();
            return std::nullopt; // Success
        }
        
        // Channel is full
        return SendError(SendError::Kind::Full);
    }
    
    // Send with blocking until space is available
    SendResult<T> send_blocking(T&& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until there's space or channel is closed
        not_full_.wait(lock, [this]() {
            return closed_.load() || capacity_ == 0 || queue_.size() < capacity_;
        });
        
        if (closed_.load()) {
            return SendError(SendError::Kind::Closed);
        }
        
        queue_.push(std::move(value));
        not_empty_.notify_one();
        return std::nullopt; // Success
    }
    
    // Receive a value from the channel
    RecvResult<T> recv() {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until there's data or channel is closed and empty
        not_empty_.wait(lock, [this]() {
            return !queue_.empty() || (closed_.load() && sender_count_.load() == 0);
        });
        
        if (!queue_.empty()) {
            T value = std::move(queue_.front());
            queue_.pop();
            not_full_.notify_one();
            return value;
        }
        
        // Channel is closed and empty
        return std::nullopt;
    }
    
    // Try to receive without blocking
    TryRecvResult<T> try_recv() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!queue_.empty()) {
            T value = std::move(queue_.front());
            queue_.pop();
            not_full_.notify_one();
            return value;
        }
        
        return std::nullopt;
    }
    
    // Receive with timeout
    template<typename Rep, typename Period>
    RecvResult<T> recv_timeout(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        bool success = not_empty_.wait_for(lock, timeout, [this]() {
            return !queue_.empty() || (closed_.load() && sender_count_.load() == 0);
        });
        
        if (success && !queue_.empty()) {
            T value = std::move(queue_.front());
            queue_.pop();
            not_full_.notify_one();
            return value;
        }
        
        return std::nullopt;
    }
    
    // Close the channel
    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_.store(true);
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }
    
    // Check if channel is closed
    bool is_closed() const {
        return closed_.load();
    }
    
    // Get current queue size
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }
    
    // Get channel capacity (0 means unbounded)
    size_t capacity() const {
        return capacity_;
    }
    
    // Reference counting for senders and receivers
    void add_sender() { sender_count_.fetch_add(1); }
    void remove_sender() { 
        if (sender_count_.fetch_sub(1) == 1) {
            // Last sender removed, notify receivers
            not_empty_.notify_all();
        }
    }
    
    void add_receiver() { receiver_count_.fetch_add(1); }
    void remove_receiver() { receiver_count_.fetch_sub(1); }
    
    size_t sender_count() const { return sender_count_.load(); }
    size_t receiver_count() const { return receiver_count_.load(); }
    
private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::queue<T> queue_;
    size_t capacity_;
    std::atomic<bool> closed_;
    std::atomic<size_t> sender_count_;
    std::atomic<size_t> receiver_count_;
};

// Sender handle for sending values to a channel
template<typename T>
class Sender {
    static_assert(is_send_v<T>, "Channel element type must be Send");
    
public:
    explicit Sender(std::shared_ptr<ChannelState<T>> state) : state_(std::move(state)) {}
    
    // Copy constructor
    Sender(const Sender& other) : state_(other.state_) {
        if (state_) {
            state_->add_sender();
        }
    }
    
    // Move constructor
    Sender(Sender&& other) noexcept : state_(std::move(other.state_)) {}
    
    // Copy assignment
    Sender& operator=(const Sender& other) {
        if (this != &other) {
            if (state_) {
                state_->remove_sender();
            }
            state_ = other.state_;
            if (state_) {
                state_->add_sender();
            }
        }
        return *this;
    }
    
    // Move assignment
    Sender& operator=(Sender&& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->remove_sender();
            }
            state_ = std::move(other.state_);
        }
        return *this;
    }
    
    ~Sender() {
        if (state_) {
            state_->remove_sender();
        }
    }
    
    // Send a value (non-blocking for bounded channels)
    SendResult<T> send(T value) {
        if (!state_) {
            return SendError(SendError::Kind::Closed);
        }
        return state_->send(std::move(value));
    }
    
    // Send a value (blocking until space is available)
    SendResult<T> send_blocking(T value) {
        if (!state_) {
            return SendError(SendError::Kind::Closed);
        }
        return state_->send_blocking(std::move(value));
    }
    
    // Close the channel
    void close() {
        if (state_) {
            state_->close();
        }
    }
    
    // Check if channel is closed
    bool is_closed() const {
        return !state_ || state_->is_closed();
    }
    
private:
    std::shared_ptr<ChannelState<T>> state_;
};

// Receiver handle for receiving values from a channel
template<typename T>
class Receiver {
    static_assert(is_send_v<T>, "Channel element type must be Send");
    
public:
    explicit Receiver(std::shared_ptr<ChannelState<T>> state) : state_(std::move(state)) {}
    
    // Copy constructor
    Receiver(const Receiver& other) : state_(other.state_) {
        if (state_) {
            state_->add_receiver();
        }
    }
    
    // Move constructor
    Receiver(Receiver&& other) noexcept : state_(std::move(other.state_)) {}
    
    // Copy assignment
    Receiver& operator=(const Receiver& other) {
        if (this != &other) {
            if (state_) {
                state_->remove_receiver();
            }
            state_ = other.state_;
            if (state_) {
                state_->add_receiver();
            }
        }
        return *this;
    }
    
    // Move assignment
    Receiver& operator=(Receiver&& other) noexcept {
        if (this != &other) {
            if (state_) {
                state_->remove_receiver();
            }
            state_ = std::move(other.state_);
        }
        return *this;
    }
    
    ~Receiver() {
        if (state_) {
            state_->remove_receiver();
        }
    }
    
    // Receive a value (blocking)
    RecvResult<T> recv() {
        if (!state_) {
            return std::nullopt;
        }
        return state_->recv();
    }
    
    // Try to receive without blocking
    TryRecvResult<T> try_recv() {
        if (!state_) {
            return std::nullopt;
        }
        return state_->try_recv();
    }
    
    // Receive with timeout
    template<typename Rep, typename Period>
    RecvResult<T> recv_timeout(const std::chrono::duration<Rep, Period>& timeout) {
        if (!state_) {
            return std::nullopt;
        }
        return state_->recv_timeout(timeout);
    }
    
    // Check if channel is closed
    bool is_closed() const {
        return !state_ || state_->is_closed();
    }
    
private:
    std::shared_ptr<ChannelState<T>> state_;
};

// Channel factory for creating sender/receiver pairs
template<typename T>
class Channel {
    static_assert(is_send_v<T>, "Channel element type must be Send");
    
public:
    // Create an unbounded channel
    static std::pair<Sender<T>, Receiver<T>> unbounded() {
        auto state = std::make_shared<ChannelState<T>>(0);
        return {Sender<T>(state), Receiver<T>(state)};
    }
    
    // Create a bounded channel with specified capacity
    static std::pair<Sender<T>, Receiver<T>> bounded(size_t capacity) {
        auto state = std::make_shared<ChannelState<T>>(capacity);
        return {Sender<T>(state), Receiver<T>(state)};
    }
    
    // Create a synchronous channel (capacity = 0, blocking sends)
    static std::pair<Sender<T>, Receiver<T>> sync() {
        return bounded(1);
    }
};

// Convenience functions
template<typename T>
std::pair<Sender<T>, Receiver<T>> channel() {
    return Channel<T>::unbounded();
}

template<typename T>
std::pair<Sender<T>, Receiver<T>> channel(size_t capacity) {
    return Channel<T>::bounded(capacity);
}

template<typename T>
std::pair<Sender<T>, Receiver<T>> sync_channel() {
    return Channel<T>::sync();
}

// Send/Sync trait implementations for channel types
template<typename T>
struct Send<Sender<T>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<Sender<T>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Send<Receiver<T>> {
    static constexpr bool value = Send<T>::value;
};

template<typename T>
struct Sync<Receiver<T>> {
    static constexpr bool value = Send<T>::value;
};

} // namespace meld::stdx