#include "meld/std/channels.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>

namespace meld::stdx {

// Explicit instantiations for common types
template class Sender<int>;
template class Receiver<int>;
template class Channel<int>;

template class Sender<std::string>;
template class Receiver<std::string>;
template class Channel<std::string>;

template class Sender<double>;
template class Receiver<double>;
template class Channel<double>;

// Utility functions for channel debugging and monitoring
namespace channel_utils {

template<typename T>
void log_channel_stats(const std::shared_ptr<ChannelState<T>>& state, const std::string& context) {
    if (state) {
        std::cout << "[Channel Stats - " << context << "] "
                  << "Size: " << state->size() 
                  << ", Capacity: " << (state->capacity() == 0 ? "unbounded" : std::to_string(state->capacity()))
                  << ", Closed: " << (state->is_closed() ? "yes" : "no")
                  << ", Senders: " << state->sender_count()
                  << ", Receivers: " << state->receiver_count()
                  << std::endl;
    }
}

// Helper function to create a producer-consumer pattern
template<typename T, typename Producer, typename Consumer>
void run_producer_consumer(Producer producer, Consumer consumer, size_t num_producers = 1, size_t num_consumers = 1) {
    auto [sender, receiver] = channel<T>();
    
    std::vector<std::thread> threads;
    
    // Start producer threads
    for (size_t i = 0; i < num_producers; ++i) {
        auto sender_copy = sender;
        threads.emplace_back([producer, sender_copy, i]() mutable {
            producer(std::move(sender_copy), i);
        });
    }
    
    // Start consumer threads
    for (size_t i = 0; i < num_consumers; ++i) {
        auto receiver_copy = receiver;
        threads.emplace_back([consumer, receiver_copy, i]() mutable {
            consumer(std::move(receiver_copy), i);
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
}

// Demonstration functions
void demonstrate_unbounded_channel() {
    std::cout << "=== Unbounded Channel Demo ===" << std::endl;
    
    auto [sender, receiver] = channel<int>();
    
    std::thread producer([sender = std::move(sender)]() mutable {
        for (int i = 0; i < 10; ++i) {
            auto result = sender.send(i);
            if (result) {
                std::cout << "Send failed: " << result->what() << std::endl;
            } else {
                std::cout << "Sent: " << i << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        sender.close();
        std::cout << "Producer finished" << std::endl;
    });
    
    std::thread consumer([receiver = std::move(receiver)]() mutable {
        while (true) {
            auto result = receiver.recv();
            if (result) {
                std::cout << "Received: " << *result << std::endl;
            } else {
                std::cout << "Channel closed" << std::endl;
                break;
            }
        }
        std::cout << "Consumer finished" << std::endl;
    });
    
    producer.join();
    consumer.join();
}

void demonstrate_bounded_channel() {
    std::cout << "\n=== Bounded Channel Demo ===" << std::endl;
    
    auto [sender, receiver] = channel<std::string>(3); // Capacity of 3
    
    std::thread producer([sender = std::move(sender)]() mutable {
        std::vector<std::string> messages = {
            "Hello", "World", "From", "Bounded", "Channel", "Demo"
        };
        
        for (const auto& msg : messages) {
            auto result = sender.send_blocking(msg);
            if (result) {
                std::cout << "Send failed: " << result->what() << std::endl;
            } else {
                std::cout << "Sent: " << msg << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        sender.close();
    });
    
    std::thread consumer([receiver = std::move(receiver)]() mutable {
        while (true) {
            auto result = receiver.recv_timeout(std::chrono::seconds(1));
            if (result) {
                std::cout << "Received: " << *result << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Slower consumer
            } else {
                std::cout << "Receive timeout or channel closed" << std::endl;
                break;
            }
        }
    });
    
    producer.join();
    consumer.join();
}

void demonstrate_multiple_producers_consumers() {
    std::cout << "\n=== Multiple Producers/Consumers Demo ===" << std::endl;
    
    auto [sender, receiver] = channel<int>();
    
    std::vector<std::thread> threads;
    
    // Start 3 producer threads
    for (int producer_id = 0; producer_id < 3; ++producer_id) {
        auto sender_copy = sender;
        threads.emplace_back([sender_copy, producer_id]() mutable {
            for (int i = 0; i < 5; ++i) {
                int value = producer_id * 100 + i;
                auto result = sender_copy.send(value);
                if (!result) {
                    std::cout << "Producer " << producer_id << " sent: " << value << std::endl;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            std::cout << "Producer " << producer_id << " finished" << std::endl;
        });
    }
    
    // Start 2 consumer threads
    for (int consumer_id = 0; consumer_id < 2; ++consumer_id) {
        auto receiver_copy = receiver;
        threads.emplace_back([receiver_copy, consumer_id]() mutable {
            while (true) {
                auto result = receiver_copy.recv_timeout(std::chrono::seconds(2));
                if (result) {
                    std::cout << "Consumer " << consumer_id << " received: " << *result << std::endl;
                } else {
                    std::cout << "Consumer " << consumer_id << " timed out" << std::endl;
                    break;
                }
            }
            std::cout << "Consumer " << consumer_id << " finished" << std::endl;
        });
    }
    
    // Close the original sender after a delay
    std::thread closer([sender = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        sender.close();
        std::cout << "Channel closed by main thread" << std::endl;
    });
    
    for (auto& thread : threads) {
        thread.join();
    }
    closer.join();
}

void demonstrate_try_recv() {
    std::cout << "\n=== Try Receive Demo ===" << std::endl;
    
    auto [sender, receiver] = channel<double>();
    
    // Send some values
    sender.send(3.14);
    sender.send(2.71);
    sender.send(1.41);
    
    // Try to receive without blocking
    for (int i = 0; i < 5; ++i) {
        auto result = receiver.try_recv();
        if (result) {
            std::cout << "Try recv got: " << *result << std::endl;
        } else {
            std::cout << "Try recv got nothing (attempt " << i + 1 << ")" << std::endl;
        }
    }
    
    sender.close();
}

} // namespace channel_utils

// Explicit instantiation of utility functions
template void channel_utils::log_channel_stats<int>(const std::shared_ptr<ChannelState<int>>&, const std::string&);
template void channel_utils::log_channel_stats<std::string>(const std::shared_ptr<ChannelState<std::string>>&, const std::string&);

} // namespace meld::stdx