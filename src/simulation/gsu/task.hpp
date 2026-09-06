#pragma once

#include <coroutine>
#include <cstdint>
#include <exception>
#include <type_traits>
#include <utility>

namespace starfox::simulation::gsu {

// Nested operations transfer control directly to their children. Only clock
// waits return to the caller; no OS threads or wall-clock waits occur.
template<class T> struct Result {
    T value{};
    void return_value(T result) noexcept { value = result; }
};
template<> struct Result<void> { void return_void() noexcept {} };

template<class T = void> class Task {
public:
    struct promise_type : Result<T> {
        std::coroutine_handle<> continuation{std::noop_coroutine()};
        std::exception_ptr error;
        Task get_return_object() { return Task{Handle::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        struct Final {
            bool await_ready() const noexcept { return false; }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> handle) const noexcept {
                return handle.promise().continuation;
            }
            void await_resume() const noexcept {}
        };
        Final final_suspend() noexcept { return {}; }
        void unhandled_exception() noexcept { error = std::current_exception(); }
    };
    using Handle = std::coroutine_handle<promise_type>;
    Task() = default;
    explicit Task(Handle handle) noexcept : handle_(handle) {}
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    ~Task() { if (handle_) handle_.destroy(); }
    Task(const Task&) = delete;
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
        handle_.promise().continuation = parent;
        return handle_;
    }
    auto await_resume() {
        check();
        if constexpr (!std::is_void_v<T>) return handle_.promise().value;
    }
    void check() const {
        if (handle_.promise().error) std::rethrow_exception(handle_.promise().error);
    }
    Handle handle() const noexcept { return handle_; }
private:
    Handle handle_{};
};

using u8 = std::uint8_t; using u32 = std::uint32_t; using s32 = std::int32_t;
using n8 = std::uint8_t; using n16 = std::uint16_t; using n32 = std::uint32_t;
using i8 = std::int8_t; using i16 = std::int16_t;

template<unsigned Bits> struct Masked {
    using Storage = std::conditional_t<(Bits <= 8), std::uint8_t, std::uint32_t>;
    static constexpr Storage mask = (std::uint32_t{1} << Bits) - 1U;
    Storage data{};
    Masked() = default;
    template<class T> Masked(T value) : data(Storage(value) & mask) {}
    template<class T> Masked& operator=(T value) { data = Storage(value) & mask; return *this; }
    operator Storage() const noexcept { return data; }
    unsigned bit(unsigned first, unsigned last) const noexcept {
        return (data >> first) & ((1U << (last - first + 1U)) - 1U);
    }
    unsigned bit(unsigned index) const noexcept { return bit(index,index); }
};
using n4 = Masked<4>; using n24 = Masked<24>;

template<unsigned Bits, unsigned Index> struct BitField {
    n16* target;
    explicit BitField(n16* value) noexcept : target(value) {}
    operator bool() const noexcept { return (*target & (1U << Index)) != 0U; }
    BitField& operator=(bool value) noexcept {
        *target = n16((*target & ~(1U << Index)) | (unsigned(value) << Index)); return *this;
    }
    BitField& operator=(const BitField& value) noexcept { return *this = bool(value); }
};
template<unsigned Bits, unsigned First, unsigned Last> struct BitRange {
    n16* target;
    explicit BitRange(n16* value) noexcept : target(value) {}
    operator u32() const noexcept { return (*target >> First) & ((1U << (Last - First + 1U)) - 1U); }
};
struct Range {
    struct Iterator {
        unsigned value;
        unsigned operator*() const noexcept { return value; }
        Iterator& operator++() noexcept { ++value; return *this; }
        bool operator!=(Iterator other) const noexcept { return value != other.value; }
    };
    unsigned count;
    Iterator begin() const noexcept { return {0U}; }
    Iterator end() const noexcept { return {count}; }
};
inline Range range(unsigned count) noexcept { return {count}; }

} // namespace starfox::simulation::gsu
