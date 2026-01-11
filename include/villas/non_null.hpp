#pragma once

#include <exception>
#include <type_traits>

namespace villas::exception {

struct bad_null_pointer : public std::exception {
  const char *what() const noexcept override {
    return "cannot construct non_null pointer from nullptr";
  }
};

} // namespace villas::exception

namespace villas {

template <typename T> class non_null final {
public:
  // infallible implicit conversion from reference
  constexpr non_null(T &value) noexcept : ptr(&value) {}
  constexpr non_null(std::nullptr_t) = delete;
  constexpr non_null(T &&value) = delete;

  // allow implicit pointer conversions
  template <typename U>
    requires std::is_convertible_v<T *, U *>
  constexpr non_null(non_null<U> const &other) noexcept : ptr(other.get()) {}

  // explicit conversion from nullable pointer
  constexpr explicit non_null(T *ptr) : ptr(ptr) {
    if (ptr == nullptr)
      throw exception::bad_null_pointer{};
  }

  // smart pointer interface
  constexpr T &operator*() const noexcept { return *ptr; }
  constexpr T *operator->() const noexcept { return ptr; }
  constexpr operator T &() const noexcept { return *ptr; }
  constexpr T *get() const noexcept { return ptr; }

  // this pointer is not nullable and need not be checked for null
  friend bool operator==(non_null const &lhs, std::nullptr_t) = delete;

  // this pointer compares by address by default
  friend constexpr bool operator==(non_null const &lhs,
                                   non_null const &rhs) noexcept = default;

private:
  T *ptr;
};

} // namespace villas
