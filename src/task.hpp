#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

// move-only replacement for std::function<void()>.
class Task {
  public:
    Task() noexcept = default;

    template <typename F>
        requires(!std::same_as<std::decay_t<F>, Task> && std::invocable<F&>)
    Task(F&& f) : impl_(std::make_unique<Impl<std::decay_t<F>>>(std::forward<F>(f))) {}

    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() = default;

    void operator()() {
        if (!impl_) {
            throw std::bad_function_call();
        }
        impl_->call();
    }

    explicit operator bool() const noexcept { return impl_ != nullptr; }

  private:
    struct Base {
        virtual ~Base() = default;
        virtual void call() = 0;
    };

    template <typename F> struct Impl final : Base {
        F fn;

        explicit Impl(F&& f) : fn(std::move(f)) {}
        explicit Impl(const F& f) : fn(f) {}

        void call() override { fn(); }
    };

    std::unique_ptr<Base> impl_;
};