/* A type-safe tagged_union sum type for C++
 *
 *
 * This tagged_union type is an alternative to C++17's `std::variant`.
 * The main difference is that this type forces you to explicitly
 * specify a tag type. This is deliberate design decision which reduces
 * some the compile time complexity and overhead incurred by std::variant.
 *
 * The tag type of a tagged union must be an enumeration type. You specify the
 * tag type by passing a special `npos` value to the `tagged_union` template. This
 * `npos` value is assumed to be an *invalid* value of the tag type with a numeric
 * value equal to the total number of enum members. The tagged variant types of
 * the `tagged_union` are then defined by the remaining template arguments.
 *
 *     enum class Tag { INTEGER, FLOAT, COMPLEX, npos };
 *     using Union = villas::tagged_union<Tag::npos, int, float, std::complex<float>>;
 *
 *
 * A union's special member functions are conditionally trivial, meaning that
 * moving or copying a union of trivial types will be a simple memcpy.
 *
 *     // this union is trivial because it only contains numeric types
 *     static_assert(std::is_trivially_copyable_v<Union>);
 *
 *     // this union is not trivial because std::string is not trivial
 *     enum class AnotherTag { INTEGER, STRING, npos};
 *     using NonTrivialUnion = villas::tagged_union<AnotherTag::npos, int, std::string>;
 *     static_assert(not std::is_trivially_copyable_v<villas::tagged_union<TA>>);
 *
 *
 * A tagged union will default construct to an invalid/empty state with a tag value
 * of `tagged_union::npos`. You can check for this state using the `empty` non-static
 * member function, the `std::empty` function, the `operator bool` conversion or by
 * comparing the union's tag to `tagged_union::npos`.
 *
 *     auto default_value = {};
 *     assert(default_value.empty());
 *     assert(std::empty(default_value));
 *     assert(not bool{default_value});
 *     assert(default_value.tag() == Union::npos);
 *
 *
 * Variants of the `tagged_union` can be constructed using the `villas::in_place_tag`
 * helper or the tagged_union::make factory function.
 *
 *     // construct a union with a statically known tag
 *     auto a = Union::make<Tag::INTEGER>(10);
 *     auto b = Union{villas::in_place_tag<Tag::COMPLEX>, 1.0, 2.0};
 *     Union c = villas::in_place_tag<Tag::FLOAT>;
 *
 *
 * You can also default construct a `tagged_union` using a tag only known at runtime
 * as long as all variant types are default constructible. This runtime
 * constructor even works for other constructors when all possible types
 * are constructible using the given arguments.
 *
 *     // construct a union with a runtime known tag
 *     auto d = Union{Tag::FLOAT};
 *     Union e = Tag::FLOAT;
 *     auto f = Union{some_tag, 10};
 *
 *
 * You can of course also construct a type into an existing `tagged_union` using `emplace`
 * or reset it to the empty state using `reset`.
 *
 *     e.emplace<Tag::COMPLEX>(1, 1);
 *     c.reset();
 *
 *
 * You can either get the union's tag using its `tag` non-static member function or
 * use the implicit conversion to the tag_type.
 *
 *     // switch on a tagged_union which implicitly converts to its tag-type
 *     switch (f) {
 *       using enum Tag;
 *     case INTEGER:
 *       foo();
 *     case FLOAT:
 *     case COMPLEX:
 *       bar();
 *     case Union::npos:
 *       baz();
 *     }
 *
 *
 * The `get` and `get_if` members allow you to access the union's contained value.
 * Remember that `get` throws a `bad_tagged_union_access` exception when the
 * union's tag doesn't match, while `get_if` returns a nullptr.
 *
 *     // this may throw
 *     auto &cmplx = e.get<Tag::COMPLEX>();
 *
 *     // this will never throw
 *     if (auto *ptr = d.get_if<Tag::FLOAT>()) {
 *         auto f = *ptr;
 *     }
 *
 *
 * While `switch` statements are the best way to inspect just the tag of a `tagged_union`,
 * there are better ways to inspect the value inside a `tagged_union`.
 * The `visit` non-static member function works like the `std::visit` function does for
 * `std::variant`, although `tagged_union::visit` does not support multiple visitation.
 *
 *     // visit the union with an overload set constructed by the overloaded helper
 *     e.visit(villas::utils::overloaded {
 *       [](int i){ foo(i); },
 *       [](float f){ bar(f); },
 *       [](std::complex<float> c){ bar(c); },
 *       [](){ baz(); }});
 *
 *
 * If a union has no value and the visit function can't call your visitor without any
 * arguments, it will throw a `bad_tagged_union_access` exception.
 *
 *
 * Advanced uses also allow you to get the tag to which the visited value belongs. Even
 * as a constant expression.
 *
 *     // get the tag as a runtime known value
 *     e.visit([](Tag tag, auto value){});
 *
 *     // get the tag as a compiletime known value
 *     e.visit([]<Tag tag>(villas::tag_constant<tag>, auto value){
 *       // this allows you to do additional compile time checks based on the tag
 *       static_assert(tag != Tag::npos);
 *     });
 *
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <compare>
#include <concepts>
#include <exception>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace villas {

namespace tagged_union_impl {

template <std::integral auto index, typename Head, typename... Tail>
consteval auto pack_index_impl() {
  if constexpr (index == 0)
    return std::type_identity<Head>{};
  else
    return pack_index_impl<index - 1, Tail...>();
}

template <std::integral auto index, typename... T>
struct pack_index : decltype(pack_index_impl<index, T...>()) {};

template <std::integral auto index, typename... T>
using pack_index_t = pack_index<index, T...>::type;

using copy_fn = void (*)(std::byte *, std::byte const *);
using move_fn = void (*)(std::byte *, std::byte *);
using destruct_fn = void (*)(std::byte *);

void copy(copy_fn const vtable[], std::size_t, std::byte *, std::byte const *);
void move(move_fn const vtable[], std::size_t, std::byte *, std::byte *);
void destruct(destruct_fn const vtable[], std::size_t, std::byte *);

} // namespace tagged_union_impl

struct bad_tagged_union_access : std::exception {
  bad_tagged_union_access() = default;

  char const *what() const noexcept override {
    return "tried to access inactive variant of a tagged_union type";
  }
};

template <auto tag>
struct tag_constant : std::integral_constant<decltype(tag), tag> {};

template <auto tag> constexpr static tag_constant<tag> in_place_tag{};

template <typename E, E... t> struct tag_sequence {
  constexpr static std::size_t size() noexcept { return sizeof...(t); };
};

template <auto npos> consteval static auto make_tag_sequence() {
  using tag_type = decltype(npos);
  using underlying_type =
      std::make_unsigned_t<std::underlying_type_t<decltype(npos)>>;

  return [&]<underlying_type... index>(
             std::integer_sequence<underlying_type, index...>) {
    return tag_sequence<tag_type, tag_type{index}...>{};
  }(std::make_integer_sequence<underlying_type,
                               static_cast<underlying_type>(npos)>());
}

template <typename T, T::tag_type tag> struct tagged_union_variant {
  using type = T::template variant_type<tag>;
};

template <typename T, T::tag_type tag>
using tagged_union_variant_t = tagged_union_variant<T, tag>::type;

template <auto tag_npos, typename... T> class tagged_union {
  using underlying_type = std::underlying_type_t<decltype(tag_npos)>;

  constexpr static bool nothrow_destructible =
      (... and std::is_nothrow_destructible_v<T>);
  constexpr static bool nothrow_copy_constructible =
      (... and std::is_nothrow_copy_constructible_v<T>);
  constexpr static bool nothrow_move_constructible =
      (... and std::is_nothrow_move_constructible_v<T>);
  constexpr static bool nothrow_copy_assignable =
      nothrow_copy_constructible and nothrow_destructible and
      (... and std::is_nothrow_copy_assignable_v<T>);
  constexpr static bool nothrow_move_assignable =
      nothrow_move_constructible and nothrow_destructible and
      (... and std::is_nothrow_move_assignable_v<T>);

public:
  using tag_type = decltype(tag_npos);
  static_assert(std::is_enum_v<tag_type>,
                "the tag of a tagged_union must be an enum");

  constexpr static tag_type npos = tag_npos;
  static_assert(static_cast<underlying_type>(npos) <= sizeof...(T),
                "the tag enum of this tagged_union specifies more variants "
                "than the tagged_union has template arguments");
  static_assert(static_cast<underlying_type>(npos) >= sizeof...(T),
                "the tag enum of this tagged_union specifies less variants "
                "than the tagged_union has template arguments");

  template <tag_type t>
    requires(static_cast<underlying_type>(t) <
             static_cast<underlying_type>(npos))
  using variant_type =
      tagged_union_impl::pack_index_t<static_cast<underlying_type>(t), T...>;

  constexpr tagged_union() : active_tag(npos) {}

  template <typename... Args> tagged_union(tag_type tag, Args &&...args);

  template <tag_type t> tagged_union(tag_constant<t>, auto &&...args);

  template <tag_type t, typename... Args>
  static tagged_union make(Args &&...args);

  constexpr bool empty() const noexcept { return active_tag == npos; }
  constexpr explicit operator bool() const noexcept {
    return active_tag != npos;
  }

  constexpr tag_type tag() const noexcept { return active_tag; }
  constexpr operator tag_type() const noexcept { return active_tag; }

  template <tag_type t> variant_type<t> const &get() const &;
  template <tag_type t> variant_type<t> &get() &;
  template <tag_type t> variant_type<t> &&get() &&;

  template <tag_type t> variant_type<t> const *get_if() const noexcept;
  template <tag_type t> variant_type<t> *get_if() noexcept;

  void reset() & noexcept(nothrow_destructible);
  template <tag_type t> variant_type<t> &emplace(auto &&...args) &;

  template <typename V> auto visit(V &&v) const &;
  template <typename V> auto visit(V &&v) &;
  template <typename V> auto visit(V &&v) &&;
  template <typename R, typename V> R visit(V &&v) const &;
  template <typename R, typename V> R visit(V &&v) &;
  template <typename R, typename V> R visit(V &&v) &&;

  // conditionally trivial operations
  tagged_union(tagged_union const &) noexcept
    requires(... and std::is_trivially_copy_constructible_v<T>)
  = default;
  tagged_union(tagged_union &&) noexcept
    requires(... and std::is_trivially_move_constructible_v<T>)
  = default;
  tagged_union &operator=(tagged_union const &) noexcept
    requires(... and std::is_trivially_copy_assignable_v<T>)
  = default;
  tagged_union &operator=(tagged_union &&) noexcept
    requires(... and std::is_trivially_move_assignable_v<T>)
  = default;
  ~tagged_union() noexcept
    requires(... and std::is_trivially_destructible_v<T>)
  = default;

  // non-trivial fallback operations
  tagged_union(tagged_union const &) noexcept(nothrow_copy_constructible);
  tagged_union(tagged_union &&) noexcept(nothrow_move_constructible);
  tagged_union &
  operator=(tagged_union const &) noexcept(nothrow_copy_assignable);
  tagged_union &operator=(tagged_union &&) noexcept(nothrow_move_assignable);
  ~tagged_union() noexcept(nothrow_destructible);

  friend bool operator==(tagged_union const &lhs, tagged_union const &rhs)
    requires(... and std::equality_comparable<T>)
  {
    if (lhs.active_tag == rhs.active_tag) {
      constexpr static auto vtable = {
          +[](std::byte const *lhs, std::byte const *rhs) {
            return *std::launder(reinterpret_cast<T const *>(lhs)) ==
                   *std::launder(reinterpret_cast<T const *>(rhs));
          }...};

      if (auto const index = static_cast<underlying_type>(lhs.active_tag);
          lhs.active_tag != npos)
        return std::data(vtable)[index](lhs.storage, rhs.storage);
    }

    return false;
  }

  friend auto operator<=>(tagged_union const &lhs, tagged_union const &rhs) {
    using comparison_category = std::common_comparison_category_t<
        std::compare_three_way_result_t<T>...>;
    if (lhs.active_tag == rhs.active_tag) {
      constexpr static auto vtable = {
          +[](std::byte const *lhs,
              std::byte const *rhs) -> comparison_category {
            return *std::launder(reinterpret_cast<T const *>(lhs)) <=>
                   *std::launder(reinterpret_cast<T const *>(rhs));
          }...};

      if (auto const index = static_cast<underlying_type>(lhs.active_tag);
          lhs.active_tag != npos)
        std::data(vtable)[index](lhs.storage, rhs.storage);
    }

    return comparison_category{static_cast<underlying_type>(lhs.active_tag) <=>
                               static_cast<underlying_type>(rhs.active_tag)};
  }

  friend void swap(tagged_union &lhs, tagged_union &rhs) //
      noexcept(nothrow_move_constructible and nothrow_destructible and
               (... and std::is_nothrow_swappable_v<T>))
    requires(... and
             (std::is_swappable_v<T> and std::is_move_constructible_v<T>))
  {
    // call into vtable-based swap if the tags are the same
    if (lhs.active_tag == rhs.active_tag) {
      constexpr static auto vtable = {+[](std::byte *lhs, std::byte *rhs) {
        std::swap(*std::launder(reinterpret_cast<T *>(lhs)),
                  *std::launder(reinterpret_cast<T *>(rhs)));
      }...};

      if (auto const index = static_cast<underlying_type>(lhs.active_tag);
          lhs.active_tag != npos)
        std::data(vtable)[index](lhs.storage, rhs.storage);
    }

    // fallback to simple swap by three moves where lhs is moved twice
    auto temp = std::move(lhs);
    lhs = std::move(rhs);
    rhs = std::move(temp);
  }

private:
  constexpr static tagged_union_impl::copy_fn copy_constructor_vtable[] = {
      +[](std::byte *self, std::byte const *other) {
        ::new (self) T(*std::launder(reinterpret_cast<T const *>(other)));
      }...};

  constexpr static tagged_union_impl::move_fn move_constructor_vtable[] = {
      +[](std::byte *self, std::byte *other) {
        ::new (self) T(std::move(*std::launder(reinterpret_cast<T *>(other))));
      }...};

  constexpr static tagged_union_impl::copy_fn copy_assignment_vtable[] = {
      +[](std::byte *self, std::byte const *other) {
        *std::launder(reinterpret_cast<T *>(self)) =
            *std::launder(reinterpret_cast<T const *>(other));
      }...};

  constexpr static tagged_union_impl::move_fn move_assignment_vtable[] = {
      +[](std::byte *self, std::byte *other) {
        *std::launder(reinterpret_cast<T *>(self)) =
            std::move(*std::launder(reinterpret_cast<T *>(other)));
      }...};

  constexpr static tagged_union_impl::destruct_fn destructor_vtable[] = {
      +[](std::byte *self) {
        std::launder(reinterpret_cast<T *>(self))->~T();
      }...};

  alignas(T...) std::byte storage[std::max({sizeof(T)...})];
  tag_type active_tag;
};

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::get() const & -> variant_type<t> const & {
  if (t != active_tag)
    throw bad_tagged_union_access{};

  return *std::launder(reinterpret_cast<variant_type<t> const *>(storage));
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::get() & -> variant_type<t> & {
  if (t != active_tag)
    throw bad_tagged_union_access{};

  return *std::launder(reinterpret_cast<variant_type<t> *>(storage));
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::get() && -> variant_type<t> && {
  if (t != active_tag)
    throw bad_tagged_union_access{};

  return std::move(*std::launder(reinterpret_cast<variant_type<t> *>(storage)));
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::get_if() const noexcept
    -> variant_type<t> const * {
  if (t == active_tag)
    return std::launder(reinterpret_cast<variant_type<t> const *>(storage));

  return nullptr;
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::get_if() noexcept -> variant_type<t> * {
  if (t == active_tag)
    return std::launder(reinterpret_cast<variant_type<t> *>(storage));

  return nullptr;
}

template <auto tag_npos, typename... T>
void tagged_union<tag_npos, T...>::reset() & noexcept(nothrow_destructible) {
  if (auto const old_tag = std::exchange(active_tag, npos); old_tag != npos)
    tagged_union_impl::destruct(destructor_vtable,
                                static_cast<underlying_type>(old_tag), storage);
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
auto tagged_union<tag_npos, T...>::emplace(
    auto &&...args) & -> variant_type<t> & {
  reset();

  auto *const value =
      ::new (storage) variant_type<t>(std::forward<decltype(args)>(args)...);
  active_tag = t;
  return *value;
}

template <auto tag_npos, typename... T>
template <typename V>
auto tagged_union<tag_npos, T...>::visit(V &&visitor) const & {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor, std::byte const *storage) {
      auto &value = *std::launder(reinterpret_cast<T const *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T const &>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{}, value);
      else
        return std::invoke(std::forward<V>(visitor), value);
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename V>
auto tagged_union<tag_npos, T...>::visit(V &&visitor) & {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor, std::byte *storage) {
      auto &value = *std::launder(reinterpret_cast<T *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T &>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{}, value);
      else
        return std::invoke(std::forward<V>(visitor), value);
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename V>
auto tagged_union<tag_npos, T...>::visit(V &&visitor) && {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor, std::byte *storage) {
      auto &value = *std::launder(reinterpret_cast<T *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T &&>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{},
                           std::move(value));
      else
        return std::invoke(std::forward<V>(visitor), std::move(value));
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename R, typename V>
R tagged_union<tag_npos, T...>::visit(V &&visitor) const & {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor,
                                        std::byte const *storage) -> R {
      auto &value = *std::launder(reinterpret_cast<T const *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T const &>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{}, value);
      else
        return std::invoke(std::forward<V>(visitor), value);
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename R, typename V>
R tagged_union<tag_npos, T...>::visit(V &&visitor) & {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor, std::byte *storage) -> R {
      auto &value = *std::launder(reinterpret_cast<T *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T &>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{}, value);
      else
        return std::invoke(std::forward<V>(visitor), value);
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename R, typename V>
R tagged_union<tag_npos, T...>::visit(V &&visitor) && {
  if (active_tag == npos) {
    if constexpr (std::is_invocable_v<V &&>)
      return std::invoke(std::forward<V>(visitor));
    else
      throw bad_tagged_union_access{};
  }

  return [&]<tag_type... t>(tag_sequence<tag_type, t...>) {
    constexpr static auto vtable = {+[](V &&visitor, std::byte *storage) -> R {
      auto &value = *std::launder(reinterpret_cast<T *>(storage));
      if constexpr (std::is_invocable_v<V &&, tag_constant<t> &&, T &&>)
        return std::invoke(std::forward<V>(visitor), tag_constant<t>{},
                           std::move(value));
      else
        return std::invoke(std::forward<V>(visitor), std::move(value));
    }...};

    auto const index = static_cast<underlying_type>(active_tag);
    return std::data(vtable)[index](std::forward<V>(visitor), storage);
  }(make_tag_sequence<npos>());
}

template <auto tag_npos, typename... T>
template <typename... Args>
inline tagged_union<tag_npos, T...>::tagged_union(tag_type tag, Args &&...args)
    : active_tag(tag) {
  if (static_cast<underlying_type>(tag) >= static_cast<underlying_type>(npos)) {
    throw std::out_of_range{"tried to construct tagged_union from invalid tag"};
  }

  auto const construct = [&]<tag_type t>(tag_constant<t>) -> bool {
    if (active_tag == t) {
      ::new (storage) variant_type<t>{std::forward<Args>(args)...};
      return true;
    }

    return false;
  };

  [&]<underlying_type... index>(
      std::integer_sequence<underlying_type, index...>) {
    (... or construct(in_place_tag<tag_type{index}>));
  }(std::make_integer_sequence<underlying_type,
                               static_cast<underlying_type>(npos)>());
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t>
inline tagged_union<tag_npos, T...>::tagged_union(tag_constant<t>,
                                                  auto &&...args)
    : active_tag(t) {
  if constexpr (t != npos)
    ::new (storage) variant_type<t>{std::forward<decltype(args)>(args)...};
}

template <auto tag_npos, typename... T>
template <decltype(tag_npos) t, typename... Args>
inline auto tagged_union<tag_npos, T...>::make(Args &&...args) -> tagged_union {
  return {in_place_tag<t>, std::forward<Args>(args)...};
}

template <auto tag_npos, typename... T>
inline tagged_union<tag_npos, T...>::tagged_union(
    tagged_union const &other) noexcept(nothrow_copy_constructible)
    : active_tag(other.active_tag) {
  if (active_tag != npos)
    tagged_union_impl::copy(copy_constructor_vtable,
                            static_cast<underlying_type>(active_tag), storage,
                            other.storage);
}

template <auto tag_npos, typename... T>
inline tagged_union<tag_npos, T...>::tagged_union(
    tagged_union &&other) noexcept(nothrow_move_constructible)
    : active_tag(other.active_tag) {
  if (active_tag != npos)
    tagged_union_impl::move(move_constructor_vtable,
                            static_cast<underlying_type>(active_tag), storage,
                            other.storage);
}

template <auto tag_npos, typename... T>
inline auto tagged_union<tag_npos, T...>::operator=(
    tagged_union const &other) noexcept(nothrow_copy_assignable)
    -> tagged_union & {
  if (other.active_tag == active_tag) {
    if (active_tag != npos)
      tagged_union_impl::copy(copy_assignment_vtable,
                              static_cast<underlying_type>(active_tag), storage,
                              other.storage);
  } else {
    reset();

    if (auto const new_tag = other.active_tag; new_tag != npos) {
      tagged_union_impl::copy(copy_constructor_vtable,
                              static_cast<underlying_type>(new_tag), storage,
                              other.storage);
      active_tag = new_tag;
    }
  }

  return *this;
}

template <auto tag_npos, typename... T>
inline auto tagged_union<tag_npos, T...>::operator=(
    tagged_union &&other) noexcept(nothrow_move_assignable) -> tagged_union & {
  if (other.active_tag == active_tag) {
    if (active_tag != npos)
      tagged_union_impl::move(move_assignment_vtable,
                              static_cast<underlying_type>(active_tag), storage,
                              other.storage);
  } else {
    reset();

    if (auto const new_tag = other.active_tag; new_tag != npos) {
      tagged_union_impl::move(move_constructor_vtable,
                              static_cast<underlying_type>(new_tag), storage,
                              other.storage);
      active_tag = new_tag;
    }
  }

  return *this;
}

template <auto tag_npos, typename... T>
inline tagged_union<tag_npos, T...>::~tagged_union() noexcept(
    nothrow_destructible) {
  if (active_tag != npos)
    tagged_union_impl::destruct(
        destructor_vtable, static_cast<underlying_type>(active_tag), storage);
}

} // namespace villas
