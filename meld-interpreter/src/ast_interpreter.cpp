#include <meld/interpreter/ast_interpreter.hpp>
#include <meld/interpreter/prelude_source.hpp>
#include <meld/effects/effect.hpp>
#include <meld/effects/effect_firewall.hpp>
#include <meld/parser/parser.hpp>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>
#include <format>
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <array>
#include <dlfcn.h>
#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <thread>

#if defined(__linux__)
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#elif defined(__APPLE__)
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#elif !defined(_WIN32)
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#endif

namespace meld::interpreter {

namespace {

constexpr int kAsyncReadable = 1;
constexpr int kAsyncWritable = 2;
constexpr int kAsyncClosed = 4;

uint64_t mix64(uint64_t value) {
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return value;
}

uint64_t hash_bytes(std::string_view text) {
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint64_t combine_hash(uint64_t seed, uint64_t value) {
    return mix64(seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)));
}

uint64_t value_hash(const kernel::Value& value) {
    if (value.is<kernel::Empty>()) {
        return mix64(0x01ULL);
    }
    if (value.is<kernel::Boolean>()) {
        return mix64(value.as<kernel::Boolean>()->value() ? 0x02ULL : 0x03ULL);
    }
    if (value.is<kernel::Integer>()) {
        return mix64(0x10ULL ^ static_cast<uint64_t>(value.as<kernel::Integer>()->value()));
    }
    if (value.is<kernel::Float>()) {
        uint64_t bits = 0;
        double number = value.as<kernel::Float>()->value();
        if (number != 0.0) {
            std::memcpy(&bits, &number, sizeof(double));
        }
        return mix64(0x20ULL ^ bits);
    }
    if (value.is<kernel::String>()) {
        return mix64(0x30ULL ^ hash_bytes(value.as<kernel::String>()->value()));
    }
    if (value.is<kernel::Symbol>()) {
        return mix64(0x40ULL ^ hash_bytes(value.as<kernel::Symbol>()->name()));
    }
    if (value.is<kernel::Vec>()) {
        uint64_t hash = mix64(0x50ULL);
        for (const auto& item : value.as<kernel::Vec>()->elements()) {
            hash = combine_hash(hash, value_hash(item));
        }
        return hash;
    }
    if (value.is<kernel::IntVec>()) {
        uint64_t hash = mix64(0x55ULL);
        for (int64_t item : value.as<kernel::IntVec>()->elements()) {
            hash = combine_hash(hash, static_cast<uint64_t>(item));
        }
        return hash;
    }
    if (value.is<kernel::Function>() && value.as<kernel::Function>()->closure_env()) {
        std::vector<std::pair<std::string, kernel::Value>> fields;
        for (const auto& [key, field_value] : *value.as<kernel::Function>()->closure_env()) {
            fields.emplace_back(key, field_value);
        }
        std::sort(fields.begin(), fields.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        uint64_t hash = mix64(0x60ULL);
        for (const auto& [key, field_value] : fields) {
            hash = combine_hash(hash, hash_bytes(key));
            hash = combine_hash(hash, value_hash(field_value));
        }
        return hash;
    }

    return mix64(0x70ULL ^ reinterpret_cast<uintptr_t>(value.get_ptr()));
}

bool value_equal(const kernel::Value& left, const kernel::Value& right) {
    if (left.is<kernel::Empty>() || right.is<kernel::Empty>()) {
        return left.is<kernel::Empty>() && right.is<kernel::Empty>();
    }
    if (left.is<kernel::Boolean>() && right.is<kernel::Boolean>()) {
        return left.as<kernel::Boolean>()->value() == right.as<kernel::Boolean>()->value();
    }
    if (left.is<kernel::Integer>() && right.is<kernel::Integer>()) {
        return left.as<kernel::Integer>()->value() == right.as<kernel::Integer>()->value();
    }
    if (left.is<kernel::Float>() && right.is<kernel::Float>()) {
        return left.as<kernel::Float>()->value() == right.as<kernel::Float>()->value();
    }
    if (left.is<kernel::String>() && right.is<kernel::String>()) {
        return left.as<kernel::String>()->value() == right.as<kernel::String>()->value();
    }
    if (left.is<kernel::Symbol>() && right.is<kernel::Symbol>()) {
        return left.as<kernel::Symbol>()->name() == right.as<kernel::Symbol>()->name();
    }
    if (left.is<kernel::Vec>() && right.is<kernel::Vec>()) {
        const auto& left_items = left.as<kernel::Vec>()->elements();
        const auto& right_items = right.as<kernel::Vec>()->elements();
        if (left_items.size() != right_items.size()) {
            return false;
        }
        for (size_t i = 0; i < left_items.size(); ++i) {
            if (!value_equal(left_items[i], right_items[i])) {
                return false;
            }
        }
        return true;
    }
    if (left.is<kernel::IntVec>() && right.is<kernel::IntVec>()) {
        return left.as<kernel::IntVec>()->elements() == right.as<kernel::IntVec>()->elements();
    }
    if (left.is<kernel::Function>() && right.is<kernel::Function>() &&
        left.as<kernel::Function>()->closure_env() &&
        right.as<kernel::Function>()->closure_env()) {
        const auto& left_env = *left.as<kernel::Function>()->closure_env();
        const auto& right_env = *right.as<kernel::Function>()->closure_env();
        if (left_env.size() != right_env.size()) {
            return false;
        }
        for (const auto& [key, left_value] : left_env) {
            auto it = right_env.find(key);
            if (it == right_env.end() || !value_equal(left_value, it->second)) {
                return false;
            }
        }
        return true;
    }

    return left.get_ptr() == right.get_ptr();
}

int64_t positive_hash(const kernel::Value& value) {
    return static_cast<int64_t>(value_hash(value) & 0x7fffffffffffffffULL);
}

int64_t integer_arg(const std::vector<kernel::Value>& args, size_t index, int64_t fallback = 0) {
    if (index >= args.size() || !args[index].is<kernel::Integer>()) {
        return fallback;
    }
    return args[index].as<kernel::Integer>()->value();
}

std::string string_arg(const std::vector<kernel::Value>& args, size_t index, std::string fallback = {}) {
    if (index >= args.size() || !args[index].is<kernel::String>()) {
        return fallback;
    }
    return args[index].as<kernel::String>()->value();
}

kernel::Value nil_value() {
    return kernel::Value(kernel::Empty::instance());
}

kernel::Value int_value(int64_t value) {
    return kernel::Value(std::make_shared<kernel::Integer>(value));
}

kernel::Value string_value(std::string value) {
    return kernel::Value(std::make_shared<kernel::String>(std::move(value)));
}

kernel::Value struct_value(
        std::string type_name,
        std::vector<std::pair<std::string, kernel::Value>> fields) {
    kernel::Function::Environment env;
    env["__type__"] = kernel::Value(std::make_shared<kernel::Symbol>(type_name));
    for (auto& [key, value] : fields) {
        env[key] = std::move(value);
    }
    return kernel::Value(std::make_shared<kernel::Function>(
        std::vector<std::shared_ptr<kernel::Symbol>>{},
        kernel::Value{},
        std::nullopt,
        std::move(type_name),
        std::move(env)));
}

struct AsyncWakerRecord {
    int64_t scheduler_id = 0;
    int64_t task_id = 0;
    int64_t generation = 0;
    bool valid = false;
};

struct FdRegistration {
    int interest = 0;
    AsyncWakerRecord read;
    AsyncWakerRecord write;
};

struct ReactorState {
    int native_fd = -1;
    std::unordered_map<int, FdRegistration> registrations;
    std::unordered_map<int, int> ready_bits;
};

struct TimerState {
    int handle = -1;
    int native_fd = -1;
    int timeout_ms = 0;
    bool armed = false;
};

struct WakeRecord {
    int64_t task_id = 0;
    int64_t generation = 0;
};

struct AsyncKernelState {
    std::mutex mutex;
    int next_reactor_handle = 1;
    int next_timer_handle = 1'000'000'000;
    int next_atomic_handle = 1;
    std::unordered_map<int, ReactorState> reactors;
    std::unordered_map<int, TimerState> timers;
    std::unordered_map<int, std::shared_ptr<std::atomic<int64_t>>> atomics;
    std::unordered_map<int64_t, std::vector<WakeRecord>> wakes;
};

AsyncKernelState& async_kernel_state() {
    static AsyncKernelState state;
    return state;
}

void enqueue_wake_locked(AsyncKernelState& state, const AsyncWakerRecord& waker) {
    if (!waker.valid) {
        return;
    }
    state.wakes[waker.scheduler_id].push_back(WakeRecord{
        .task_id = waker.task_id,
        .generation = waker.generation,
    });
}

void set_fd_nonblocking(int fd) {
#if !defined(_WIN32)
    if (fd < 0) {
        return;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#else
    (void)fd;
#endif
}

void set_fd_cloexec(int fd) {
#if !defined(_WIN32)
    if (fd < 0) {
        return;
    }
    int flags = fcntl(fd, F_GETFD, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
#else
    (void)fd;
#endif
}

std::array<int, 2> create_nonblocking_pipe() {
    std::array<int, 2> fds{-1, -1};
#if defined(_WIN32)
    return fds;
#elif defined(__linux__)
    if (pipe2(fds.data(), O_NONBLOCK | O_CLOEXEC) == 0) {
        return fds;
    }
    if (errno != ENOSYS && errno != EINVAL) {
        return std::array<int, 2>{-1, -1};
    }
    if (pipe(fds.data()) != 0) {
        return std::array<int, 2>{-1, -1};
    }
    set_fd_nonblocking(fds[0]);
    set_fd_nonblocking(fds[1]);
    set_fd_cloexec(fds[0]);
    set_fd_cloexec(fds[1]);
    return fds;
#else
    if (pipe(fds.data()) != 0) {
        return std::array<int, 2>{-1, -1};
    }
    set_fd_nonblocking(fds[0]);
    set_fd_nonblocking(fds[1]);
    set_fd_cloexec(fds[0]);
    set_fd_cloexec(fds[1]);
    return fds;
#endif
}

int create_native_reactor() {
#if defined(__linux__)
    return epoll_create1(EPOLL_CLOEXEC);
#elif defined(__APPLE__)
    return kqueue();
#else
    return -1;
#endif
}

int create_native_timer() {
#if defined(__linux__)
    return timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
#else
    return -1;
#endif
}

int normalize_timer_timeout(int timeout_ms) {
    return timeout_ms <= 0 ? 1 : timeout_ms;
}

int platform_arm_timer(TimerState& timer, int timeout_ms) {
    timer.timeout_ms = normalize_timer_timeout(timeout_ms);
    timer.armed = true;
#if defined(__linux__)
    itimerspec spec{};
    spec.it_value.tv_sec = timer.timeout_ms / 1000;
    spec.it_value.tv_nsec = (timer.timeout_ms % 1000) * 1'000'000;
    if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) {
        spec.it_value.tv_nsec = 1;
    }
    return timerfd_settime(timer.native_fd, 0, &spec, nullptr);
#else
    return 0;
#endif
}

std::string port_string(int port) {
    return std::to_string(port);
}

int create_tcp_listener(const std::string& host, int port) {
#if defined(_WIN32)
    (void)host;
    (void)port;
    return -1;
#else
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (host.empty()) {
        hints.ai_flags = AI_PASSIVE;
    }

    addrinfo* results = nullptr;
    const char* node = host.empty() ? nullptr : host.c_str();
    int rc = getaddrinfo(node, port_string(port).c_str(), &hints, &results);
    if (rc != 0) {
        errno = EINVAL;
        return -1;
    }

    int listen_fd = -1;
    for (addrinfo* ai = results; ai != nullptr; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }

        int enabled = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
        set_fd_nonblocking(fd);
        set_fd_cloexec(fd);

        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, 128) == 0) {
            listen_fd = fd;
            break;
        }

        (void)close(fd);
    }

    freeaddrinfo(results);
    return listen_fd;
#endif
}

int accept_tcp_connection(int listener_fd) {
#if defined(_WIN32)
    (void)listener_fd;
    return -1;
#elif defined(__linux__)
    int fd = accept4(listener_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (fd >= 0) {
        return fd;
    }
    return -1;
#else
    int fd = accept(listener_fd, nullptr, nullptr);
    if (fd >= 0) {
        set_fd_nonblocking(fd);
        set_fd_cloexec(fd);
    }
    return fd;
#endif
}

int create_tcp_connection(const std::string& host, int port) {
#if defined(_WIN32)
    (void)host;
    (void)port;
    return -1;
#else
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* results = nullptr;
    int rc = getaddrinfo(host.c_str(), port_string(port).c_str(), &hints, &results);
    if (rc != 0) {
        errno = EINVAL;
        return -1;
    }

    int connect_fd = -1;
    for (addrinfo* ai = results; ai != nullptr; ai = ai->ai_next) {
        int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }

        set_fd_nonblocking(fd);
        set_fd_cloexec(fd);
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0 ||
            errno == EINPROGRESS ||
            errno == EWOULDBLOCK ||
            errno == EALREADY) {
            connect_fd = fd;
            break;
        }

        (void)close(fd);
    }

    freeaddrinfo(results);
    return connect_fd;
#endif
}

int tcp_local_port(int fd) {
#if defined(_WIN32)
    (void)fd;
    return -1;
#else
    sockaddr_storage addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        return -1;
    }
    if (addr.ss_family == AF_INET) {
        auto* in = reinterpret_cast<sockaddr_in*>(&addr);
        return ntohs(in->sin_port);
    }
    if (addr.ss_family == AF_INET6) {
        auto* in6 = reinterpret_cast<sockaddr_in6*>(&addr);
        return ntohs(in6->sin6_port);
    }
    return -1;
#endif
}

int tcp_connect_error(int fd) {
#if defined(_WIN32)
    (void)fd;
    return ECONNREFUSED;
#else
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) != 0) {
        return errno;
    }
    return err;
#endif
}

int platform_update_registration(AsyncKernelState& state, ReactorState& reactor, int fd, int interest) {
    auto timer_it = state.timers.find(fd);
    if (timer_it != state.timers.end()) {
        auto& timer = timer_it->second;
#if defined(__linux__)
        epoll_event event{};
        event.data.fd = timer.handle;
        event.events = EPOLLERR | EPOLLHUP;
        if (interest & kAsyncReadable) {
            event.events |= EPOLLIN;
        }
        if (epoll_ctl(reactor.native_fd, EPOLL_CTL_MOD, timer.native_fd, &event) == 0) {
            return 0;
        }
        if (errno == ENOENT) {
            return epoll_ctl(reactor.native_fd, EPOLL_CTL_ADD, timer.native_fd, &event);
        }
        return -1;
#elif defined(__APPLE__)
        if ((interest & kAsyncReadable) == 0) {
            return 0;
        }
        struct kevent change{};
        EV_SET(&change,
               static_cast<uintptr_t>(timer.handle),
               EVFILT_TIMER,
               EV_ADD | EV_ENABLE | EV_ONESHOT,
               NOTE_USECONDS,
               normalize_timer_timeout(timer.timeout_ms) * 1000,
               nullptr);
        return kevent(reactor.native_fd, &change, 1, nullptr, 0, nullptr);
#else
        (void)reactor;
        (void)interest;
        return 0;
#endif
    }

    set_fd_nonblocking(fd);
#if defined(__linux__)
    epoll_event event{};
    event.data.fd = fd;
    event.events = EPOLLERR | EPOLLHUP;
    if (interest & kAsyncReadable) {
        event.events |= EPOLLIN;
    }
    if (interest & kAsyncWritable) {
        event.events |= EPOLLOUT;
    }
    if (epoll_ctl(reactor.native_fd, EPOLL_CTL_MOD, fd, &event) == 0) {
        return 0;
    }
    if (errno == ENOENT) {
        return epoll_ctl(reactor.native_fd, EPOLL_CTL_ADD, fd, &event);
    }
    return -1;
#elif defined(__APPLE__)
    std::vector<struct kevent> changes;
    auto add_change = [&](int16_t filter) {
        struct kevent change{};
        EV_SET(&change,
               fd,
               filter,
               EV_ADD | EV_ENABLE | EV_CLEAR,
               0,
               0,
               nullptr);
        changes.push_back(change);
    };
    if (interest & kAsyncReadable) {
        add_change(EVFILT_READ);
    }
    if (interest & kAsyncWritable) {
        add_change(EVFILT_WRITE);
    }
    if (changes.empty()) {
        return 0;
    }
    int rc = kevent(reactor.native_fd, changes.data(), static_cast<int>(changes.size()), nullptr, 0, nullptr);
    if (rc < 0) {
        return -1;
    }
    return 0;
#else
    (void)reactor;
    (void)fd;
    (void)interest;
    return 0;
#endif
}

void platform_unregister(AsyncKernelState& state, ReactorState& reactor, int fd) {
    auto timer_it = state.timers.find(fd);
    if (timer_it != state.timers.end()) {
#if defined(__linux__)
        (void)epoll_ctl(reactor.native_fd, EPOLL_CTL_DEL, timer_it->second.native_fd, nullptr);
#elif defined(__APPLE__)
        struct kevent change{};
        EV_SET(&change, static_cast<uintptr_t>(timer_it->second.handle), EVFILT_TIMER, EV_DELETE, 0, 0, nullptr);
        (void)kevent(reactor.native_fd, &change, 1, nullptr, 0, nullptr);
#else
        (void)reactor;
#endif
        return;
    }

#if defined(__linux__)
    (void)epoll_ctl(reactor.native_fd, EPOLL_CTL_DEL, fd, nullptr);
#elif defined(__APPLE__)
    struct kevent changes[2]{};
    EV_SET(&changes[0], fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    EV_SET(&changes[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    (void)kevent(reactor.native_fd, changes, 2, nullptr, 0, nullptr);
#else
    (void)reactor;
    (void)fd;
#endif
}

std::vector<std::pair<int, int>> platform_poll(ReactorState& reactor, int timeout_ms) {
    std::vector<std::pair<int, int>> events;
#if defined(__linux__)
    std::array<epoll_event, 128> raw_events{};
    int count = epoll_wait(reactor.native_fd, raw_events.data(), static_cast<int>(raw_events.size()), timeout_ms);
    if (count <= 0) {
        return events;
    }
    events.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        int bits = 0;
        auto flags = raw_events[static_cast<size_t>(i)].events;
        if (flags & EPOLLIN) bits |= kAsyncReadable;
        if (flags & EPOLLOUT) bits |= kAsyncWritable;
        if (flags & (EPOLLERR | EPOLLHUP)) bits |= kAsyncClosed | kAsyncReadable | kAsyncWritable;
        events.emplace_back(raw_events[static_cast<size_t>(i)].data.fd, bits);
    }
#elif defined(__APPLE__)
    std::array<struct kevent, 128> raw_events{};
    struct timespec timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_nsec = (timeout_ms % 1000) * 1'000'000;
    int count = kevent(reactor.native_fd,
                       nullptr,
                       0,
                       raw_events.data(),
                       static_cast<int>(raw_events.size()),
                       &timeout);
    if (count <= 0) {
        return events;
    }
    events.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto& event = raw_events[static_cast<size_t>(i)];
        int bits = 0;
        if (event.filter == EVFILT_READ) bits |= kAsyncReadable;
        if (event.filter == EVFILT_WRITE) bits |= kAsyncWritable;
        if (event.filter == EVFILT_TIMER) bits |= kAsyncReadable;
        if (event.flags & EV_EOF) bits |= kAsyncClosed | kAsyncReadable | kAsyncWritable;
        events.emplace_back(static_cast<int>(event.ident), bits);
    }
#else
    std::vector<pollfd> fds;
    fds.reserve(reactor.registrations.size());
    for (const auto& [fd, registration] : reactor.registrations) {
        pollfd pfd{};
        pfd.fd = fd;
        if (registration.interest & kAsyncReadable) pfd.events |= POLLIN;
        if (registration.interest & kAsyncWritable) pfd.events |= POLLOUT;
        fds.push_back(pfd);
    }
    int count = poll(fds.data(), fds.size(), timeout_ms);
    if (count <= 0) {
        return events;
    }
    for (const auto& pfd : fds) {
        int bits = 0;
        if (pfd.revents & POLLIN) bits |= kAsyncReadable;
        if (pfd.revents & POLLOUT) bits |= kAsyncWritable;
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) bits |= kAsyncClosed | kAsyncReadable | kAsyncWritable;
        if (bits != 0) {
            events.emplace_back(pfd.fd, bits);
        }
    }
#endif
    return events;
}

kernel::Value ready_event_value(int fd, int interest) {
    return struct_value("ReadyEvent", {
        {"fd", int_value(fd)},
        {"interest", int_value(interest)},
        {"error", nil_value()},
    });
}

kernel::Value wake_event_value(const WakeRecord& wake) {
    return struct_value("WakeEvent", {
        {"task-id", int_value(wake.task_id)},
        {"generation", int_value(wake.generation)},
    });
}

kernel::Value atomic_compare_exchange_value(bool success, int64_t observed) {
    return struct_value("AtomicCompareExchange", {
        {"success", kernel::Value(kernel::Boolean::from(success))},
        {"value", int_value(observed)},
    });
}

void secure_zero_value(const kernel::Value& value) {
    if (value.is<kernel::String>()) {
        auto& text = value.as<kernel::String>()->value();
        volatile char* data = text.empty() ? nullptr : text.data();
        for (size_t i = 0; i < text.size(); ++i) {
            data[i] = 0;
        }
        text.clear();
        return;
    }
    if (value.is<kernel::Vec>()) {
        for (const auto& item : value.as<kernel::Vec>()->elements()) {
            secure_zero_value(item);
        }
        value.as<kernel::Vec>()->elements().clear();
    }
}

} // namespace

// ─── InterpreterError ───────────────────────────────────────────────

InterpreterError::InterpreterError(std::string message, SourceLocation location,
                                   std::vector<StackFrame> stack_trace)
    : std::runtime_error(message)
    , location_(std::move(location))
    , stack_trace_(std::move(stack_trace))
    , suggestions_(make_suggestions(message)) {}

const SourceLocation& InterpreterError::location() const { return location_; }
const std::vector<StackFrame>& InterpreterError::stack_trace() const { return stack_trace_; }

std::vector<std::string> InterpreterError::make_suggestions(const std::string& msg) {
    std::vector<std::string> s;
    if (msg.find("Division by zero") != std::string::npos)
        s.push_back("Add a zero check before dividing: when(divisor == 0).then({ rtn Err(\"division by zero\") })");
    else if (msg.find("Undefined function") != std::string::npos) {
        auto p = msg.find("'"); auto e = msg.find("'", p+1);
        std::string fn = (p != std::string::npos && e != std::string::npos) ? msg.substr(p+1, e-p-1) : "name";
        s.push_back("Define the function: fnc " + fn + "(...) -> ... { }");
        s.push_back("Check spelling or import the module containing '" + fn + "'");
    } else if (msg.find("Undefined variable") != std::string::npos) {
        auto p = msg.find("'"); auto e = msg.find("'", p+1);
        std::string var = (p != std::string::npos && e != std::string::npos) ? msg.substr(p+1, e-p-1) : "name";
        s.push_back("Declare the variable: val " + var + " = ...");
        s.push_back("Check spelling of '" + var + "'");
    } else if (msg.find("immutable") != std::string::npos || msg.find("Cannot reassign") != std::string::npos)
        s.push_back("Change 'val' to 'var' to allow mutation");
    else if (msg.find("Unsupported binary operator") != std::string::npos) {
        s.push_back("Check that operand types match");
        s.push_back("Define an operator overload: fnc add(a: Type, b: Type) -> Type { }");
    } else if (msg.find("not callable") != std::string::npos)
        s.push_back("The value is not a function — verify you're calling the right variable");
    else if (msg.find("unwrap") != std::string::npos || msg.find("panic") != std::string::npos)
        s.push_back("Check for None/Err before unwrapping: when(result.is-ok).then({ ... })");
    return s;
}

std::string InterpreterError::to_json() const {
    std::string json = "{\"error\": {";
    json += "\"message\": \"" + std::string(what()) + "\", ";
    json += "\"file\": \"" + location_.file + "\", ";
    json += "\"line\": " + std::to_string(location_.line) + ", ";
    json += "\"column\": " + std::to_string(location_.column) + ", ";
    json += "\"stack\": [";
    for (size_t i = 0; i < stack_trace_.size(); ++i) {
        json += "\"" + stack_trace_[i].function_name + "\"";
        if (i + 1 < stack_trace_.size()) json += ", ";
    }
    json += "], \"suggestions\": [";
    for (size_t i = 0; i < suggestions_.size(); ++i) {
        json += "\"" + suggestions_[i] + "\"";
        if (i + 1 < suggestions_.size()) json += ", ";
    }
    json += "]}}";
    return json;
}

// ─── Environment ────────────────────────────────────────────────────

Environment::Environment(std::shared_ptr<Environment> parent)
    : parent_(std::move(parent)) {}

void Environment::define(const std::string& name, kernel::Value value, bool is_mutable) {
    bindings_.insert_or_assign(name, Binding{std::move(value), is_mutable});
}

void Environment::assign(const std::string& name, kernel::Value value) {
    // Search current scope first
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        if (!it->second.is_mutable) {
            throw InterpreterError(
                std::format("Cannot reassign immutable binding '{}'", name),
                SourceLocation{});
        }
        it->second.value = std::move(value);
        return;
    }
    // Walk up the scope chain
    if (parent_) {
        parent_->assign(name, std::move(value));
        return;
    }
    throw InterpreterError(
        std::format("Undefined variable '{}'", name),
        SourceLocation{});
}

kernel::Value Environment::lookup(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second.value;
    }
    if (parent_) {
        return parent_->lookup(name);
    }
    throw InterpreterError(
        std::format("Undefined variable '{}'", name),
        SourceLocation{});
}

bool Environment::has(const std::string& name) const {
    if (bindings_.contains(name)) return true;
    if (parent_) return parent_->has(name);
    return false;
}

std::shared_ptr<Environment> Environment::parent() const { return parent_; }

std::shared_ptr<Environment> Environment::create_child() {
    return std::make_shared<Environment>(shared_from_this());
}

std::optional<kernel::Value> Environment::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) return it->second.value;
    if (parent_) return parent_->get(name);
    return std::nullopt;
}

std::unordered_map<std::string, kernel::Value> Environment::all_bindings() const {
    std::unordered_map<std::string, kernel::Value> result;
    for (const auto& [name, binding] : bindings_) {
        result[name] = binding.value;
    }
    return result;
}

// ─── AstInterpreter ─────────────────────────────────────────────────

AstInterpreter::AstInterpreter(std::shared_ptr<Environment> env)
    : env_(env ? std::move(env) : std::make_shared<Environment>()) {
    register_builtins();
    register_native_functions();
}

std::shared_ptr<Environment> AstInterpreter::environment() const { return env_; }

// ─── Built-in functions ──────────────────────────────────────────────
void AstInterpreter::register_builtins() {
    // ═══════════════════════════════════════════════════════════════════
    // IRREDUCIBLE KERNEL — only things that physically require C++.
    // ═══════════════════════════════════════════════════════════════════

    env_->define("nil", kernel::Value(kernel::Empty::instance()), false);

    auto def = [this](const char* name, kernel::Function::NativeImpl impl) {
        env_->define(name, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), name)), false);
    };

    // I/O
    def("println", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        for (auto& a : args) { if (a.is<kernel::String>()) std::cout << a.as<kernel::String>()->value(); else std::cout << a.to_string(); }
        std::cout << "\n"; return kernel::Value(kernel::Empty::instance());
    });
    def("print", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        for (auto& a : args) { if (a.is<kernel::String>()) std::cout << a.as<kernel::String>()->value(); else std::cout << a.to_string(); }
        return kernel::Value(kernel::Empty::instance());
    });

    // Array primitives
    def("len", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty()) return kernel::Value(std::make_shared<kernel::Integer>(0));
        if (args[0].is<kernel::Vec>()) return kernel::Value(std::make_shared<kernel::Integer>(args[0].as<kernel::Vec>()->size()));
        if (args[0].is<kernel::IntVec>()) return kernel::Value(std::make_shared<kernel::Integer>(args[0].as<kernel::IntVec>()->size()));
        if (args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::Integer>(args[0].as<kernel::String>()->value().size()));
        return kernel::Value(std::make_shared<kernel::Integer>(0));
    });
    def("arr-push", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return args.empty() ? kernel::Value{} : args[0];
        auto elems = args[0].as<kernel::Vec>()->elements();
        elems.push_back(args[1]);
        return kernel::Value(std::make_shared<kernel::Vec>(std::move(elems)));
    });
    def("arr-new", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int64_t size = integer_arg(args, 0);
        if (size < 0) size = 0;
        kernel::Value fill = args.size() > 1 ? args[1] : kernel::Value(kernel::Empty::instance());
        return kernel::Value(std::make_shared<kernel::Vec>(std::vector<kernel::Value>(
            static_cast<size_t>(size), fill)));
    });
    def("arr-get", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        int64_t index = integer_arg(args, 1, -1);
        const auto& elems = args[0].as<kernel::Vec>()->elements();
        if (index < 0 || static_cast<size_t>(index) >= elems.size()) return kernel::Value{};
        return elems[static_cast<size_t>(index)];
    });
    def("arr-set-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::Vec>()) return args.empty() ? kernel::Value{} : args[0];
        int64_t index = integer_arg(args, 1, -1);
        auto vec = args[0].as<kernel::Vec>();
        auto& elems = vec->elements();
        if (index >= 0 && static_cast<size_t>(index) < elems.size()) {
            elems[static_cast<size_t>(index)] = args[2];
        }
        return args[0];
    });
    def("arr-push-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return args.empty() ? kernel::Value{} : args[0];
        args[0].as<kernel::Vec>()->push_back(args[1]);
        return args[0];
    });
    def("arr-pop-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto& elems = args[0].as<kernel::Vec>()->elements();
        if (elems.empty()) return kernel::Value{};
        auto value = elems.back();
        elems.pop_back();
        return value;
    });
    def("arr-swap-remove-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        int64_t index = integer_arg(args, 1, -1);
        auto& elems = args[0].as<kernel::Vec>()->elements();
        if (index < 0 || static_cast<size_t>(index) >= elems.size()) return kernel::Value{};
        auto removed = elems[static_cast<size_t>(index)];
        elems[static_cast<size_t>(index)] = elems.back();
        elems.pop_back();
        return removed;
    });
    def("arr-clear-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::Vec>()) return args.empty() ? kernel::Value{} : args[0];
        args[0].as<kernel::Vec>()->elements().clear();
        return args[0];
    });
    def("iarr-new", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int64_t size = integer_arg(args, 0);
        if (size < 0) size = 0;
        int64_t fill = integer_arg(args, 1);
        return kernel::Value(std::make_shared<kernel::IntVec>(std::vector<int64_t>(
            static_cast<size_t>(size), fill)));
    });
    def("iarr-get", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::IntVec>()) return kernel::Value(std::make_shared<kernel::Integer>(0));
        int64_t index = integer_arg(args, 1, -1);
        const auto& elems = args[0].as<kernel::IntVec>()->elements();
        if (index < 0 || static_cast<size_t>(index) >= elems.size()) return kernel::Value(std::make_shared<kernel::Integer>(0));
        return kernel::Value(std::make_shared<kernel::Integer>(elems[static_cast<size_t>(index)]));
    });
    def("iarr-set-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::IntVec>()) return args.empty() ? kernel::Value{} : args[0];
        int64_t index = integer_arg(args, 1, -1);
        int64_t value = integer_arg(args, 2);
        auto vec = args[0].as<kernel::IntVec>();
        if (index >= 0 && static_cast<size_t>(index) < vec->size()) {
            vec->set(static_cast<size_t>(index), value);
        }
        return args[0];
    });
    def("iarr-fill-mut", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::IntVec>()) return args.empty() ? kernel::Value{} : args[0];
        args[0].as<kernel::IntVec>()->fill(integer_arg(args, 1));
        return args[0];
    });
    def("hash-code", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        return kernel::Value(std::make_shared<kernel::Integer>(args.empty() ? 0 : positive_hash(args[0])));
    });
    def("value-eq", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        return kernel::Value(kernel::Boolean::from(args.size() >= 2 && value_equal(args[0], args[1])));
    });
    def("bit-and", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t left = static_cast<uint64_t>(integer_arg(args, 0));
        uint64_t right = static_cast<uint64_t>(integer_arg(args, 1));
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(left & right)));
    });
    def("bit-or", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t left = static_cast<uint64_t>(integer_arg(args, 0));
        uint64_t right = static_cast<uint64_t>(integer_arg(args, 1));
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(left | right)));
    });
    def("bit-xor", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t left = static_cast<uint64_t>(integer_arg(args, 0));
        uint64_t right = static_cast<uint64_t>(integer_arg(args, 1));
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(left ^ right)));
    });
    def("bit-shl", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t value = static_cast<uint64_t>(integer_arg(args, 0));
        int64_t shift = integer_arg(args, 1);
        if (shift < 0 || shift >= 64) return kernel::Value(std::make_shared<kernel::Integer>(0));
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(value << shift)));
    });
    def("bit-shr", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t value = static_cast<uint64_t>(integer_arg(args, 0));
        int64_t shift = integer_arg(args, 1);
        if (shift < 0 || shift >= 64) return kernel::Value(std::make_shared<kernel::Integer>(0));
        return kernel::Value(std::make_shared<kernel::Integer>(static_cast<int64_t>(value >> shift)));
    });
    def("bit-popcount", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        uint64_t value = static_cast<uint64_t>(integer_arg(args, 0));
        return kernel::Value(std::make_shared<kernel::Integer>(std::popcount(value)));
    });
    def("loop-while", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Function>() || !args[1].is<kernel::Function>()) {
            return kernel::Value{};
        }
        auto condition = args[0].as<kernel::Function>();
        auto body = args[1].as<kernel::Function>();
        if (!condition->impl() || !body->impl()) {
            return kernel::Value{};
        }
        kernel::Value result;
        while ((*condition->impl())({}).is_truthy()) {
            result = (*body->impl())({});
        }
        return result;
    });

    // Collection primitives (iterative, no stack overflow)
    def("forEach", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto& elems = args[0].as<kernel::Vec>()->elements();
        auto& fn = args[1];
        if (!fn.is<kernel::Function>() || !fn.as<kernel::Function>()->impl()) return kernel::Value{};
        for (auto& e : elems) (*fn.as<kernel::Function>()->impl())({e});
        return kernel::Value{};
    });
    def("map", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto& elems = args[0].as<kernel::Vec>()->elements();
        auto& fn = args[1];
        if (!fn.is<kernel::Function>() || !fn.as<kernel::Function>()->impl()) return args[0];
        std::vector<kernel::Value> result;
        result.reserve(elems.size());
        for (auto& e : elems) result.push_back((*fn.as<kernel::Function>()->impl())({e}));
        return kernel::Value(std::make_shared<kernel::Vec>(std::move(result)));
    });
    def("filter", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto& elems = args[0].as<kernel::Vec>()->elements();
        auto& fn = args[1];
        if (!fn.is<kernel::Function>() || !fn.as<kernel::Function>()->impl()) return args[0];
        std::vector<kernel::Value> result;
        for (auto& e : elems) {
            auto v = (*fn.as<kernel::Function>()->impl())({e});
            if (v.is_truthy()) result.push_back(e);
        }
        return kernel::Value(std::make_shared<kernel::Vec>(std::move(result)));
    });
    def("reduce", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto& elems = args[0].as<kernel::Vec>()->elements();
        auto& init = args[1];
        auto& fn = args[2];
        if (!fn.is<kernel::Function>() || !fn.as<kernel::Function>()->impl()) return init;
        kernel::Value acc = init;
        for (auto& e : elems) acc = (*fn.as<kernel::Function>()->impl())({acc, e});
        return acc;
    });
    def("match", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[1].is<kernel::Vec>()) return kernel::Value{};
        auto& target = args[0];
        auto& cases = args[1].as<kernel::Vec>()->elements();
        for (auto& pair : cases) {
            if (!pair.is<kernel::Vec>() || pair.as<kernel::Vec>()->size() < 2) continue;
            auto& elems = pair.as<kernel::Vec>()->elements();
            if (elems[0].to_string() == target.to_string()) {
                if (elems[1].is<kernel::Function>() && elems[1].as<kernel::Function>()->impl())
                    return (*elems[1].as<kernel::Function>()->impl())({});
                return elems[1];
            }
        }
        return kernel::Value{};
    });

    // String primitives
    def("str-find", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::Integer>(-1));
        auto pos = args[0].as<kernel::String>()->value().find(args[1].as<kernel::String>()->value());
        return kernel::Value(std::make_shared<kernel::Integer>(pos == std::string::npos ? -1 : (int64_t)pos));
    });
    def("substr", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::String>() || !args[1].is<kernel::Integer>() || !args[2].is<kernel::Integer>()) return kernel::Value(std::make_shared<kernel::String>(""));
        auto& s = args[0].as<kernel::String>()->value();
        auto start = args[1].as<kernel::Integer>()->value();
        auto length = args[2].as<kernel::Integer>()->value();
        if (start < 0 || (size_t)start >= s.size()) return kernel::Value(std::make_shared<kernel::String>(""));
        return kernel::Value(std::make_shared<kernel::String>(s.substr(start, length)));
    });
    def("replace", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>() || !args[2].is<kernel::String>())
            return args.empty() ? kernel::Value(std::make_shared<kernel::String>("")) : args[0];
        std::string result = args[0].as<kernel::String>()->value();
        const auto& old_str = args[1].as<kernel::String>()->value();
        const auto& new_str = args[2].as<kernel::String>()->value();
        if (old_str.empty()) return kernel::Value(std::make_shared<kernel::String>(result));
        size_t pos = 0;
        while ((pos = result.find(old_str, pos)) != std::string::npos) {
            result.replace(pos, old_str.length(), new_str);
            pos += new_str.length();
        }
        return kernel::Value(std::make_shared<kernel::String>(result));
    });
    def("split", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>())
            return kernel::Value(std::make_shared<kernel::Vec>());
        const auto& s = args[0].as<kernel::String>()->value();
        const auto& delim = args[1].as<kernel::String>()->value();
        auto vec = std::make_shared<kernel::Vec>();
        if (delim.empty()) {
            vec->push_back(kernel::Value(std::make_shared<kernel::String>(s)));
            return kernel::Value(vec);
        }
        size_t start = 0, pos;
        while ((pos = s.find(delim, start)) != std::string::npos) {
            vec->push_back(kernel::Value(std::make_shared<kernel::String>(s.substr(start, pos - start))));
            start = pos + delim.length();
        }
        vec->push_back(kernel::Value(std::make_shared<kernel::String>(s.substr(start))));
        return kernel::Value(vec);
    });

    // Regex builtins (pure computation, no effect needed)
    def("regex-match", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>())
            return kernel::Value(kernel::Boolean::from(false));
        try {
            std::regex re(args[0].as<kernel::String>()->value());
            return kernel::Value(kernel::Boolean::from(std::regex_search(args[1].as<kernel::String>()->value(), re)));
        } catch (...) { return kernel::Value(kernel::Boolean::from(false)); }
    });
    def("regex-find-all", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        auto vec = std::make_shared<kernel::Vec>();
        if (args.size() < 2 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>()) return kernel::Value(vec);
        try {
            std::regex re(args[0].as<kernel::String>()->value());
            std::string input = args[1].as<kernel::String>()->value();
            for (auto it = std::sregex_iterator(input.begin(), input.end(), re); it != std::sregex_iterator(); ++it)
                vec->push_back(kernel::Value(std::make_shared<kernel::String>((*it)[0].str())));
        } catch (...) {}
        return kernel::Value(vec);
    });
    def("regex-replace", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>() || !args[2].is<kernel::String>())
            return args.size() > 1 ? args[1] : kernel::Value(std::make_shared<kernel::String>(""));
        try {
            std::regex re(args[0].as<kernel::String>()->value());
            return kernel::Value(std::make_shared<kernel::String>(std::regex_replace(args[1].as<kernel::String>()->value(), re, args[2].as<kernel::String>()->value())));
        } catch (...) { return args[1]; }
    });

    // Type reflection + conversion
    def("type-of", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty()) return kernel::Value(std::make_shared<kernel::String>("unit"));
        auto& v = args[0];
        if (v.is<kernel::Empty>()) return kernel::Value(std::make_shared<kernel::String>("unit"));
        if (v.is<kernel::Integer>()) return kernel::Value(std::make_shared<kernel::String>("int"));
        if (v.is<kernel::Float>()) return kernel::Value(std::make_shared<kernel::String>("float"));
        if (v.is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>("string"));
        if (v.is<kernel::Boolean>()) return kernel::Value(std::make_shared<kernel::String>("bool"));
        if (v.is<kernel::Vec>()) return kernel::Value(std::make_shared<kernel::String>("array"));
        if (v.is<kernel::IntVec>()) return kernel::Value(std::make_shared<kernel::String>("int-array"));
        if (v.is<kernel::Function>() && v.as<kernel::Function>()->closure_env()) {
            auto& env = *v.as<kernel::Function>()->closure_env();
            auto it = env.find("__type__");
            if (it != env.end()) {
                if (it->second.is<kernel::Symbol>()) {
                    return kernel::Value(std::make_shared<kernel::String>(it->second.as<kernel::Symbol>()->name()));
                }
                return kernel::Value(std::make_shared<kernel::String>(it->second.to_string()));
            }
        }
        if (v.is<kernel::Function>()) return kernel::Value(std::make_shared<kernel::String>("function"));
        return kernel::Value(std::make_shared<kernel::String>("unknown"));
    });
    def("int-to-str", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (!args.empty() && args[0].is<kernel::Integer>()) return kernel::Value(std::make_shared<kernel::String>(std::to_string(args[0].as<kernel::Integer>()->value())));
        return kernel::Value(std::make_shared<kernel::String>(""));
    });
    def("float-to-str", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (!args.empty() && args[0].is<kernel::Float>()) return kernel::Value(std::make_shared<kernel::String>(std::to_string(args[0].as<kernel::Float>()->value())));
        return kernel::Value(std::make_shared<kernel::String>(""));
    });
    def("to-string", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        return kernel::Value(std::make_shared<kernel::String>(args.empty() ? "" : args[0].to_string()));
    });

    // Error handling
    def("panic", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        throw InterpreterError(args.empty() ? "panic" : args[0].to_string(), SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // sandbox(allowed_effects_list, body) — restrict which effects body can use
    def("sandbox", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::Vec>())
            throw InterpreterError("sandbox requires (effects_list, body)", SourceLocation{source_file_, 0, 0}, call_stack_);
        std::set<std::string> allowed;
        for (auto& e : args[0].as<kernel::Vec>()->elements()) {
            if (e.is<kernel::String>()) allowed.insert(e.as<kernel::String>()->value());
            else allowed.insert(e.to_string());
        }
        allowed_effects_stack_.push_back(std::move(allowed));
        kernel::Value result;
        try {
            if (args[1].is<kernel::Function>() && args[1].as<kernel::Function>()->impl())
                result = (*args[1].as<kernel::Function>()->impl())({});
        } catch (...) {
            allowed_effects_stack_.pop_back();
            throw;
        }
        allowed_effects_stack_.pop_back();
        return result;
    });

    // inferred-effects(fn) -> string listing the effects a function uses
    def("inferred-effects", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::Function>()) return kernel::Value(std::make_shared<kernel::String>(""));
        const void* fn_id = args[0].as<kernel::Function>().get();
        auto it = metadata_table_.find(fn_id);
        if (it == metadata_table_.end()) return kernel::Value(std::make_shared<kernel::String>("(pure)"));
        auto eit = it->second.find("inferred_effects");
        if (eit == it->second.end()) return kernel::Value(std::make_shared<kernel::String>("(pure)"));
        return eit->second;
    });

    // generator(items_list) -> returns a next() function that yields one item at a time
    def("generator", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::Vec>()) return kernel::Value{};
        auto items = std::make_shared<std::vector<kernel::Value>>(args[0].as<kernel::Vec>()->elements());
        auto idx = std::make_shared<size_t>(0);
        kernel::Function::NativeImpl next = [items, idx](const std::vector<kernel::Value>&) -> kernel::Value {
            if (*idx < items->size()) return (*items)[(*idx)++];
            return kernel::Value{};
        };
        return kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(next), "generator"));
    });

    // uses("Console.println", "FileSystem.read") — declare permitted effects for next function
    // Uses string identifiers matching Effect.operation paths.
    // Will become @uses(Console.println) syntax when parser supports it.
    def("uses", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        pending_effects_.clear();
        for (auto& a : args) {
            if (a.is<kernel::String>()) pending_effects_.insert(a.as<kernel::String>()->value());
            else pending_effects_.insert(a.to_string());
        }
        return kernel::Value{};
    });

    // Effect handlers (I/O effects)
    register_effect_handler("Console", "println", env_->lookup("println"));
    register_effect_handler("Console", "print", env_->lookup("print"));

    // FFI effect handlers
    auto ffi_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("FFI", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("FFI.") + op)));
    };

    ffi_fn("load", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>())
            throw InterpreterError("FFI.load requires a library path", SourceLocation{source_file_, 0, 0}, call_stack_);
        void* handle = dlopen(args[0].as<kernel::String>()->value().c_str(), RTLD_LAZY);
        if (!handle)
            throw InterpreterError(std::string("FFI.load failed: ") + dlerror(), SourceLocation{source_file_, 0, 0}, call_stack_);
        int64_t id = static_cast<int64_t>(ffi_handles_.size());
        ffi_handles_.push_back(handle);
        return kernel::Value(std::make_shared<kernel::Integer>(id));
    });

    ffi_fn("call", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        return ffi_dispatch_int(args);
    });

    ffi_fn("call-float", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        return ffi_dispatch_float(args);
    });

    ffi_fn("call-string", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        return ffi_dispatch_string(args);
    });

    ffi_fn("call-void", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        return ffi_dispatch_void(args);
    });

    ffi_fn("close", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::Integer>()) return kernel::Value(kernel::Boolean::from(false));
        int64_t id = args[0].as<kernel::Integer>()->value();
        if (id >= 0 && id < (int64_t)ffi_handles_.size() && ffi_handles_[id]) {
            dlclose(ffi_handles_[id]);
            ffi_handles_[id] = nullptr;
        }
        return kernel::Value(kernel::Boolean::from(true));
    });

    // FileSystem effect
    auto fs_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("FileSystem", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("FileSystem.") + op)));
    };
    fs_fn("read", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>(""));
        std::ifstream f(args[0].as<kernel::String>()->value());
        if (!f) return kernel::Value(std::make_shared<kernel::String>(""));
        return kernel::Value(std::make_shared<kernel::String>(std::string(std::istreambuf_iterator<char>(f), {})));
    });
    fs_fn("write", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>()) return kernel::Value(kernel::Boolean::from(false));
        std::ofstream f(args[0].as<kernel::String>()->value());
        if (!f) return kernel::Value(kernel::Boolean::from(false));
        f << (args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string());
        return kernel::Value(kernel::Boolean::from(true));
    });
    fs_fn("exists", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(kernel::Boolean::from(false));
        return kernel::Value(kernel::Boolean::from(std::filesystem::exists(args[0].as<kernel::String>()->value())));
    });
    fs_fn("remove", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(kernel::Boolean::from(false));
        return kernel::Value(kernel::Boolean::from(std::filesystem::remove(args[0].as<kernel::String>()->value())));
    });
    fs_fn("append", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>()) return kernel::Value(kernel::Boolean::from(false));
        std::ofstream f(args[0].as<kernel::String>()->value(), std::ios::app);
        if (!f) return kernel::Value(kernel::Boolean::from(false));
        f << (args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string());
        return kernel::Value(kernel::Boolean::from(true));
    });

    // Env effect
    auto env_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Env", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Env.") + op)));
    };
    env_fn("get", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>(""));
        const char* v = std::getenv(args[0].as<kernel::String>()->value().c_str());
        return kernel::Value(std::make_shared<kernel::String>(v ? v : ""));
    });
    env_fn("cwd", [](const std::vector<kernel::Value>&) -> kernel::Value {
        return kernel::Value(std::make_shared<kernel::String>(std::filesystem::current_path().string()));
    });
    env_fn("home", [](const std::vector<kernel::Value>&) -> kernel::Value {
        const char* h = std::getenv("HOME");
        return kernel::Value(std::make_shared<kernel::String>(h ? h : ""));
    });
    env_fn("set", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>() || !args[1].is<kernel::String>())
            return kernel::Value(kernel::Boolean::from(false));
        setenv(args[0].as<kernel::String>()->value().c_str(), args[1].as<kernel::String>()->value().c_str(), 1);
        return kernel::Value(kernel::Boolean::from(true));
    });

    // Process effect
    auto proc_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Process", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Process.") + op)));
    };
    proc_fn("exec", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>(""));
        std::string cmd = args[0].as<kernel::String>()->value();
        for (size_t i = 1; i < args.size(); ++i) {
            if (args[i].is<kernel::Vec>()) { for (auto& a : args[i].as<kernel::Vec>()->elements()) cmd += " " + a.to_string(); }
            else cmd += " " + (args[i].is<kernel::String>() ? args[i].as<kernel::String>()->value() : args[i].to_string());
        }
        std::array<char, 4096> buf; std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return kernel::Value(std::make_shared<kernel::String>(""));
        while (fgets(buf.data(), buf.size(), pipe.get())) result += buf.data();
        return kernel::Value(std::make_shared<kernel::String>(result));
    });
    proc_fn("exit", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        std::exit(args.empty() ? 0 : (args[0].is<kernel::Integer>() ? (int)args[0].as<kernel::Integer>()->value() : 0));
        return kernel::Value{};
    });

    // Http effect
    auto http_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Http", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Http.") + op)));
    };
    auto curl_run = [](const std::string& cmd) -> std::string {
        std::array<char, 4096> buf; std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) return "";
        while (fgets(buf.data(), buf.size(), pipe.get())) result += buf.data();
        return result;
    };
    http_fn("get", [curl_run](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>(""));
        return kernel::Value(std::make_shared<kernel::String>(curl_run("curl -s -L '" + args[0].as<kernel::String>()->value() + "' 2>/dev/null")));
    });
    http_fn("post", [curl_run](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2) return kernel::Value(std::make_shared<kernel::String>(""));
        std::string url = args[0].is<kernel::String>() ? args[0].as<kernel::String>()->value() : "";
        std::string body = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : "";
        return kernel::Value(std::make_shared<kernel::String>(curl_run("curl -s -L -X POST -H 'Content-Type: application/json' -d '" + body + "' '" + url + "' 2>/dev/null")));
    });
    http_fn("put", [curl_run](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2) return kernel::Value(std::make_shared<kernel::String>(""));
        std::string url = args[0].is<kernel::String>() ? args[0].as<kernel::String>()->value() : "";
        std::string body = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : "";
        return kernel::Value(std::make_shared<kernel::String>(curl_run("curl -s -L -X PUT -H 'Content-Type: application/json' -d '" + body + "' '" + url + "' 2>/dev/null")));
    });
    http_fn("delete", [curl_run](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) return kernel::Value(std::make_shared<kernel::String>(""));
        return kernel::Value(std::make_shared<kernel::String>(curl_run("curl -s -L -X DELETE '" + args[0].as<kernel::String>()->value() + "' 2>/dev/null")));
    });

    // IO effect
    auto io_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("IO", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("IO.") + op)));
    };
    io_fn("read-line", [](const std::vector<kernel::Value>&) -> kernel::Value {
        std::string line; std::getline(std::cin, line);
        return kernel::Value(std::make_shared<kernel::String>(line));
    });

    // Log effect
    auto log_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Log", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Log.") + op)));
    };
    log_fn("write", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        std::string level = args.size() > 0 && args[0].is<kernel::String>() ? args[0].as<kernel::String>()->value() : "INFO";
        std::string msg = args.size() > 1 && args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : "";
        std::cerr << "[" << level << "] " << msg << "\n";
        return kernel::Value(kernel::Empty::instance());
    });

    // Time effect
    auto time_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Time", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Time.") + op)));
    };
    time_fn("now-ms", [](const std::vector<kernel::Value>&) -> kernel::Value {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return kernel::Value(std::make_shared<kernel::Integer>(ms));
    });

    // Random effect
    auto rand_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("Random", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("Random.") + op)));
    };
    rand_fn("int", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int64_t lo = args.size() > 0 && args[0].is<kernel::Integer>() ? args[0].as<kernel::Integer>()->value() : 0;
        int64_t hi = args.size() > 1 && args[1].is<kernel::Integer>() ? args[1].as<kernel::Integer>()->value() : 100;
        return kernel::Value(std::make_shared<kernel::Integer>(lo + (std::rand() % (hi - lo))));
    });
    rand_fn("float", [](const std::vector<kernel::Value>&) -> kernel::Value {
        return kernel::Value(std::make_shared<kernel::Float>(static_cast<double>(std::rand()) / RAND_MAX));
    });
    rand_fn("bool", [](const std::vector<kernel::Value>&) -> kernel::Value {
        return kernel::Value(kernel::Boolean::from(std::rand() % 2 == 0));
    });

    // ─── kernel.* primitives ────────────────────────────────────────
    auto kernel_fn = [&](const char* op, kernel::Function::NativeImpl impl) {
        register_effect_handler("kernel", op, kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{}, std::move(impl), std::string("kernel.") + op)));
    };

    // kernel.reactor-create(driver) -> handle
    kernel_fn("reactor-create", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        std::string driver = string_arg(args, 0, "auto");
        (void)driver;

        int native_fd = create_native_reactor();
        if (native_fd < 0) {
            throw InterpreterError("kernel.reactor-create failed", SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        int handle = state.next_reactor_handle++;
        state.reactors.emplace(handle, ReactorState{.native_fd = native_fd});
        return int_value(handle);
    });

    auto register_reactor_interest =
        [this](const std::vector<kernel::Value>& args, bool replace) -> kernel::Value {
            if (args.size() < 6) {
                throw InterpreterError("kernel.reactor-register requires (handle, fd, interest, scheduler-id, task-id, generation)",
                                       SourceLocation{source_file_, 0, 0}, call_stack_);
            }

            int handle = static_cast<int>(integer_arg(args, 0, -1));
            int fd = static_cast<int>(integer_arg(args, 1, -1));
            int interest = static_cast<int>(integer_arg(args, 2, 0));
            AsyncWakerRecord waker{
                .scheduler_id = integer_arg(args, 3),
                .task_id = integer_arg(args, 4),
                .generation = integer_arg(args, 5),
                .valid = true,
            };

            auto& state = async_kernel_state();
            std::lock_guard<std::mutex> lock(state.mutex);
            auto reactor_it = state.reactors.find(handle);
            if (reactor_it == state.reactors.end()) {
                throw InterpreterError("kernel.reactor-register: invalid reactor handle",
                                       SourceLocation{source_file_, 0, 0}, call_stack_);
            }

            auto& registration = reactor_it->second.registrations[fd];
            if (replace) {
                registration = FdRegistration{};
            }
            registration.interest |= interest;
            if (interest & kAsyncReadable) {
                registration.read = waker;
            }
            if (interest & kAsyncWritable) {
                registration.write = waker;
            }

            if (platform_update_registration(state, reactor_it->second, fd, registration.interest) != 0) {
                throw InterpreterError("kernel.reactor-register: OS registration failed",
                                       SourceLocation{source_file_, 0, 0}, call_stack_);
            }
            return nil_value();
        };

    kernel_fn("reactor-register", [register_reactor_interest](const std::vector<kernel::Value>& args) -> kernel::Value {
        return register_reactor_interest(args, false);
    });

    kernel_fn("reactor-reregister", [register_reactor_interest](const std::vector<kernel::Value>& args) -> kernel::Value {
        return register_reactor_interest(args, true);
    });

    // kernel.reactor-unregister(handle, fd)
    kernel_fn("reactor-unregister", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int fd = static_cast<int>(integer_arg(args, 1, -1));

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto reactor_it = state.reactors.find(handle);
        if (reactor_it == state.reactors.end()) {
            return nil_value();
        }
        platform_unregister(state, reactor_it->second, fd);
        reactor_it->second.registrations.erase(fd);
        reactor_it->second.ready_bits.erase(fd);
        return nil_value();
    });

    // kernel.reactor-poll(handle, timeout-ms)
    kernel_fn("reactor-poll", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int timeout_ms = static_cast<int>(integer_arg(args, 1, 0));

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto reactor_it = state.reactors.find(handle);
        if (reactor_it == state.reactors.end()) {
            throw InterpreterError("kernel.reactor-poll: invalid reactor handle",
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        auto raw_events = platform_poll(reactor_it->second, timeout_ms);
        auto result = std::make_shared<kernel::Vec>();
        for (const auto& [fd, bits] : raw_events) {
            if (bits == 0) {
                continue;
            }

            reactor_it->second.ready_bits[fd] |= bits;
            auto reg_it = reactor_it->second.registrations.find(fd);
            if (reg_it != reactor_it->second.registrations.end()) {
                if (bits & (kAsyncReadable | kAsyncClosed)) {
                    enqueue_wake_locked(state, reg_it->second.read);
                }
                if (bits & (kAsyncWritable | kAsyncClosed)) {
                    enqueue_wake_locked(state, reg_it->second.write);
                }
            }
            result->push_back(ready_event_value(fd, bits));
        }
        return kernel::Value(result);
    });

    // kernel.reactor-ready(handle, fd, interest)
    kernel_fn("reactor-ready", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int fd = static_cast<int>(integer_arg(args, 1, -1));
        int interest = static_cast<int>(integer_arg(args, 2, 0));

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto reactor_it = state.reactors.find(handle);
        if (reactor_it == state.reactors.end()) {
            throw InterpreterError("kernel.reactor-ready: invalid reactor handle",
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        int bits = reactor_it->second.ready_bits[fd];
        int matched = bits & (interest | kAsyncClosed);
        if (matched == 0) {
            return nil_value();
        }
        reactor_it->second.ready_bits[fd] = bits & ~matched;
        return ready_event_value(fd, matched);
    });

    // kernel.fd-read(fd, max-bytes)
    kernel_fn("fd-read", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
        int64_t max_bytes = integer_arg(args, 1, 0);
        if (fd < 0 || max_bytes <= 0) {
            return string_value("");
        }

#if defined(_WIN32)
        (void)fd;
        (void)max_bytes;
        return nil_value();
#else
        std::string buffer(static_cast<size_t>(max_bytes), '\0');
        ssize_t count = ::read(fd, buffer.data(), buffer.size());
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return nil_value();
            }
            throw InterpreterError(std::format("kernel.fd-read failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        buffer.resize(static_cast<size_t>(count));
        return string_value(std::move(buffer));
#endif
    });

    // kernel.fd-write(fd, data, offset)
    kernel_fn("fd-write", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
        std::string data = string_arg(args, 1);
        int64_t offset = integer_arg(args, 2, 0);
        if (fd < 0 || offset < 0 || static_cast<size_t>(offset) >= data.size()) {
            return int_value(0);
        }

#if defined(_WIN32)
        (void)fd;
        (void)data;
        (void)offset;
        return nil_value();
#else
        ssize_t count = ::write(fd, data.data() + offset, data.size() - static_cast<size_t>(offset));
        if (count < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                return nil_value();
            }
            throw InterpreterError(std::format("kernel.fd-write failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        return int_value(count);
#endif
    });

    kernel_fn("fd-close", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
#if !defined(_WIN32)
        if (fd >= 0) {
            (void)::close(fd);
        }
#else
        (void)fd;
#endif
        return nil_value();
    });

    // kernel.fd-pipe() -> [read-fd, write-fd]
    kernel_fn("fd-pipe", [this](const std::vector<kernel::Value>&) -> kernel::Value {
        auto fds = create_nonblocking_pipe();
        if (fds[0] < 0 || fds[1] < 0) {
            throw InterpreterError(std::format("kernel.fd-pipe failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        auto result = std::make_shared<kernel::Vec>();
        result->push_back(int_value(fds[0]));
        result->push_back(int_value(fds[1]));
        return kernel::Value(result);
    });

    // kernel.tcp-listen(host, port) -> nonblocking listener fd
    kernel_fn("tcp-listen", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        std::string host = string_arg(args, 0, "127.0.0.1");
        int port = static_cast<int>(integer_arg(args, 1, 0));
        int fd = create_tcp_listener(host, port);
        if (fd < 0) {
            throw InterpreterError(std::format("kernel.tcp-listen failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        return int_value(fd);
    });

    // kernel.tcp-local-port(listener-fd) -> bound TCP port
    kernel_fn("tcp-local-port", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
        int port = tcp_local_port(fd);
        if (port < 0) {
            throw InterpreterError(std::format("kernel.tcp-local-port failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        return int_value(port);
    });

    // kernel.tcp-accept(listener-fd) -> connected fd, or nil when not ready
    kernel_fn("tcp-accept", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
        errno = 0;
        int accepted = accept_tcp_connection(fd);
        int saved_errno = errno;
        if (accepted >= 0) {
            return int_value(accepted);
        }
        if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK || saved_errno == EINTR) {
            return nil_value();
        }
        throw InterpreterError(std::format("kernel.tcp-accept failed: {}", std::strerror(saved_errno)),
                               SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // kernel.tcp-connect(host, port) -> nonblocking socket fd
    kernel_fn("tcp-connect", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        std::string host = string_arg(args, 0, "127.0.0.1");
        int port = static_cast<int>(integer_arg(args, 1, 0));
        int fd = create_tcp_connection(host, port);
        if (fd < 0) {
            throw InterpreterError(std::format("kernel.tcp-connect failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        return int_value(fd);
    });

    // kernel.tcp-connect-ready(fd) -> true when nonblocking connect succeeded.
    kernel_fn("tcp-connect-ready", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int fd = static_cast<int>(integer_arg(args, 0, -1));
        int err = tcp_connect_error(fd);
        if (err == 0) {
            return kernel::Value(kernel::Boolean::from(true));
        }
        if (err == EINPROGRESS || err == EALREADY || err == EWOULDBLOCK) {
            return kernel::Value(kernel::Boolean::from(false));
        }
        throw InterpreterError(std::format("kernel.tcp-connect-ready failed: {}", std::strerror(err)),
                               SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // kernel.timer-create() -> readable timer handle
    kernel_fn("timer-create", [](const std::vector<kernel::Value>&) -> kernel::Value {
        auto& state = async_kernel_state();
#if defined(__linux__)
        int native_fd = create_native_timer();
        if (native_fd < 0) {
            throw InterpreterError(std::format("kernel.timer-create failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        int handle = native_fd;
#else
        int native_fd = -1;
        int handle = 0;
#endif

        std::lock_guard<std::mutex> lock(state.mutex);
#if !defined(__linux__)
        handle = state.next_timer_handle++;
#endif
        state.timers.emplace(handle, TimerState{
            .handle = handle,
            .native_fd = native_fd,
        });
        return int_value(handle);
    });

    // kernel.timer-arm(handle, timeout-ms)
    kernel_fn("timer-arm", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int timeout_ms = static_cast<int>(integer_arg(args, 1, 0));

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto timer_it = state.timers.find(handle);
        if (timer_it == state.timers.end()) {
            throw InterpreterError("kernel.timer-arm: invalid timer handle",
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        if (platform_arm_timer(timer_it->second, timeout_ms) != 0) {
            throw InterpreterError(std::format("kernel.timer-arm failed: {}", std::strerror(errno)),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }

        for (auto& [_, reactor] : state.reactors) {
            auto reg_it = reactor.registrations.find(handle);
            if (reg_it != reactor.registrations.end()) {
                if (platform_update_registration(state, reactor, handle, reg_it->second.interest) != 0) {
                    throw InterpreterError("kernel.timer-arm: reactor update failed",
                                           SourceLocation{source_file_, 0, 0}, call_stack_);
                }
            }
        }
        return nil_value();
    });

    // kernel.timer-cancel(handle)
    kernel_fn("timer-cancel", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto timer_it = state.timers.find(handle);
        if (timer_it == state.timers.end()) {
            return nil_value();
        }

        for (auto& [_, reactor] : state.reactors) {
            platform_unregister(state, reactor, handle);
            reactor.registrations.erase(handle);
            reactor.ready_bits.erase(handle);
        }

#if !defined(_WIN32)
        if (timer_it->second.native_fd >= 0) {
            (void)::close(timer_it->second.native_fd);
        }
#endif
        state.timers.erase(timer_it);
        return nil_value();
    });

    auto atomic_cell = [this](int handle, const char* op) -> std::shared_ptr<std::atomic<int64_t>> {
        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        auto it = state.atomics.find(handle);
        if (it == state.atomics.end()) {
            throw InterpreterError(std::format("kernel.{}: invalid atomic handle", op),
                                   SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        return it->second;
    };

    // kernel.atomic-create(initial) -> handle
    kernel_fn("atomic-create", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int64_t initial = integer_arg(args, 0, 0);
        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        int handle = state.next_atomic_handle++;
        state.atomics.emplace(handle, std::make_shared<std::atomic<int64_t>>(initial));
        return int_value(handle);
    });

    kernel_fn("atomic-load", [atomic_cell](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        auto cell = atomic_cell(handle, "atomic-load");
        return int_value(cell->load(std::memory_order_seq_cst));
    });

    kernel_fn("atomic-store", [atomic_cell](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int64_t value = integer_arg(args, 1, 0);
        auto cell = atomic_cell(handle, "atomic-store");
        cell->store(value, std::memory_order_seq_cst);
        return nil_value();
    });

    kernel_fn("atomic-exchange", [atomic_cell](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int64_t value = integer_arg(args, 1, 0);
        auto cell = atomic_cell(handle, "atomic-exchange");
        return int_value(cell->exchange(value, std::memory_order_seq_cst));
    });

    kernel_fn("atomic-fetch-add", [atomic_cell](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int64_t value = integer_arg(args, 1, 0);
        auto cell = atomic_cell(handle, "atomic-fetch-add");
        return int_value(cell->fetch_add(value, std::memory_order_seq_cst));
    });

    kernel_fn("atomic-compare-exchange", [atomic_cell](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        int64_t expected = integer_arg(args, 1, 0);
        int64_t desired = integer_arg(args, 2, 0);
        auto cell = atomic_cell(handle, "atomic-compare-exchange");
        int64_t observed = expected;
        bool success = cell->compare_exchange_strong(observed, desired, std::memory_order_seq_cst);
        return atomic_compare_exchange_value(success, observed);
    });

    kernel_fn("atomic-destroy", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int handle = static_cast<int>(integer_arg(args, 0, -1));
        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.atomics.erase(handle);
        return nil_value();
    });

    kernel_fn("thread-yield", [](const std::vector<kernel::Value>&) -> kernel::Value {
        std::this_thread::yield();
        return nil_value();
    });

    kernel_fn("cpu-count", [](const std::vector<kernel::Value>&) -> kernel::Value {
        unsigned count = std::thread::hardware_concurrency();
        return int_value(count == 0 ? 1 : static_cast<int64_t>(count));
    });

    // kernel.wake-task(scheduler-id, task-id, generation)
    kernel_fn("wake-task", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        AsyncWakerRecord waker{
            .scheduler_id = integer_arg(args, 0),
            .task_id = integer_arg(args, 1),
            .generation = integer_arg(args, 2),
            .valid = true,
        };

        auto& state = async_kernel_state();
        std::lock_guard<std::mutex> lock(state.mutex);
        enqueue_wake_locked(state, waker);
        return nil_value();
    });

    // kernel.take-wakes(scheduler-id)
    kernel_fn("take-wakes", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        int64_t scheduler_id = integer_arg(args, 0);
        std::vector<WakeRecord> wakes;
        {
            auto& state = async_kernel_state();
            std::lock_guard<std::mutex> lock(state.mutex);
            auto it = state.wakes.find(scheduler_id);
            if (it != state.wakes.end()) {
                wakes = std::move(it->second);
                state.wakes.erase(it);
            }
        }

        auto result = std::make_shared<kernel::Vec>();
        for (const auto& wake : wakes) {
            result->push_back(wake_event_value(wake));
        }
        return kernel::Value(result);
    });

    kernel_fn("secure-zero", [](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (!args.empty()) {
            secure_zero_value(args[0]);
        }
        return nil_value();
    });

    // kernel.apply(func, args_list) -> value
    kernel_fn("apply", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2) throw InterpreterError("kernel.apply requires (func, args_list)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto& fn = args[0];
        if (!args[1].is<kernel::Vec>()) throw InterpreterError("kernel.apply: second arg must be a list", SourceLocation{source_file_, 0, 0}, call_stack_);
        return apply_function(fn, args[1].as<kernel::Vec>()->elements(), SourceLocation{source_file_, 0, 0});
    });

    // kernel.call(name, ...args) -> value — dispatch to registered native function by name
    kernel_fn("call", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) throw InterpreterError("kernel.call requires (name, ...args)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto name = args[0].as<kernel::String>()->value();
        std::vector<kernel::Value> call_args(args.begin() + 1, args.end());

        // Native function dispatch table
        auto it = native_functions_.find(name);
        if (it != native_functions_.end()) {
            return it->second(call_args);
        }

        // Fallback: look up in Meld environment
        try {
            auto fn = env_->lookup(name);
            return apply_function(fn, call_args, SourceLocation{source_file_, 0, 0});
        } catch (...) {
            throw InterpreterError("kernel.call: unknown native function '" + name + "'", SourceLocation{source_file_, 0, 0}, call_stack_);
        }
    });

    // kernel.def(name, value) -> value
    kernel_fn("def", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>()) throw InterpreterError("kernel.def requires (name, value)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto name = args[0].as<kernel::String>()->value();
        env_->define(name, args[1], false);
        return args[1];
    });

    // kernel.set(name, value) -> value
    kernel_fn("set", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>()) throw InterpreterError("kernel.set requires (name, value)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto name = args[0].as<kernel::String>()->value();
        env_->assign(name, args[1]);
        return args[1];
    });

    // kernel.lookup(name) -> value
    kernel_fn("lookup", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty() || !args[0].is<kernel::String>()) throw InterpreterError("kernel.lookup requires (name)", SourceLocation{source_file_, 0, 0}, call_stack_);
        return env_->lookup(args[0].as<kernel::String>()->value());
    });

    // kernel.meta-set(obj, key, value) -> obj
    kernel_fn("meta-set", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 3 || !args[1].is<kernel::String>()) throw InterpreterError("kernel.meta-set requires (obj, key, value)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto& key = args[1].as<kernel::String>()->value();
        // Namespace enforcement: privileged prefixes are compiler/toolchain-only
        static const std::array<std::string_view, 5> reserved = {"type.", "effect.", "ownership.", "provenance.", "internal."};
        for (auto& prefix : reserved) {
            if (key.starts_with(prefix))
                throw InterpreterError(std::format("kernel.meta-set: namespace '{}' is reserved for the compiler/toolchain", prefix), SourceLocation{source_file_, 0, 0}, call_stack_);
        }
        const void* id = args[0].is<kernel::Function>() ? static_cast<const void*>(args[0].as<kernel::Function>().get()) : nullptr;
        if (!id) throw InterpreterError("kernel.meta-set: obj must be a struct/function", SourceLocation{source_file_, 0, 0}, call_stack_);
        metadata_table_[id][key] = args[2];
        return args[0];
    });

    // kernel.meta-get(obj, key) -> value?
    kernel_fn("meta-get", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[1].is<kernel::String>()) throw InterpreterError("kernel.meta-get requires (obj, key)", SourceLocation{source_file_, 0, 0}, call_stack_);
        const void* id = args[0].is<kernel::Function>() ? static_cast<const void*>(args[0].as<kernel::Function>().get()) : nullptr;
        if (!id) return kernel::Value{};
        auto it = metadata_table_.find(id);
        if (it == metadata_table_.end()) return kernel::Value{};
        auto kit = it->second.find(args[1].as<kernel::String>()->value());
        return kit != it->second.end() ? kit->second : kernel::Value{};
    });

    // kernel.meta-has(obj, key) -> bool
    kernel_fn("meta-has", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[1].is<kernel::String>()) throw InterpreterError("kernel.meta-has requires (obj, key)", SourceLocation{source_file_, 0, 0}, call_stack_);
        const void* id = args[0].is<kernel::Function>() ? static_cast<const void*>(args[0].as<kernel::Function>().get()) : nullptr;
        if (!id) return kernel::Value(kernel::Boolean::from(false));
        auto it = metadata_table_.find(id);
        if (it == metadata_table_.end()) return kernel::Value(kernel::Boolean::from(false));
        return kernel::Value(kernel::Boolean::from(it->second.count(args[1].as<kernel::String>()->value()) > 0));
    });

    // ─── Delimited continuations ────────────────────────────────────
    // kernel.mark(delimiter, body) — install delimiter, run body
    kernel_fn("mark", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>())
            throw InterpreterError("kernel.mark requires (delimiter, body)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto delimiter = args[0].as<kernel::String>()->value();
        auto& body = args[1];
        if (!body.is<kernel::Function>() || !body.as<kernel::Function>()->impl())
            throw InterpreterError("kernel.mark: body must be a function", SourceLocation{source_file_, 0, 0}, call_stack_);

        try {
            return (*body.as<kernel::Function>()->impl())({});
        } catch (SuspendSignal& sig) {
            if (sig.delimiter != delimiter) throw; // re-throw if not our delimiter
            // Create a one-shot continuation
            auto cont = std::make_shared<Continuation>();
            auto body_fn = body.as<kernel::Function>();
            cont->resume_fn = [this, body_fn, delimiter](kernel::Value resume_val) -> kernel::Value {
                // Push the resume value onto the resume stack
                resume_values_.push_back(resume_val);
                try {
                    auto result = (*body_fn->impl())({});
                    resume_values_.pop_back();
                    return result;
                } catch (...) {
                    resume_values_.pop_back();
                    throw;
                }
            };
            // Wrap continuation as a Meld value (struct with __type__ = "Continuation")
            kernel::Function::Environment cont_env;
            cont_env["__type__"] = kernel::Value(std::make_shared<kernel::Symbol>("Continuation"));
            auto cont_val = kernel::Value(std::make_shared<kernel::Function>(
                std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{},
                std::nullopt, "Continuation", std::move(cont_env)));
            // Store the C++ continuation in the metadata table
            const void* cont_id = cont_val.as<kernel::Function>().get();
            metadata_table_[cont_id]["__continuation__"] = kernel::Value{}; // placeholder
            continuation_table_[cont_id] = cont;

            // Call the user's callback with the continuation
            if (sig.callback.is<kernel::Function>() && sig.callback.as<kernel::Function>()->impl())
                return (*sig.callback.as<kernel::Function>()->impl())({cont_val});
            return kernel::Value{};
        }
    });

    // kernel.suspend(delimiter, callback) — capture continuation up to mark
    kernel_fn("suspend", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2 || !args[0].is<kernel::String>())
            throw InterpreterError("kernel.suspend requires (delimiter, callback)", SourceLocation{source_file_, 0, 0}, call_stack_);
        // If we're in a resume (replay), return the resume value instead of suspending
        if (!resume_values_.empty()) {
            auto val = resume_values_.back();
            return val;
        }
        // First run: throw to unwind to the mark
        SuspendSignal sig;
        sig.delimiter = args[0].as<kernel::String>()->value();
        sig.callback = args[1];
        throw sig;
    });

    // kernel.resume(k, value) — resume a captured continuation
    kernel_fn("resume", [this](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2)
            throw InterpreterError("kernel.resume requires (continuation, value)", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto& k = args[0];
        if (!k.is<kernel::Function>()) 
            throw InterpreterError("kernel.resume: first arg must be a Continuation", SourceLocation{source_file_, 0, 0}, call_stack_);
        const void* cont_id = k.as<kernel::Function>().get();
        auto it = continuation_table_.find(cont_id);
        if (it == continuation_table_.end() || !it->second)
            throw InterpreterError("kernel.resume: invalid continuation", SourceLocation{source_file_, 0, 0}, call_stack_);
        auto cont = it->second;
        if (cont->consumed)
            throw InterpreterError("kernel.resume: continuation already consumed (one-shot)", SourceLocation{source_file_, 0, 0}, call_stack_);
        cont->consumed = true;
        return cont->resume_fn(args[1]);
        return cont->resume_fn(args[1]);
    });

    // kernel.quote — stub
    kernel_fn("quote", [this](const std::vector<kernel::Value>&) -> kernel::Value {
        throw InterpreterError("kernel.quote: AST-as-value not yet implemented", SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // kernel.eval — stub
    kernel_fn("eval", [this](const std::vector<kernel::Value>&) -> kernel::Value {
        throw InterpreterError("kernel.eval: AST-as-value not yet implemented", SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // kernel.lambda — stub
    kernel_fn("lambda", [this](const std::vector<kernel::Value>&) -> kernel::Value {
        throw InterpreterError("kernel.lambda: dynamic closure creation not yet implemented", SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // kernel.load — stub
    kernel_fn("load", [this](const std::vector<kernel::Value>&) -> kernel::Value {
        throw InterpreterError("kernel.load: dynamic library loading not yet implemented", SourceLocation{source_file_, 0, 0}, call_stack_);
    });

    // ─── Conditional dispatch (then/when/else on When/WhenThen) ─────
    // These are native because they implement conditional logic (bootstrap).
    // They are dispatched through the normal method dispatch path.

    auto make_when = [](bool cond) -> kernel::Value {
        kernel::Function::Environment env;
        env["__type__"] = kernel::Value(std::make_shared<kernel::Symbol>("When"));
        env["__cond__"] = kernel::Value(kernel::Boolean::from(cond));
        return kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{},
            std::nullopt, "When", std::move(env)));
    };

    auto make_when_then = [](bool resolved, kernel::Value value) -> kernel::Value {
        kernel::Function::Environment env;
        env["__type__"] = kernel::Value(std::make_shared<kernel::Symbol>("WhenThen"));
        env["__resolved__"] = kernel::Value(kernel::Boolean::from(resolved));
        env["__value__"] = std::move(value);
        return kernel::Value(std::make_shared<kernel::Function>(
            std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{},
            std::nullopt, "WhenThen", std::move(env)));
    };

    auto get_type = [](const kernel::Value& v) -> std::string {
        if (!v.is<kernel::Function>() || !v.as<kernel::Function>()->closure_env()) return "";
        auto& env = *v.as<kernel::Function>()->closure_env();
        auto it = env.find("__type__");
        if (it == env.end()) return "";
        return it->second.is<kernel::String>() ? it->second.as<kernel::String>()->value() : it->second.to_string();
    };

    auto get_bool = [](const kernel::Value& v, const std::string& key) -> bool {
        if (!v.is<kernel::Function>() || !v.as<kernel::Function>()->closure_env()) return false;
        auto& env = *v.as<kernel::Function>()->closure_env();
        auto it = env.find(key);
        return it != env.end() && it->second.is<kernel::Boolean>() && it->second.as<kernel::Boolean>()->value();
    };

    auto get_value = [](const kernel::Value& v, const std::string& key) -> kernel::Value {
        if (!v.is<kernel::Function>() || !v.as<kernel::Function>()->closure_env()) return kernel::Value{};
        auto& env = *v.as<kernel::Function>()->closure_env();
        auto it = env.find(key);
        return it != env.end() ? it->second : kernel::Value{};
    };

    auto call_thunk = [](const kernel::Value& body) -> kernel::Value {
        if (body.is<kernel::Function>() && body.as<kernel::Function>()->impl())
            return (*body.as<kernel::Function>()->impl())({});
        return kernel::Value{};
    };

    def("then", [get_type, get_bool, get_value, call_thunk, make_when_then](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2) return kernel::Value{};
        auto type = get_type(args[0]);
        if (type == "When" || type == ":When") {
            bool cond = get_bool(args[0], "__cond__");
            kernel::Value val;
            if (cond) val = call_thunk(args[1]);
            return make_when_then(cond, std::move(val));
        }
        if (type == "WhenThen" || type == ":WhenThen") {
            bool resolved = get_bool(args[0], "__resolved__");
            if (resolved) return make_when_then(true, get_value(args[0], "__value__"));
            kernel::Value val = call_thunk(args[1]);
            return make_when_then(true, std::move(val));
        }
        return kernel::Value{};
    });

    def("else", [get_type, get_bool, get_value, call_thunk](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.size() < 2) return kernel::Value{};
        auto type = get_type(args[0]);
        if (type == "WhenThen" || type == ":WhenThen") {
            bool resolved = get_bool(args[0], "__resolved__");
            if (resolved) return get_value(args[0], "__value__");
            return call_thunk(args[1]);
        }
        return kernel::Value{};
    });

    // "when" handles both: when(bool) -> When, and WhenThen.when(cond) -> When/WhenThen
    def("when", [get_type, get_bool, get_value, make_when, make_when_then](const std::vector<kernel::Value>& args) -> kernel::Value {
        if (args.empty()) return kernel::Value{};
        // WhenThen.when(cond) — chaining
        auto type = get_type(args[0]);
        if (type == "WhenThen" || type == ":WhenThen") {
            bool resolved = get_bool(args[0], "__resolved__");
            if (resolved) return make_when_then(true, get_value(args[0], "__value__"));
            bool new_cond = args.size() > 1 && args[1].is<kernel::Boolean>() && args[1].as<kernel::Boolean>()->value();
            return make_when(new_cond);
        }
        // when(bool) -> When
        bool cond = !args.empty() && args[0].is<kernel::Boolean>() && args[0].as<kernel::Boolean>()->value();
        return make_when(cond);
    });

    // Load pure Meld prelude
    load_prelude();
}
// ─── Debug/Pretty string formatters ─────────────────────────────────

std::string AstInterpreter::value_to_debug_string(const kernel::Value& val) {
    if (val.is<kernel::Integer>()) {
        return std::format("int({})", val.as<kernel::Integer>()->value());
    }
    if (val.is<kernel::Float>()) {
        return std::format("float({})", val.as<kernel::Float>()->value());
    }
    if (val.is<kernel::Boolean>()) {
        return val.as<kernel::Boolean>()->value() ? "bool(true)" : "bool(false)";
    }
    if (val.is<kernel::String>()) {
        return std::format("string(\"{}\")", val.as<kernel::String>()->value());
    }
    if (val.is<kernel::Symbol>()) {
        return std::format("symbol(:{})", val.as<kernel::Symbol>()->name());
    }
    if (val.is<kernel::Empty>()) {
        return "nil";
    }
    if (val.is<kernel::Vec>()) {
        auto vec = val.as<kernel::Vec>();
        std::string result = "vec([";
        for (size_t i = 0; i < vec->size(); ++i) {
            if (i > 0) result += ", ";
            result += value_to_debug_string(vec->at(i));
        }
        result += "])";
        return result;
    }
    if (val.is<kernel::Cons>()) {
        auto cons = val.as<kernel::Cons>();
        return std::format("cons({}, {})",
            value_to_debug_string(cons->car()),
            value_to_debug_string(cons->cdr()));
    }
    if (val.is<kernel::Function>()) {
        auto fn = val.as<kernel::Function>();
        if (fn->name()) {
            return std::format("function({})", *fn->name());
        }
        return "function(<lambda>)";
    }
    if (val.is<kernel::Placeholder>()) {
        return "placeholder(_)";
    }
    return val.to_string();
}

std::string AstInterpreter::value_to_pretty_string(const kernel::Value& val, int indent) {
    std::string pad(indent * 2, ' ');

    if (val.is<kernel::Vec>()) {
        auto vec = val.as<kernel::Vec>();
        if (vec->size() == 0) return pad + "vec([])";
        std::string result = pad + "vec([\n";
        for (size_t i = 0; i < vec->size(); ++i) {
            result += value_to_pretty_string(vec->at(i), indent + 1);
            if (i + 1 < vec->size()) result += ",";
            result += "\n";
        }
        result += pad + "])";
        return result;
    }
    if (val.is<kernel::Cons>()) {
        auto cons = val.as<kernel::Cons>();
        std::string result = pad + "cons(\n";
        result += value_to_pretty_string(cons->car(), indent + 1) + ",\n";
        result += value_to_pretty_string(cons->cdr(), indent + 1) + "\n";
        result += pad + ")";
        return result;
    }
    // Scalar types: just indent + debug representation
    return pad + value_to_debug_string(val);
}

void AstInterpreter::set_source_file(const std::string& file) { source_file_ = file; }

// ─── Effect Firewall integration (Req 11.1, 11.2) ──────────────────

void AstInterpreter::set_effect_firewall(effects::EffectFirewall* firewall) {
    effect_firewall_ = firewall;
}

void AstInterpreter::register_effect_handler(const std::string& effect,
                                              const std::string& op,
                                              kernel::Value fn) {
    effect_handlers_[{effect, op}] = std::move(fn);
}

// ─── Debug hook API ─────────────────────────────────────────────────

void AstInterpreter::set_debug_mode(bool enabled) { debug_mode_ = enabled; }
bool AstInterpreter::debug_mode() const { return debug_mode_; }

void AstInterpreter::set_breakpoint(const std::string& file, size_t line) {
    BreakpointKey key{file, line};
    breakpoints_[key] = BreakpointInfo{file, line, std::nullopt, false, ""};
}

void AstInterpreter::set_conditional_breakpoint(const std::string& file, size_t line,
                                                  const std::string& condition) {
    BreakpointKey key{file, line};
    breakpoints_[key] = BreakpointInfo{file, line, condition, false, ""};
}

void AstInterpreter::set_logpoint(const std::string& file, size_t line,
                                   const std::string& log_expression) {
    BreakpointKey key{file, line};
    breakpoints_[key] = BreakpointInfo{file, line, std::nullopt, true, log_expression};
}

void AstInterpreter::remove_breakpoint(const std::string& file, size_t line) {
    breakpoints_.erase(BreakpointKey{file, line});
}

void AstInterpreter::clear_all_breakpoints() {
    breakpoints_.clear();
}

void AstInterpreter::set_step_mode(StepMode mode) {
    step_mode_ = mode;
    step_start_depth_ = call_stack_.size();
}

StepMode AstInterpreter::step_mode() const { return step_mode_; }

void AstInterpreter::set_pause_callback(PauseCallback callback) {
    pause_callback_ = std::move(callback);
}

void AstInterpreter::set_log_callback(LogCallback callback) {
    log_callback_ = std::move(callback);
}

void AstInterpreter::request_pause() {
    pause_requested_ = true;
}

const std::vector<StackFrame>& AstInterpreter::call_stack() const {
    return call_stack_;
}

// ─── Effect context introspection (Req 12D) ─────────────────────────

std::vector<std::string> AstInterpreter::get_active_effect_handlers() const {
    auto info = effects::EffectRuntime::instance().get_stack_info();
    return info.effect_names;
}

std::vector<std::string> AstInterpreter::get_current_uses_annotation() const {
    // TODO: When @uses annotations are tracked per-function in the AST,
    // return the declared effects of the currently executing function.
    // For now, return empty — the annotation system is specified in
    // .kiro/specs/effects-annotations/ and will be wired here once available.
    return {};
}

// ─── Debug pause check ──────────────────────────────────────────────

void AstInterpreter::check_debug_pause(const SourceLocation& loc) {
    if (!debug_mode_) return;

    bool should_pause = false;

    // 1. Check async pause request
    if (pause_requested_) {
        pause_requested_ = false;
        should_pause = true;
    }

    // 2. Check stepping mode
    if (!should_pause) {
        size_t current_depth = call_stack_.size();
        switch (step_mode_) {
            case StepMode::StepIn:
                should_pause = true;
                break;
            case StepMode::StepOver:
                should_pause = (current_depth <= step_start_depth_);
                break;
            case StepMode::StepOut:
                should_pause = (current_depth < step_start_depth_);
                break;
            case StepMode::Continue:
                break;
        }
    }

    // 3. Check breakpoints
    if (!should_pause && loc.line > 0) {
        auto it = breakpoints_.find(BreakpointKey{loc.file, loc.line});
        if (it != breakpoints_.end()) {
            const auto& bp = it->second;

            if (bp.is_logpoint) {
                // Logpoint: evaluate expression and log, don't pause
                if (log_callback_ && !bp.log_expression.empty()) {
                    std::string msg = evaluate_to_string(bp.log_expression, env_);
                    log_callback_(loc, msg);
                }
                return; // Never pause for logpoints
            }

            if (bp.condition.has_value()) {
                // Conditional breakpoint: only pause if condition is truthy
                should_pause = evaluate_condition(bp.condition.value(), env_);
            } else {
                should_pause = true;
            }
        }
    }

    // 4. Invoke pause callback
    if (should_pause && pause_callback_) {
        step_mode_ = StepMode::Continue; // Reset step mode on pause
        pause_callback_(loc, env_);
    }
}

bool AstInterpreter::evaluate_condition(const std::string& condition_expr,
                                         std::shared_ptr<Environment> env) {
    try {
        // Parse the condition as a single expression
        parser::Parser parser_instance;
        parser::ast::expression ast;
        if (!parser_instance.parse_expression(condition_expr, ast)) return false;

        // Evaluate in the given environment
        auto prev_env = env_;
        env_ = env;
        auto result = evaluate(ast);
        env_ = prev_env;

        // Check truthiness: Boolean true, non-zero Integer, non-empty String
        if (auto b = result.as<kernel::Boolean>()) return b->value();
        if (auto i = result.as<kernel::Integer>()) return i->value() != 0;
        if (auto s = result.as<kernel::String>()) return !s->value().empty();
        return true; // Non-null values are truthy
    } catch (...) {
        return false; // Condition evaluation errors → don't pause
    }
}

std::string AstInterpreter::evaluate_to_string(const std::string& expr,
                                                std::shared_ptr<Environment> env) {
    try {
        parser::Parser parser_instance;
        parser::ast::expression ast;
        if (!parser_instance.parse_expression(expr, ast)) return "<parse error>";

        auto prev_env = env_;
        env_ = env;
        auto result = evaluate(ast);
        env_ = prev_env;

        // Convert result to string representation
        if (auto i = result.as<kernel::Integer>()) return std::to_string(i->value());
        if (auto s = result.as<kernel::String>()) return s->value();
        if (auto b = result.as<kernel::Boolean>()) return b->value() ? "true" : "false";
        return "<value>";
    } catch (const std::exception& e) {
        return std::string("<error: ") + e.what() + ">";
    }
}

template<typename T>
SourceLocation AstInterpreter::source_location([[maybe_unused]] const T& node) const {
    // x3::position_tagged nodes carry iterator positions, but extracting
    // line/column requires the original source buffer which we don't store.
    // For now, return file name only; line/column can be enhanced later
    // when we integrate with the parser's position cache.
    return SourceLocation{source_file_, 0, 0};
}

// ─── Expression dispatch via boost::apply_visitor ───────────────────

struct EvalVisitor : public boost::static_visitor<kernel::Value> {
    AstInterpreter& interp;
    explicit EvalVisitor(AstInterpreter& i) : interp(i) {}

    kernel::Value operator()(const parser::ast::identifier& id) const {
        return interp.eval_identifier(id);
    }
    kernel::Value operator()(const parser::ast::integer_literal& lit) const {
        return interp.eval_integer_literal(lit);
    }
    kernel::Value operator()(const parser::ast::float_literal& lit) const {
        return interp.eval_float_literal(lit);
    }
    kernel::Value operator()(const parser::ast::string_literal& lit) const {
        return interp.eval_string_literal(lit);
    }
    kernel::Value operator()(const parser::ast::regex_literal&) const {
        return kernel::Value{}; // TODO: regex support
    }
    kernel::Value operator()(const parser::ast::boolean_literal& lit) const {
        return interp.eval_boolean_literal(lit);
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::list_expression>& ast) const {
        return interp.eval_list_expression(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::anonymous_array_literal>& ast) const {
        std::vector<kernel::Value> elements;
        for (const auto& elem : ast.get().elements)
            elements.push_back(interp.evaluate(elem.get()));
        return kernel::Value(std::make_shared<kernel::Vec>(std::move(elements)));
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::array_indexing>& ast) const {
        return interp.eval_array_indexing(ast.get());
    }
    kernel::Value operator()(const parser::ast::array_indexing& ast) const {
        return interp.eval_array_indexing(ast);
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::spread_expression>&) const {
        return kernel::Value{}; // TODO: spread
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::function_call>& ast) const {
        return interp.eval_function_call(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::val_declaration>& ast) const {
        return interp.eval_val_declaration(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::var_declaration>& ast) const {
        return interp.eval_var_declaration(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::binary_operation>& ast) const {
        return interp.eval_binary_operation(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::unary_operation>& ast) const {
        return interp.eval_unary_operation(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::function_definition>& ast) const {
        return interp.eval_function_definition(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::lambda_expression>& ast) const {
        return interp.eval_lambda_expression(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::block_expression>& ast) const {
        return interp.eval_block_expression(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::return_statement>& ast) const {
        return interp.eval_return_statement(ast.get());
    }

    // Task 7.1 / 7.2 (implicit-effect-calls): handle both perform_expression
    // and implicit_effect_call through the shared eval_effect_suspend helper.
    // Requirements 3.1, 3.2, 3.3.
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::perform_expression>& ast) const {
        return interp.eval_perform_expression(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>& ast) const {
        return interp.eval_implicit_effect_call(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::handle_expression>& ast) const {
        return interp.eval_handle_expression(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::assertion_expression>& ast) const {
        return interp.eval_assertion(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::test_block>& ast) const {
        return interp.eval_test_block(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::initialization_block>& ast) const {
        return interp.eval_initialization_block(ast.get());
    }
    kernel::Value operator()(const parser::ast::tuple_indexing& ast) const {
        return interp.eval_dot_access(ast);
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::tuple_indexing>& ast) const {
        return interp.eval_dot_access(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::struct_definition>& ast) const {
        // Register type name in environment
        interp.env_->define(ast.get().name.name, kernel::Value::from_symbol(ast.get().name.name), false);
        return kernel::Value{};
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::class_definition>& ast) const {
        interp.env_->define(ast.get().name.name, kernel::Value::from_symbol(ast.get().name.name), false);
        return kernel::Value{};
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::enum_definition>& ast) const {
        interp.env_->define(ast.get().name.name, kernel::Value::from_symbol(ast.get().name.name), false);
        return kernel::Value{};
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::import_declaration>& ast) const {
        return interp.eval_import(ast.get());
    }
    kernel::Value operator()(const boost::spirit::x3::forward_ast<parser::ast::named_return_assignment>& ast) const {
        auto value = interp.evaluate(ast.get().value.get());
        interp.env_->assign(ast.get().return_name.name, value);
        return value;
    }

    // Catch-all for unimplemented AST node types
    template<typename T>
    kernel::Value operator()(const T&) const {
        return kernel::Value{};
    }
};

kernel::Value AstInterpreter::evaluate(const parser::ast::expression& expr) {
    // Debug hook: check breakpoints and stepping before each expression
    if (debug_mode_) {
        // Extract source location from the expression variant if possible
        auto loc = SourceLocation{source_file_, 0, 0};
        // TODO: Extract line/column from position_tagged nodes when
        // parser position cache integration is available
        check_debug_pause(loc);
    }

    EvalVisitor visitor{*this};
    return boost::apply_visitor(visitor, expr);
}

void AstInterpreter::load_prelude() {
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (p.parse_file(std::string(PRELUDE_SOURCE), ast)) {
        for (const auto& expr : ast) evaluate(expr);
    }
}

kernel::Value AstInterpreter::evaluate_program(const std::vector<parser::ast::expression>& exprs) {
    kernel::Value result;
    for (const auto& expr : exprs) {
        result = evaluate(expr);
    }

    // Auto-invoke main() if defined at top level
    try {
        auto main_fn = env_->lookup("main");
        if (main_fn.is<kernel::Function>()) {
            result = apply_function(main_fn, {}, SourceLocation{"<entry>", 0, 0});
        } else {
        }
    } catch (const InterpreterError&) {
        throw;  // Let caller handle
    } catch (const std::exception&) {
        throw;  // Let caller handle
    }

    return result;
}

// ─── Literal evaluation ─────────────────────────────────────────────

kernel::Value AstInterpreter::eval_integer_literal(const parser::ast::integer_literal& lit) {
    return kernel::Value(std::make_shared<kernel::Integer>(lit.value));
}

kernel::Value AstInterpreter::eval_float_literal(const parser::ast::float_literal& lit) {
    return kernel::Value(std::make_shared<kernel::Float>(lit.value));
}

kernel::Value AstInterpreter::eval_string_literal(const parser::ast::string_literal& lit) {
    if (!lit.has_interpolation) {
        return kernel::Value(std::make_shared<kernel::String>(lit.value));
    }
    
    // Process string with interpolation: expand ${expr} and ${:modifier expr}
    // into concatenation of string segments and function calls.
    //
    // Desugaring rules (per design doc):
    //   ${expr}            → __interpolate-default__(expr)
    //   ${:modifier expr}  → __interpolate-<modifier>__(expr)
    //
    // The compiler only knows the naming convention — it never interprets
    // what the modifier symbol means.
    
    std::string result;
    const std::string& src = lit.value;
    size_t i = 0;
    
    while (i < src.size()) {
        if (i + 1 < src.size() && src[i] == '$' && src[i + 1] == '{') {
            i += 2; // skip ${
            
            // Check for modifier: ${:symbol ...}
            std::string modifier;
            if (i < src.size() && src[i] == ':') {
                i++; // skip ':'
                // Read modifier name (supports kebab-case)
                while (i < src.size() && src[i] != ' ' && src[i] != '\t' && src[i] != '}') {
                    modifier += src[i];
                    i++;
                }
                // Skip whitespace between modifier and expression
                while (i < src.size() && (src[i] == ' ' || src[i] == '\t')) {
                    i++;
                }
            }
            
            // Read the expression until closing }
            std::string expr_str;
            int brace_count = 1;
            while (i < src.size() && brace_count > 0) {
                if (src[i] == '{') brace_count++;
                else if (src[i] == '}') {
                    brace_count--;
                    if (brace_count == 0) break;
                }
                expr_str += src[i];
                i++;
            }
            if (i < src.size()) i++; // skip closing }
            
            // Evaluate the inner expression via X3
            parser::Parser expr_parser;
            std::vector<parser::ast::expression> expr_ast;
            if (expr_parser.parse_file(expr_str, expr_ast) && !expr_ast.empty()) {
                kernel::Value val = evaluate(expr_ast[0]);
                
                // Determine the interpolation function name
                std::string func_name = modifier.empty() 
                    ? "__interpolate-default__" 
                    : "__interpolate-" + modifier + "__";
                
                // Look up and call the interpolation function
                try {
                    auto func = env_->lookup(func_name);
                    auto loc = SourceLocation{source_file_, 0, 0};
                    auto str_result = apply_function(func, {val}, loc);
                    if (str_result.is<kernel::String>()) {
                        result += str_result.as<kernel::String>()->value();
                    } else {
                        result += str_result.to_string();
                    }
                } catch (...) {
                    if (modifier.empty()) {
                        // Fallback for default: use Value::to_string() directly
                        result += val.to_string();
                    } else {
                        throw InterpreterError(
                            std::format("Undefined interpolation modifier '{}' (no function '{}' in scope)",
                                       modifier, func_name),
                            SourceLocation{source_file_, 0, 0},
                            call_stack_);
                    }
                }
            } else {
                throw InterpreterError(
                    std::format("Failed to parse interpolation expression: {}", expr_str),
                    SourceLocation{source_file_, 0, 0},
                    call_stack_);
            }
        } else {
            result += src[i];
            i++;
        }
    }
    
    return kernel::Value(std::make_shared<kernel::String>(result));
}

kernel::Value AstInterpreter::eval_boolean_literal(const parser::ast::boolean_literal& lit) {
    return kernel::Value(kernel::Boolean::from(lit.value));
}

kernel::Value AstInterpreter::eval_identifier(const parser::ast::identifier& id) {
    if (id.name.empty()) return kernel::Value{};  // Skip empty identifiers from parser
    try {
        return env_->lookup(id.name);
    } catch (InterpreterError& e) {
        throw InterpreterError(
            std::format("Undefined variable '{}'", id.name),
            source_location(id),
            call_stack_);
    }
}

// ─── Declarations ───────────────────────────────────────────────────

kernel::Value AstInterpreter::eval_val_declaration(const parser::ast::val_declaration& decl) {
    auto value = evaluate(decl.value.get());
    env_->define(decl.name.name, value, /*is_mutable=*/false);
    return value;
}

kernel::Value AstInterpreter::eval_var_declaration(const parser::ast::var_declaration& decl) {
    auto value = evaluate(decl.value.get());
    // Value semantics: copy struct closures on assignment
    if (value.is<kernel::Function>() && value.as<kernel::Function>()->closure_env()) {
        auto& env = *value.as<kernel::Function>()->closure_env();
        if (env.count("__type__")) {
            // Deep copy the closure
            kernel::Function::Environment copy = env;
            value = kernel::Value(std::make_shared<kernel::Function>(
                std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{},
                std::nullopt, value.as<kernel::Function>()->name().value_or(""),
                std::move(copy)));
        }
    }
    env_->define(decl.name.name, value, /*is_mutable=*/true);
    return value;
}

// ─── Function definition ────────────────────────────────────────────

kernel::Value AstInterpreter::eval_function_definition(const parser::ast::function_definition& def) {
    // Capture the defining environment and the AST body for closure semantics.
    // We use kernel::Function's NativeImpl to wrap the interpreter call.
    auto captured_env = env_;
    auto body_copy = def.body.get();
    auto params_copy = def.parameters;
    auto fn_name = def.name.name;

    // Get effects from @uses annotation (AST) or uses() builtin (runtime)
    std::set<std::string> fn_effects;
    if (def.has_effects) {
        for (auto& e : def.effects_clause) fn_effects.insert(e.name);
    } else if (!pending_effects_.empty()) {
        fn_effects = std::move(pending_effects_);
    }
    pending_effects_.clear();

    // Infer effects from body (collect implicit_effect_call nodes)
    std::set<std::string> inferred_effects;
    std::function<void(const parser::ast::expression&)> collect_effects;
    collect_effects = [&](const parser::ast::expression& expr) {
        if (auto* iec = boost::get<boost::spirit::x3::forward_ast<parser::ast::implicit_effect_call>>(&expr)) {
            inferred_effects.insert(iec->get().effect_name.name + "." + iec->get().operation_name.name);
        }
        // Walk into blocks, function calls, etc.
        if (auto* fc = boost::get<boost::spirit::x3::forward_ast<parser::ast::function_call>>(&expr)) {
            for (auto& a : fc->get().arguments) collect_effects(a.get());
        }
    };
    for (auto& stmt : body_copy.statements) collect_effects(stmt.get());

    // Build parameter symbol list for kernel::Function
    std::vector<std::shared_ptr<kernel::Symbol>> param_symbols;
    param_symbols.reserve(params_copy.size());
    for (const auto& p : params_copy) {
        param_symbols.push_back(std::make_shared<kernel::Symbol>(p.name.name));
    }

    // The native impl captures the AST body and closure env by value
    kernel::Function::NativeImpl impl =
        [this, captured_env, body_copy, params_copy, fn_name, fn_effects]
        (const std::vector<kernel::Value>& args) -> kernel::Value {
            // Create child environment from the closure's captured env
            auto call_env = std::make_shared<Environment>(captured_env);
            for (size_t i = 0; i < params_copy.size() && i < args.size(); ++i) {
                call_env->define(params_copy[i].name.name, args[i], /*is_mutable=*/false);
            }

            // Push stack frame
            call_stack_.push_back(StackFrame{fn_name, SourceLocation{source_file_}});

            // If @uses was declared, enforce effect sandbox
            if (!fn_effects.empty())
                allowed_effects_stack_.push_back(fn_effects);

            // Swap environment, evaluate body, restore
            auto prev_env = env_;
            env_ = call_env;
            kernel::Value result;
            try {
                result = eval_block_expression(body_copy);
            } catch (const NonLocalReturn& nlr) {
                env_ = prev_env;
                call_stack_.pop_back();
                if (!fn_effects.empty()) allowed_effects_stack_.pop_back();
                return nlr.value;
            } catch (...) {
                env_ = prev_env;
                call_stack_.pop_back();
                if (!fn_effects.empty()) allowed_effects_stack_.pop_back();
                throw;
            }
            env_ = prev_env;
            call_stack_.pop_back();
            if (!fn_effects.empty()) allowed_effects_stack_.pop_back();
            return result;
        };

    auto fn = std::make_shared<kernel::Function>(
        std::move(param_symbols),
        kernel::Value{},  // body not used (NativeImpl handles it)
        std::move(impl),
        fn_name);

    // Multiple dispatch: if same name exists, chain implementations
    if (env_->has(fn_name)) {
        try {
            auto existing = env_->lookup(fn_name);
            if (existing.is<kernel::Function>() && existing.as<kernel::Function>()->impl()) {
                auto old_fn = existing.as<kernel::Function>();
                auto new_fn = fn;
                // Collect type annotations for all params
                std::vector<std::string> param_types;
                for (auto& p : params_copy) {
                    param_types.push_back(p.type.type_name.name);
                }
                kernel::Function::NativeImpl dispatch =
                    [new_fn, old_fn, param_types](const std::vector<kernel::Value>& args) -> kernel::Value {
                        // Check if all typed params match the args' __type__
                        bool match = true;
                        for (size_t i = 0; i < param_types.size() && i < args.size(); ++i) {
                            if (param_types[i].empty() || param_types[i] == "any") continue;
                            if (!args[i].is<kernel::Function>() || !args[i].as<kernel::Function>()->closure_env()) {
                                match = false; break;
                            }
                            auto& env = *args[i].as<kernel::Function>()->closure_env();
                            auto it = env.find("__type__");
                            if (it == env.end()) { match = false; break; }
                            std::string type_str = it->second.is<kernel::String>()
                                ? it->second.as<kernel::String>()->value()
                                : it->second.to_string();
                            if (type_str != param_types[i] && type_str != ":" + param_types[i]) {
                                match = false; break;
                            }
                        }
                        if (match && !param_types.empty() && !(param_types[0].empty() || param_types[0] == "any")) {
                            return (*new_fn->impl())(args);
                        }
                        return (*old_fn->impl())(args);
                    };
                fn = std::make_shared<kernel::Function>(
                    std::vector<std::shared_ptr<kernel::Symbol>>{}, kernel::Value{},
                    std::move(dispatch), fn_name);
            }
        } catch (...) {}
    }

    env_->define(fn_name, kernel::Value(fn), /*is_mutable=*/false);

    // Store inferred effects as metadata on the function
    if (!inferred_effects.empty()) {
        const void* fn_id = fn.get();
        std::string effects_str;
        for (auto& e : inferred_effects) { if (!effects_str.empty()) effects_str += ", "; effects_str += e; }
        metadata_table_[fn_id]["inferred_effects"] = kernel::Value(std::make_shared<kernel::String>(effects_str));
    }

    return kernel::Value(fn);
}

// ─── Lambda expression ──────────────────────────────────────────────

kernel::Value AstInterpreter::eval_lambda_expression(const parser::ast::lambda_expression& lambda) {
    auto captured_env = env_;
    // Copy by value to avoid dangling references in REPL mode
    auto body_copy = lambda.body;
    auto params_copy = lambda.parameters;
    bool is_block = lambda.is_block;

    std::vector<std::shared_ptr<kernel::Symbol>> param_symbols;
    param_symbols.reserve(params_copy.size());
    for (const auto& p : params_copy) {
        param_symbols.push_back(std::make_shared<kernel::Symbol>(p.name.name));
    }

    kernel::Function::NativeImpl impl =
        [this, captured_env, body_copy, params_copy]
        (const std::vector<kernel::Value>& args) -> kernel::Value {
            auto call_env = std::make_shared<Environment>(captured_env);
            for (size_t i = 0; i < params_copy.size() && i < args.size(); ++i) {
                call_env->define(params_copy[i].name.name, args[i], /*is_mutable=*/false);
            }

            call_stack_.push_back(StackFrame{"<lambda>", SourceLocation{source_file_}});

            auto prev_env = env_;
            env_ = call_env;
            kernel::Value result;
            try {
                result = evaluate(body_copy.get());
            } catch (const NonLocalReturn& nlr) {
                env_ = prev_env;
                call_stack_.pop_back();
                return nlr.value;
            } catch (...) {
                env_ = prev_env;
                call_stack_.pop_back();
                throw;
            }
            env_ = prev_env;
            call_stack_.pop_back();
            return result;
        };

    auto fn = std::make_shared<kernel::Function>(
        std::move(param_symbols),
        kernel::Value{},
        std::move(impl));

    return kernel::Value(fn);
}

// ─── Function call ──────────────────────────────────────────────────

kernel::Value AstInterpreter::eval_function_call(const parser::ast::function_call& call) {
    // ─── .with() — scoped effect handler override ───────────────────
    // expr.with(Effect1 { ... }, Effect2 { ... }) is parsed as:
    //   function_call { name="with", args=[expr, Effect1{...}, Effect2{...}] }
    if (call.function_name.name == "with" && !call.arguments.empty()) {
        // First arg is the expression to evaluate under overridden handlers
        // Remaining args are handle_expression nodes (effect handler impls)
        
        // Save current handlers
        auto saved_handlers = effect_handlers_;
        
        // Register override handlers (args 1..N are handler blocks)
        for (size_t i = 1; i < call.arguments.size(); ++i) {
            auto handler_val = evaluate(call.arguments[i].get());
            // The handler blocks are evaluated by eval_handle_expression
            // which registers them in effect_handlers_ as a side effect.
            // (Already done by evaluate() above)
        }
        
        // Evaluate the target expression (first arg) with overridden handlers
        kernel::Value result;
        try {
            result = evaluate(call.arguments[0].get());
        } catch (...) {
            effect_handlers_ = saved_handlers;
            throw;
        }
        effect_handlers_ = saved_handlers;
        return result;
    }

    // Evaluate callee
    kernel::Value callee;
    try {
        callee = env_->lookup(call.function_name.name);
    } catch (InterpreterError&) {
        // Function not found — try general method dispatch: look up method name, pass receiver as first arg
        if (!call.arguments.empty()) {
            try {
                auto method_fn = env_->lookup(call.function_name.name);
                std::vector<kernel::Value> all_args;
                for (const auto& arg : call.arguments)
                    all_args.push_back(evaluate(arg.get()));
                return apply_function(method_fn, all_args, source_location(call));
            } catch (InterpreterError&) {}
        }
        throw InterpreterError(
            std::format("Undefined function '{}'", call.function_name.name),
            source_location(call),
            call_stack_);
    }

    // Evaluate arguments left-to-right
    std::vector<kernel::Value> args;
    args.reserve(call.arguments.size());
    for (const auto& arg : call.arguments) {
        args.push_back(evaluate(arg.get()));
    }

    return apply_function(callee, args, source_location(call));
}

kernel::Value AstInterpreter::apply_function(const kernel::Value& callee,
                                              const std::vector<kernel::Value>& args,
                                              const SourceLocation& call_site) {
    auto fn = callee.as<kernel::Function>();
    if (!fn) {
        throw InterpreterError("Value is not callable", call_site, call_stack_);
    }

    // If the function has a NativeImpl, use it directly
    if (fn->impl()) {
        return (*fn->impl())(args);
    }

    // Fallback: shouldn't happen for interpreter-created functions
    throw InterpreterError("Function has no implementation", call_site, call_stack_);
}

// ─── Binary operations ──────────────────────────────────────────────

kernel::Value AstInterpreter::eval_binary_operation(const parser::ast::binary_operation& op) {
    // Assignment operator
    if (op.op == "=") {
        // LHS: identifier or field access (tuple_indexing)
        auto* id = boost::get<parser::ast::identifier>(&op.left.get());
        auto* ti = boost::get<boost::spirit::x3::forward_ast<parser::ast::tuple_indexing>>(&op.left.get());
        if (id) {
            auto value = evaluate(op.right.get());
            env_->assign(id->name, value);
            return value;
        } else if (ti) {
            // Field assignment: obj.field = value
            auto obj = evaluate(ti->get().tuple.get());
            auto value = evaluate(op.right.get());
            if (obj.is<kernel::Function>() && obj.as<kernel::Function>()->closure_env()) {
                auto fn = obj.as<kernel::Function>();
                auto& env = const_cast<kernel::Function::Environment&>(*fn->closure_env());
                env[ti->get().index] = value;
            }
            return value;
        }
        throw InterpreterError("Left side of assignment must be an identifier or field access",
                               source_location(op), call_stack_);
    }

    auto left = evaluate(op.left.get());
    auto right = evaluate(op.right.get());
    return apply_binary_op(op.op, left, right, source_location(op));
}

kernel::Value AstInterpreter::apply_binary_op(const std::string& op,
                                               const kernel::Value& left,
                                               const kernel::Value& right,
                                               const SourceLocation& loc) {
    // Integer arithmetic
    auto li = left.is<kernel::Integer>() ? left.as<kernel::Integer>() : nullptr;
    auto ri = right.is<kernel::Integer>() ? right.as<kernel::Integer>() : nullptr;
    if (li && ri) {
        int64_t lv = li->value(), rv = ri->value();
        if (op == "+") return kernel::Value(std::make_shared<kernel::Integer>(lv + rv));
        if (op == "-") return kernel::Value(std::make_shared<kernel::Integer>(lv - rv));
        if (op == "*") return kernel::Value(std::make_shared<kernel::Integer>(lv * rv));
        if (op == "/") {
            if (rv == 0) throw InterpreterError("Division by zero", loc, call_stack_);
            return kernel::Value(std::make_shared<kernel::Integer>(lv / rv));
        }
        if (op == "%") {
            if (rv == 0) throw InterpreterError("Modulo by zero", loc, call_stack_);
            return kernel::Value(std::make_shared<kernel::Integer>(lv % rv));
        }
        // Comparison
        if (op == "==") return kernel::Value(kernel::Boolean::from(lv == rv));
        if (op == "!=") return kernel::Value(kernel::Boolean::from(lv != rv));
        if (op == "<")  return kernel::Value(kernel::Boolean::from(lv < rv));
        if (op == "<=") return kernel::Value(kernel::Boolean::from(lv <= rv));
        if (op == ">")  return kernel::Value(kernel::Boolean::from(lv > rv));
        if (op == ">=") return kernel::Value(kernel::Boolean::from(lv >= rv));
    }

    // Float arithmetic
    auto lf = left.is<kernel::Float>() ? left.as<kernel::Float>() : nullptr;
    auto rf = right.is<kernel::Float>() ? right.as<kernel::Float>() : nullptr;
    if (lf && rf) {
        double l = lf->value(), r = rf->value();
        if (op == "+") return kernel::Value(std::make_shared<kernel::Float>(l + r));
        if (op == "-") return kernel::Value(std::make_shared<kernel::Float>(l - r));
        if (op == "*") return kernel::Value(std::make_shared<kernel::Float>(l * r));
        if (op == "/") return kernel::Value(std::make_shared<kernel::Float>(l / r));
        if (op == "==") return kernel::Value(kernel::Boolean::from(l == r));
        if (op == "!=") return kernel::Value(kernel::Boolean::from(l != r));
        if (op == "<")  return kernel::Value(kernel::Boolean::from(l < r));
        if (op == "<=") return kernel::Value(kernel::Boolean::from(l <= r));
        if (op == ">")  return kernel::Value(kernel::Boolean::from(l > r));
        if (op == ">=") return kernel::Value(kernel::Boolean::from(l >= r));
    }
    // Mixed int/float
    if (li && rf) {
        double l = li->value(), r = rf->value();
        if (op == "+") return kernel::Value(std::make_shared<kernel::Float>(l + r));
        if (op == "-") return kernel::Value(std::make_shared<kernel::Float>(l - r));
        if (op == "*") return kernel::Value(std::make_shared<kernel::Float>(l * r));
        if (op == "/") return kernel::Value(std::make_shared<kernel::Float>(l / r));
    }
    if (lf && ri) {
        double l = lf->value(), r = ri->value();
        if (op == "+") return kernel::Value(std::make_shared<kernel::Float>(l + r));
        if (op == "-") return kernel::Value(std::make_shared<kernel::Float>(l - r));
        if (op == "*") return kernel::Value(std::make_shared<kernel::Float>(l * r));
        if (op == "/") return kernel::Value(std::make_shared<kernel::Float>(l / r));
    }

    // String concatenation (also handles string + non-string via to_string)
    auto ls = left.is<kernel::String>() ? left.as<kernel::String>() : nullptr;
    auto rs = right.is<kernel::String>() ? right.as<kernel::String>() : nullptr;
    if (ls && rs) {
        if (op == "+") return kernel::Value(std::make_shared<kernel::String>(ls->value() + rs->value()));
        if (op == "==") return kernel::Value(kernel::Boolean::from(ls->value() == rs->value()));
        if (op == "!=") return kernel::Value(kernel::Boolean::from(ls->value() != rs->value()));
    }
    // String + anything → string concatenation
    if (ls && op == "+") {
        return kernel::Value(std::make_shared<kernel::String>(ls->value() + right.to_string()));
    }
    if (rs && op == "+") {
        return kernel::Value(std::make_shared<kernel::String>(left.to_string() + rs->value()));
    }

    // Boolean logical operators
    auto lb = left.is<kernel::Boolean>() ? left.as<kernel::Boolean>() : nullptr;
    auto rb = right.is<kernel::Boolean>() ? right.as<kernel::Boolean>() : nullptr;
    if (lb && rb) {
        if (op == "&&" || op == "and") return kernel::Value(kernel::Boolean::from(lb->value() && rb->value()));
        if (op == "||" || op == "or")  return kernel::Value(kernel::Boolean::from(lb->value() || rb->value()));
        if (op == "==") return kernel::Value(kernel::Boolean::from(lb->value() == rb->value()));
        if (op == "!=") return kernel::Value(kernel::Boolean::from(lb->value() != rb->value()));
    }

    // Struct equality: compare all fields
    if ((op == "==" || op == "!=") &&
        left.is<kernel::Function>() && left.as<kernel::Function>()->closure_env() &&
        right.is<kernel::Function>() && right.as<kernel::Function>()->closure_env()) {
        auto& l_env = *left.as<kernel::Function>()->closure_env();
        auto& r_env = *right.as<kernel::Function>()->closure_env();
        bool equal = (l_env.size() == r_env.size());
        if (equal) {
            for (auto& [k, v] : l_env) {
                auto it2 = r_env.find(k);
                if (it2 == r_env.end() || v.to_string() != it2->second.to_string()) { equal = false; break; }
            }
        }
        return kernel::Value(kernel::Boolean::from(op == "==" ? equal : !equal));
    }

    // Operator overloading: look up opr function by name
    static const std::unordered_map<std::string, std::string> op_names = {
        {"+", "add"}, {"-", "sub"}, {"*", "mul"}, {"/", "div"},
        {"==", "eq"}, {"!=", "neq"}, {"<", "lt"}, {">", "gt"},
        {"<=", "lte"}, {">=", "gte"}
    };
    auto it = op_names.find(op);
    if (it != op_names.end()) {
        try {
            auto fn = env_->lookup(it->second);
            return apply_function(fn, {left, right}, loc);
        } catch (InterpreterError&) {}
    }

    throw InterpreterError(
        std::format("Unsupported binary operator '{}' for given operand types", op),
        loc, call_stack_);
}

// ─── Unary operations ───────────────────────────────────────────────

kernel::Value AstInterpreter::eval_unary_operation(const parser::ast::unary_operation& op) {
    auto operand = evaluate(op.operand.get());
    return apply_unary_op(op.op, operand, source_location(op));
}

kernel::Value AstInterpreter::apply_unary_op(const std::string& op,
                                              const kernel::Value& operand,
                                              const SourceLocation& loc) {
    if (op == "-") {
        auto i = operand.as<kernel::Integer>();
        if (i) return kernel::Value(std::make_shared<kernel::Integer>(-i->value()));
    }
    if (op == "!" || op == "not") {
        auto b = operand.as<kernel::Boolean>();
        if (b) return kernel::Value(kernel::Boolean::from(!b->value()));
    }
    throw InterpreterError(
        std::format("Unsupported unary operator '{}' for given operand type", op),
        loc, call_stack_);
}

// ─── List expression ────────────────────────────────────────────────

kernel::Value AstInterpreter::eval_list_expression(const parser::ast::list_expression& list) {
    std::vector<kernel::Value> elements;
    elements.reserve(list.elements.size());
    for (const auto& elem : list.elements) {
        elements.push_back(evaluate(elem.get()));
    }
    return kernel::Value(std::make_shared<kernel::Vec>(std::move(elements)));
}

// ─── Block expression ───────────────────────────────────────────────

kernel::Value AstInterpreter::eval_block_expression(const parser::ast::block_expression& block) {
    auto block_env = env_->create_child();
    auto prev_env = env_;
    env_ = block_env;

    kernel::Value result;
    try {
        for (size_t i = 0; i < block.statements.size(); ++i) {
            const auto& stmt = block.statements[i];
            // Debug hook — fire before each statement
            if (debug_hook_) {
                DebugContext ctx{SourceLocation{source_file_, 0, 0}, *env_, call_stack_, call_stack_.size()};
                debug_hook_(ctx);
            }
            result = evaluate(stmt.get());
        }
    } catch (...) {
        env_ = prev_env;
        throw;
    }
    env_ = prev_env;
    return result;
}

// ─── Return statement ───────────────────────────────────────────────

kernel::Value AstInterpreter::eval_return_statement(const parser::ast::return_statement& ret) {
    kernel::Value val;
    if (ret.has_expression) {
        val = evaluate(ret.expr.get());
    }
    throw NonLocalReturn{val};
}

// ─── Effect suspend helper — Task 7.1 (implicit-effect-calls) ───────
// Shared evaluation for both perform_expression and implicit_effect_call.
// Requirement 3.1: identical primitive_suspend + continuation machinery.
// Requirement 3.3: the returned Value is the resumed value, so
//   val x = Effect.op(args) correctly binds x.
kernel::Value AstInterpreter::eval_effect_suspend(
        const std::string& effect_name,
        const std::string& operation_name,
        const std::vector<boost::spirit::x3::forward_ast<parser::ast::expression>>& arguments) {
    // Sandbox enforcement: if active, only allowed effects can be called
    if (!allowed_effects_stack_.empty()) {
        auto& allowed = allowed_effects_stack_.back();
        if (!allowed.empty()) {
            auto qualified = effect_name + "." + operation_name;
            // Allow if either the whole effect or the specific operation is listed
            if (allowed.find(qualified) == allowed.end() && allowed.find(effect_name) == allowed.end()) {
                throw InterpreterError(
                    std::format("Effect '{}.{}' is not permitted in this sandbox. Allowed: {}", effect_name, operation_name,
                        [&]{ std::string s; for (auto& e : allowed) { if (!s.empty()) s += ", "; s += e; } return s; }()),
                    SourceLocation{source_file_, 0, 0}, call_stack_);
            }
        }
    }

    // 1. Evaluate each argument expression left-to-right.
    std::vector<kernel::Value> arg_values;
    arg_values.reserve(arguments.size());
    for (const auto& arg : arguments) {
        arg_values.push_back(evaluate(arg.get()));
    }

    // 2. Dispatch through Meld-side handler registry first.
    auto it = effect_handlers_.find({effect_name, operation_name});
    if (it != effect_handlers_.end()) {
        return apply_function(it->second, arg_values,
                              SourceLocation{source_file_, 0, 0});
    }

    // 3. Fallback: legacy C++ EffectRuntime dispatch
    return effects::EffectRuntime::instance().perform_effect(
        effect_name, operation_name, arg_values);
}

// ─── perform_expression handler — Task 7.1 ─────────────────────────
// Delegates entirely to eval_effect_suspend so that runtime behaviour is
// identical regardless of whether the source used explicit perform { … }
// or the new implicit syntax.  Requirements 3.1, 3.2.
// Effect Firewall check added for Req 11.1, 11.2.
kernel::Value AstInterpreter::eval_perform_expression(
        const parser::ast::perform_expression& perform) {
    // ── Effect Firewall enforcement (Req 11.1, 11.2) ────────────────
    if (effect_firewall_) {
        effects::EffectSourceLocation loc{source_file_, 0, 0};
        bool permitted = effect_firewall_->check(
            perform.effect_name.name, source_file_, loc);
        if (!permitted) {
            auto violations = effect_firewall_->get_violations(source_file_);
            std::string reason = violations.empty() ? "effect not permitted"
                                                    : violations.back().reason;
            throw InterpreterError(
                std::format("Effect firewall violation: {}", reason),
                SourceLocation{source_file_, 0, 0},
                call_stack_);
        }
    }

    return eval_effect_suspend(perform.effect_name.name,
                               perform.operation_name.name,
                               perform.arguments);
}

// ─── implicit_effect_call handler — Task 7.2 ────────────────────────
// Produces the exact same runtime behaviour as perform_expression by
// calling the shared eval_effect_suspend helper.  Requirements 3.1, 3.2, 3.3.
// Effect Firewall check added for Req 11.1, 11.2.
kernel::Value AstInterpreter::eval_implicit_effect_call(
        const parser::ast::implicit_effect_call& call) {
    // First try: method dispatch on receiver object
    // implicit_effect_call has effect_name (receiver) and operation_name (method)
    try {
        auto receiver = env_->lookup(call.effect_name.name);
        // General: look up method as a function, call with receiver as first arg
        std::string method = call.operation_name.name;
        try {
            auto method_fn = env_->lookup(method);
            std::vector<kernel::Value> all_args;
            all_args.push_back(receiver);
            for (const auto& a : call.arguments) all_args.push_back(evaluate(a.get()));
            return apply_function(method_fn, all_args, SourceLocation{source_file_, 0, 0});
        } catch (InterpreterError&) {}
    } catch (const InterpreterError&) {
        // Not a variable — fall through to effect handling
    }

    // ── Effect Firewall enforcement (Req 11.1, 11.2) ────────────────
    if (effect_firewall_) {
        effects::EffectSourceLocation loc{source_file_, 0, 0};
        bool permitted = effect_firewall_->check(
            call.effect_name.name, source_file_, loc);
        if (!permitted) {
            auto violations = effect_firewall_->get_violations(source_file_);
            std::string reason = violations.empty() ? "effect not permitted"
                                                    : violations.back().reason;
            throw InterpreterError(
                std::format("Effect firewall violation: {}", reason),
                SourceLocation{source_file_, 0, 0},
                call_stack_);
        }
    }

    return eval_effect_suspend(call.effect_name.name,
                               call.operation_name.name,
                               call.arguments);
}

kernel::Value AstInterpreter::eval_assertion(
        const parser::ast::assertion_expression& assertion) {
    auto cond = evaluate(assertion.condition.get());
    if (!cond.is_truthy()) {
        std::string msg = assertion.has_message ? assertion.message : "Assertion failed";
        throw InterpreterError(msg, SourceLocation{source_file_, 0, 0}, call_stack_);
    }
    return kernel::Value{};
}

kernel::Value AstInterpreter::eval_test_block(
        const parser::ast::test_block& test) {
    std::string desc = test.has_description ? test.description : "<unnamed test>";
    try {
        // Evaluate each statement in the test body individually
        auto& stmts = test.body.get().statements;
        for (const auto& stmt : stmts) {
            evaluate(stmt.get());
        }
        std::cerr << "  PASS: " << desc << "\n";
    } catch (const InterpreterError& e) {
        std::cerr << "  FAIL: " << desc << " — " << e.what() << "\n";
    }
    return kernel::Value{};
}

kernel::Value AstInterpreter::eval_handle_expression(
        const parser::ast::handle_expression& handle) {
    // Helper: install handlers from inline_trait_impl list
    auto install_handlers = [this](const std::vector<parser::ast::inline_trait_impl>& handlers) {
        for (const auto& impl : handlers) {
            const std::string& effect_name = impl.trait_name.name;
            for (const auto& method : impl.methods) {
                auto body_copy = method.body.get();
                auto params_copy = method.parameters;
                auto captured_env = env_;
                auto method_name = method.name.name;

                kernel::Function::NativeImpl fn_impl =
                    [this, captured_env, body_copy, params_copy]
                    (const std::vector<kernel::Value>& args) -> kernel::Value {
                        auto call_env = std::make_shared<Environment>(captured_env);
                        for (size_t i = 0; i < params_copy.size() && i < args.size(); ++i) {
                            call_env->define(params_copy[i].name.name, args[i], false);
                        }
                        auto prev_env = env_;
                        env_ = call_env;
                        kernel::Value result;
                        try {
                            result = eval_block_expression(body_copy);
                        } catch (const NonLocalReturn& ret) {
                            env_ = prev_env;
                            return ret.value;
                        } catch (...) {
                            env_ = prev_env;
                            throw;
                        }
                        env_ = prev_env;
                        return result;
                    };

                register_effect_handler(effect_name, method_name,
                    kernel::Value(std::make_shared<kernel::Function>(
                        std::vector<std::shared_ptr<kernel::Symbol>>{},
                        kernel::Value{},
                        std::move(fn_impl),
                        effect_name + "." + method_name)));
            }
        }
    };

    // If body is empty, this is a top-level handler registration.
    if (handle.body.get().statements.empty() && !handle.handlers.empty()) {
        install_handlers(handle.handlers);
        return kernel::Value{};
    }

    // Non-empty body: scoped handler — install, evaluate body, restore.
    auto saved_handlers = effect_handlers_;
    install_handlers(handle.handlers);
    kernel::Value result;
    try {
        result = eval_block_expression(handle.body.get());
    } catch (...) {
        effect_handlers_ = saved_handlers;
        throw;
    }
    effect_handlers_ = saved_handlers;
    return result;
}

kernel::Value AstInterpreter::eval_import(const parser::ast::import_declaration& imp) {
    // Convert namespace path to filesystem path: imp std.math -> std/math.meld
    if (imp.namespace_path.empty()) return kernel::Value{};

    std::filesystem::path base;
    if (!source_file_.empty()) {
        base = std::filesystem::path(source_file_).parent_path();
    } else {
        base = std::filesystem::current_path();
    }

    // Build relative path from module path segments
    std::filesystem::path module_path = base;
    for (const auto& seg : imp.namespace_path) {
        module_path /= seg;
    }
    module_path += ".meld";

    // Also try meld-core/std/ prefix for standard library
    std::filesystem::path std_path;
    if (imp.namespace_path.size() >= 1 && imp.namespace_path[0] == "std") {
        std_path = base;
        // Walk up to find project root (look for meld-core/)
        auto root = base;
        for (int i = 0; i < 10; ++i) {
            if (std::filesystem::exists(root / "meld-core")) break;
            root = root.parent_path();
        }
        std_path = root / "meld-core";
        for (const auto& seg : imp.namespace_path) {
            std_path /= seg;
        }
        std_path += ".meld";
    }

    // Try to find the file
    std::filesystem::path resolved;
    if (std::filesystem::exists(module_path)) {
        resolved = module_path;
    } else if (!std_path.empty() && std::filesystem::exists(std_path)) {
        resolved = std_path;
    } else {
        // Module not found — silently skip (may be a forward declaration)
        return kernel::Value{};
    }

    // Read the file
    std::ifstream file(resolved);
    if (!file.is_open()) return kernel::Value{};
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Parse with hand-written parser
    // Parse with X3
    parser::Parser p;
    std::vector<parser::ast::expression> ast;
    if (!p.parse_file(source, ast) || ast.empty()) {
        throw InterpreterError(
            "Import parse error in '" + resolved.string() + "': " + p.error_message(),
            SourceLocation{resolved.string(), 0, 0},
            call_stack_);
    }

    // Evaluate in a child environment
    auto child_env = std::make_shared<Environment>(env_);
    auto saved_env = env_;
    auto saved_source = source_file_;
    env_ = child_env;
    source_file_ = resolved.string();

    for (const auto& expr : ast) {
        evaluate(expr);
    }

    env_ = saved_env;
    source_file_ = saved_source;

    // Merge exported definitions into current environment
    // For aliased imports: imp math = std.math → define "math" namespace
    if (imp.has_alias && !imp.alias.empty()) {
        // TODO: create namespace object
        env_->define(imp.alias, kernel::Value::from_symbol(imp.alias), false);
    } else if (!imp.symbols.empty()) {
        // Destructured: imp { sin, cos } = std.math
        for (const auto& sym : imp.symbols) {
            auto val = child_env->get(sym.original_name);
            if (val) {
                env_->define(sym.local_name, *val, false);
            }
        }
    } else {
        // Basic: imp std.math → merge all definitions
        auto all = child_env->all_bindings();
        for (const auto& [name, val] : all) {
            if (name.starts_with("__")) continue; // skip internal
            env_->define(name, val, false);
        }
    }

    return kernel::Value{};
}

kernel::Value AstInterpreter::eval_array_indexing(const parser::ast::array_indexing& idx) {
    auto arr = evaluate(idx.array.get());
    auto index = evaluate(idx.index.get());
    if (arr.is<kernel::Vec>()) {
        auto vec = arr.as<kernel::Vec>();
        if (index.is<kernel::Integer>()) {
            auto& elems = vec->elements();
            int64_t i = index.as<kernel::Integer>()->value();
            if (i >= 0 && static_cast<size_t>(i) < elems.size())
                return elems[i];
        }
    }
    if (arr.is<kernel::IntVec>()) {
        auto vec = arr.as<kernel::IntVec>();
        if (index.is<kernel::Integer>()) {
            int64_t i = index.as<kernel::Integer>()->value();
            if (i >= 0 && static_cast<size_t>(i) < vec->size())
                return kernel::Value(std::make_shared<kernel::Integer>(vec->at(static_cast<size_t>(i))));
        }
    }
    return kernel::Value{};
}

kernel::Value AstInterpreter::eval_dot_access(const parser::ast::tuple_indexing& dot) {
    auto receiver = evaluate(dot.tuple.get());
    const auto& field = dot.index;

    // Struct field access (stored as Function with closure)
    if (receiver.is<kernel::Function>()) {
        auto fn = receiver.as<kernel::Function>();
        if (fn->closure_env()) {
            auto it = fn->closure_env()->find(field);
            if (it != fn->closure_env()->end()) return it->second;
        }
    }

    // Built-in string methods
    if (receiver.is<kernel::String>()) {
        auto s = receiver.as<kernel::String>();
        const auto& str = s->value();
        if (field == "length") return kernel::Value(std::make_shared<kernel::Integer>(str.size()));
        if (field == "trim") {
            auto start = str.find_first_not_of(" \t\n\r");
            auto end = str.find_last_not_of(" \t\n\r");
            return kernel::Value(std::make_shared<kernel::String>(
                start == std::string::npos ? "" : str.substr(start, end - start + 1)));
        }
        if (field == "to-upper") {
            std::string upper = str;
            for (auto& c : upper) c = std::toupper(c);
            return kernel::Value(std::make_shared<kernel::String>(upper));
        }
        if (field == "to-lower") {
            std::string lower = str;
            for (auto& c : lower) c = std::tolower(c);
            return kernel::Value(std::make_shared<kernel::String>(lower));
        }
        if (field == "is-empty") return kernel::Value(kernel::Boolean::from(str.empty()));
    }

    // Built-in integer methods
    if (receiver.is<kernel::Integer>()) {
        auto i = receiver.as<kernel::Integer>();
        if (field == "abs") return kernel::Value(std::make_shared<kernel::Integer>(std::abs(i->value())));
    }

    // Built-in boolean methods
    if (receiver.is<kernel::Boolean>()) {
        if (field == "ifTrue" || field == "ifFalse") return receiver;
    }

    // Namespace-style access: Type.method
    return kernel::Value::from_symbol(field);
}

kernel::Value AstInterpreter::eval_initialization_block(
        const parser::ast::initialization_block& init) {
    // Store struct instance as a Function with closure containing fields
    kernel::Function::Environment fields;
    fields["__type__"] = kernel::Value(std::make_shared<kernel::Symbol>(init.type_name.name));
    for (const auto& param : init.parameters) {
        auto value = evaluate(param.value.get());
        fields[param.name.name] = value;
    }
    // Create a "struct" as a function with a closure (fields accessible via dot)
    auto instance = std::make_shared<kernel::Function>(
        std::vector<std::shared_ptr<kernel::Symbol>>{},
        kernel::Value{},
        std::nullopt,
        init.type_name.name,
        std::move(fields));
    return kernel::Value(instance);
}

// ═══════════════════════════════════════════════════════════════════════════
// FFI dispatch helpers (shared by effect handlers and native_functions_)
// ═══════════════════════════════════════════════════════════════════════════

kernel::Value AstInterpreter::ffi_dispatch_int(const std::vector<kernel::Value>& args) {
    if (args.size() < 2)
        throw InterpreterError("FFI.call requires (handle, func_name, ...args)", SourceLocation{source_file_, 0, 0}, call_stack_);
    int64_t handle_id = args[0].is<kernel::Integer>() ? args[0].as<kernel::Integer>()->value() : -1;
    if (handle_id < 0 || handle_id >= (int64_t)ffi_handles_.size())
        throw InterpreterError("FFI.call: invalid library handle", SourceLocation{source_file_, 0, 0}, call_stack_);
    std::string func_name = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string();
    void* sym = dlsym(ffi_handles_[handle_id], func_name.c_str());
    if (!sym)
        throw InterpreterError("FFI.call: symbol not found: " + func_name, SourceLocation{source_file_, 0, 0}, call_stack_);
    std::vector<int64_t> c_args;
    std::vector<std::string> string_storage;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].is<kernel::Integer>()) c_args.push_back(args[i].as<kernel::Integer>()->value());
        else if (args[i].is<kernel::Float>()) {
            double d = args[i].as<kernel::Float>()->value();
            int64_t bits; std::memcpy(&bits, &d, sizeof(bits));
            c_args.push_back(bits);
        } else if (args[i].is<kernel::String>()) {
            string_storage.push_back(args[i].as<kernel::String>()->value());
            c_args.push_back(reinterpret_cast<int64_t>(string_storage.back().c_str()));
        } else if (args[i].is<kernel::Boolean>()) c_args.push_back(args[i].as<kernel::Boolean>()->value() ? 1 : 0);
        else c_args.push_back(0);
    }
    using Fn0 = int64_t(*)(); using Fn1 = int64_t(*)(int64_t);
    using Fn2 = int64_t(*)(int64_t, int64_t); using Fn3 = int64_t(*)(int64_t, int64_t, int64_t);
    using Fn4 = int64_t(*)(int64_t, int64_t, int64_t, int64_t);
    using Fn5 = int64_t(*)(int64_t, int64_t, int64_t, int64_t, int64_t);
    using Fn6 = int64_t(*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
    int64_t result = 0;
    switch (c_args.size()) {
        case 0: result = reinterpret_cast<Fn0>(sym)(); break;
        case 1: result = reinterpret_cast<Fn1>(sym)(c_args[0]); break;
        case 2: result = reinterpret_cast<Fn2>(sym)(c_args[0], c_args[1]); break;
        case 3: result = reinterpret_cast<Fn3>(sym)(c_args[0], c_args[1], c_args[2]); break;
        case 4: result = reinterpret_cast<Fn4>(sym)(c_args[0], c_args[1], c_args[2], c_args[3]); break;
        case 5: result = reinterpret_cast<Fn5>(sym)(c_args[0], c_args[1], c_args[2], c_args[3], c_args[4]); break;
        case 6: result = reinterpret_cast<Fn6>(sym)(c_args[0], c_args[1], c_args[2], c_args[3], c_args[4], c_args[5]); break;
        default: throw InterpreterError("FFI.call: max 6 arguments supported", SourceLocation{source_file_, 0, 0}, call_stack_);
    }
    return kernel::Value(std::make_shared<kernel::Integer>(result));
}

kernel::Value AstInterpreter::ffi_dispatch_float(const std::vector<kernel::Value>& args) {
    if (args.size() < 2) throw InterpreterError("FFI.call-float requires (handle, func_name, ...args)", SourceLocation{source_file_, 0, 0}, call_stack_);
    int64_t handle_id = args[0].is<kernel::Integer>() ? args[0].as<kernel::Integer>()->value() : -1;
    if (handle_id < 0 || handle_id >= (int64_t)ffi_handles_.size())
        throw InterpreterError("FFI.call-float: invalid handle", SourceLocation{source_file_, 0, 0}, call_stack_);
    std::string func_name = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string();
    void* sym = dlsym(ffi_handles_[handle_id], func_name.c_str());
    if (!sym) throw InterpreterError("FFI.call-float: symbol not found: " + func_name, SourceLocation{source_file_, 0, 0}, call_stack_);
    std::vector<double> d_args;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].is<kernel::Float>()) d_args.push_back(args[i].as<kernel::Float>()->value());
        else if (args[i].is<kernel::Integer>()) d_args.push_back(static_cast<double>(args[i].as<kernel::Integer>()->value()));
        else d_args.push_back(0.0);
    }
    using DFn0 = double(*)(); using DFn1 = double(*)(double); using DFn2 = double(*)(double, double);
    double result = 0.0;
    switch (d_args.size()) {
        case 0: result = reinterpret_cast<DFn0>(sym)(); break;
        case 1: result = reinterpret_cast<DFn1>(sym)(d_args[0]); break;
        case 2: result = reinterpret_cast<DFn2>(sym)(d_args[0], d_args[1]); break;
        default: throw InterpreterError("FFI.call-float: max 2 float arguments supported", SourceLocation{source_file_, 0, 0}, call_stack_);
    }
    return kernel::Value(std::make_shared<kernel::Float>(result));
}

kernel::Value AstInterpreter::ffi_dispatch_string(const std::vector<kernel::Value>& args) {
    if (args.size() < 2) throw InterpreterError("FFI.call-string requires (handle, func_name, ...args)", SourceLocation{source_file_, 0, 0}, call_stack_);
    int64_t handle_id = args[0].is<kernel::Integer>() ? args[0].as<kernel::Integer>()->value() : -1;
    if (handle_id < 0 || handle_id >= (int64_t)ffi_handles_.size())
        throw InterpreterError("FFI.call-string: invalid handle", SourceLocation{source_file_, 0, 0}, call_stack_);
    std::string func_name = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string();
    void* sym = dlsym(ffi_handles_[handle_id], func_name.c_str());
    if (!sym) throw InterpreterError("FFI.call-string: symbol not found: " + func_name, SourceLocation{source_file_, 0, 0}, call_stack_);
    std::vector<int64_t> c_args; std::vector<std::string> ss;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].is<kernel::Integer>()) c_args.push_back(args[i].as<kernel::Integer>()->value());
        else if (args[i].is<kernel::String>()) { ss.push_back(args[i].as<kernel::String>()->value()); c_args.push_back(reinterpret_cast<int64_t>(ss.back().c_str())); }
        else c_args.push_back(0);
    }
    using Fn0 = const char*(*)(); using Fn1 = const char*(*)(int64_t); using Fn2 = const char*(*)(int64_t, int64_t);
    const char* r = nullptr;
    switch (c_args.size()) {
        case 0: r = reinterpret_cast<Fn0>(sym)(); break;
        case 1: r = reinterpret_cast<Fn1>(sym)(c_args[0]); break;
        case 2: r = reinterpret_cast<Fn2>(sym)(c_args[0], c_args[1]); break;
        default: throw InterpreterError("FFI.call-string: max 2 arguments supported", SourceLocation{source_file_, 0, 0}, call_stack_);
    }
    return kernel::Value(std::make_shared<kernel::String>(r ? r : ""));
}

kernel::Value AstInterpreter::ffi_dispatch_void(const std::vector<kernel::Value>& args) {
    if (args.size() < 2) throw InterpreterError("FFI.call-void requires (handle, func_name, ...args)", SourceLocation{source_file_, 0, 0}, call_stack_);
    int64_t handle_id = args[0].is<kernel::Integer>() ? args[0].as<kernel::Integer>()->value() : -1;
    if (handle_id < 0 || handle_id >= (int64_t)ffi_handles_.size())
        throw InterpreterError("FFI.call-void: invalid handle", SourceLocation{source_file_, 0, 0}, call_stack_);
    std::string func_name = args[1].is<kernel::String>() ? args[1].as<kernel::String>()->value() : args[1].to_string();
    void* sym = dlsym(ffi_handles_[handle_id], func_name.c_str());
    if (!sym) throw InterpreterError("FFI.call-void: symbol not found: " + func_name, SourceLocation{source_file_, 0, 0}, call_stack_);
    std::vector<int64_t> c_args; std::vector<std::string> ss;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i].is<kernel::Integer>()) c_args.push_back(args[i].as<kernel::Integer>()->value());
        else if (args[i].is<kernel::String>()) { ss.push_back(args[i].as<kernel::String>()->value()); c_args.push_back(reinterpret_cast<int64_t>(ss.back().c_str())); }
        else c_args.push_back(0);
    }
    using VFn0 = void(*)(); using VFn1 = void(*)(int64_t); using VFn2 = void(*)(int64_t, int64_t);
    switch (c_args.size()) {
        case 0: reinterpret_cast<VFn0>(sym)(); break;
        case 1: reinterpret_cast<VFn1>(sym)(c_args[0]); break;
        case 2: reinterpret_cast<VFn2>(sym)(c_args[0], c_args[1]); break;
        default: break;
    }
    return kernel::Value(kernel::Empty::instance());
}

// ═══════════════════════════════════════════════════════════════════════════
// Native Function Registry — kernel.call dispatch table
// All I/O is now handled by first-class effects. This table is kept
// only for backward compatibility with kernel.call("name", args...).
// ═══════════════════════════════════════════════════════════════════════════

void AstInterpreter::register_native_functions() {
    // All native functions are now registered as effect handlers.
    // kernel.call falls back to env_->lookup() for any remaining uses.
}

} // namespace meld::interpreter
