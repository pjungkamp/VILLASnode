/* C++ libjansson C API wrapper
 *
 * This wrapper includes...
 *
 * ...an automatically reference counted `jansson::Value` with proper move-sematics.
 * ...a strongly-typed JSON `Object` which is also a proper range of key value pairs.
 * ...a strongly-typed JSON `Array` which is also a proper random access container.
 * ...type-safe packing and unpacking of JSON values into user defined types
 *
 *
 * Using a JSON Array as a range:
 *
 *     auto array = villas::jansson::Array{
 *       villas::jansson::object(),
 *       villas::jansson::string("value")};
 *
 *     for (auto value : array) do_something(value);
 *
 *
 * Using a JSON Object as a range:
 *
 *     auto object = villas::jansson::Object{
 *       {"firstname", villas::jansson::string("foo")},
 *       {"lastname", villas::jansson::string("bar")},
 *       {"age", villas::jansson::integer(10)}};
 *     for (auto [key, value] : object) do_something(key, value);
 *     for (auto key : std::views::keys(object)) do_something(key);
 *     for (auto value : std::views::values(object)) do_something(value);
 *
 *
 * JSON Object packing and unpacking:
 *
 *     std::string_view first, last;
 *     int age;
 *
 *     object.unpack(
 *       villas::jansson::required("firstname", first),
 *       villas::jansson::required("lastname", last),
 *       villas::jansson::optional("age", age));
 *
 *     auto packed = villas::jansson::Object::pack(
 *       villas::jansson::required("firstname", first),
 *       villas::jansson::required("lastname", last),
 *       villas::jansson::optional("age", age));
 *
 *     assert(object == packed);
 *
 *
 * Make any unpackable type from a JSON value:
 *
 *     // let's assume we have a JSON file named "file.json"
 *     auto json = villas::jansson::Value::loadFromFilePath("file.json");
 *
 *     // associative containers
 *     auto map = villas::jansson::make<std::unordered_map<villas::jansson::Value>>(json);
 *
 *     // resizable sequence containers
 *     auto vector = villas::jansson::make<std::vector<std::string>>(json);
 *
 *     // tuple-like fixed size containers
 *     auto array = villas::jansson::make<std::array<int, 5>>(json);
 *     auto tuple = villas::jansson::make<std::tuple<int, std::string, float>>(json);
 *
 *
 * You can make any type packable or unpackable by adding a jsonPack/jsonUnpack function
 * in the namespace where the type is declared which is then found using ADL.
 * The Object::pack and Object::unpack functions make serializing struct-like types easy.
 *
 *     struct User {
 *       std::string name;
 *       int age;
 *       bool is_admin;
 *     };
 *
 *     villas::jansson::Value jsonPack(User const &user) {
 *       return villas::jansson::Object::pack(
 *         villas::jansson::required("name", user.name),
 *         villas::jansson::required("age", user.age),
 *         villas::jansson::optional("is_admin", user.is_admin));
 *     }
 *
 *     static_assert(villas::jansson::packable<User>);
 *
 *     void jsonUnpack(User &user, villas::jansson::Value const &json) {
 *       json.object().unpack(
 *         villas::jansson::required("name", user.name),
 *         villas::jansson::required("age", user.age),
 *         villas::jansson::optional("is_admin", user.is_admin));
 *     }
 *
 *     static_assert(villas::jansson::unpackable<User>);
 *
 *
 * The whole wrapper is implemented using inline header functions. You only need to link libjansson.
 * You should probably move most jsonPack/jsonUnpack implementations to source files to only generate
 * and compile the templated packing/unpacking code once.
 *
 *
 * Author: Philipp Jungkamp <philipp@jungkamp.dev>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <compare>
#include <complex>
#include <concepts>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <source_location>
#include <string_view>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include <villas/non_null.hpp>

extern "C" {
#include <jansson.h>
}

namespace villas::jansson {

using type_t = json_type;
using int_t = json_int_t;
using error_code_t = enum json_error_code;

std::string_view typeToString(type_t type);

class bad_access final : public std::exception {
public:
  bad_access(type_t expected, type_t found)
      : bad_access(typeToString(expected), typeToString(found)) {}

  bad_access(std::string_view expected, std::string_view found)
      : message(fmt::format("expected JSON {} but found {}", expected, found)) {
  }

  char const *what() const noexcept override { return message.c_str(); }

private:
  std::string message;
};

class internal_error final : public std::exception {
public:
  internal_error(std::source_location src = std::source_location::current())
      : message(fmt::format("internal error in {}", src.function_name())) {}

  static void
  check(int ret, std::source_location src = std::source_location::current()) {
    if (ret == -1)
      throw internal_error{src};
  }

  template <typename T>
  static non_null<T>
  check(T *ptr, std::source_location src = std::source_location::current()) {
    if (ptr == nullptr)
      throw internal_error{src};
    else
      return non_null{*ptr};
  }

  char const *what() const noexcept override { return message.c_str(); }

private:
  std::string message;
};

class parse_error final : public std::exception {
public:
  parse_error(json_error_t const &err) : error(err) {}
  json_error_t const &operator*() const { return error; }
  json_error_t const *operator->() const { return &error; }
  char const *what() const noexcept override { return error.text; }

private:
  json_error_t error;
};

class Value;
class Object;
class Array;

Value object();
Value array();
Value string(std::string_view);
Value integer(int_t);
Value real(double);
Value boolean(bool);
Value null();

class Value final {
public:
  Value() noexcept;

  Value(Value const &other) noexcept;
  Value(Value &&other) noexcept;
  Value &operator=(Value const &other) noexcept;
  Value &operator=(Value &&other) noexcept;
  ~Value() noexcept;

  Value copy();
  Value deepCopy();

  static Value fromString(std::string_view string, std::size_t flags = 0);
  std::string toString(std::size_t flags = 0) const;

  static Value loadFromFileStream(std::FILE *file, std::size_t flags = 0);
  void dumpToFileStream(std::FILE *file, std::size_t flags = 0) const;

  static Value loadFromFileDescriptor(int fd, std::size_t flags = 0);
  void dumpToFileDescriptor(int fd, std::size_t flags = 0) const;

  static Value loadFromFilePath(std::filesystem::path const &path,
                                std::size_t flags = 0);
  void dumpToFilePath(std::filesystem::path const &path,
                      std::size_t flags = 0) const;

  template <typename Callback>
  static Value loadWithCallback(Callback cb, std::size_t flags = 0);
  template <typename Callback>
  void dumpWithCallback(Callback cb, std::size_t flags = 0) const;

  static Value take(json_t *owned_raw);
  static Value take(non_null<json_t> owned_ptr) noexcept;
  static Value borrow(json_t *borrowed_raw);
  static Value borrow(non_null<json_t> borrowed_ptr) noexcept;

  json_t *takeRawPointer() noexcept;
  json_t *borrowRawPointer() const noexcept;

  type_t type() const noexcept;
  void expect(type_t expected) const;
  bool isObject() const noexcept;
  bool isArray() const noexcept;
  bool isString() const noexcept;
  bool isInteger() const noexcept;
  bool isReal() const noexcept;
  bool isNumber() const noexcept;
  bool isTrue() const noexcept;
  bool isFalse() const noexcept;
  bool isBoolean() const noexcept;
  bool isNull() const noexcept;

  Object object() const;
  Array array() const;
  std::string_view string() const;
  int_t integer() const;
  double real() const;
  double number() const;
  bool boolean() const;

  Value &operator=(std::string_view s);

  template <typename I>
    requires std::integral<I> and requires(I i) { int_t{i}; }
  Value &operator=(I i);

  template <typename F>
    requires std::floating_point<F> and requires(F f) { double{f}; }
  Value &operator=(F d);

  template <typename B>
    requires std::same_as<B, bool>
  Value &operator=(B b);

  friend bool operator==(Value const &lhs, Value const &rhs);

private:
  Value(non_null<json_t> owned_ptr) noexcept;
  non_null<json_t> ptr;
};

class Object final {
public:
  using Entry = std::pair<std::string_view, Value>;

  Object() = default;

  explicit Object(Value const &value);
  explicit Object(Value &&value);

  Object(std::initializer_list<Entry> list);

  Value const &value() const & noexcept;
  Value &&value() && noexcept;
  operator Value const &() const & noexcept;
  operator Value &&() && noexcept;

  std::size_t size() const noexcept;

  std::optional<Value> get(std::string_view key) const noexcept;
  Value operator[](std::string_view key) const noexcept;

  void set(std::string_view key, Value const &val);
  void set(std::string_view key, Value &&val);
  void del(std::string_view key);
  void clear();
  void update(Object const &other);
  void update(Object &&other);
  void updateExisting(Object const &other);
  void updateExisting(Object &&other);
  void updateMissing(Object const &other);
  void updateMissing(Object &&other);

  using Sentinel = std::default_sentinel_t;
  class Iterator {
  public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Entry;

    Iterator() = default;

    explicit Iterator(Object const &obj) noexcept;

    value_type operator*() const noexcept;
    Iterator &operator++() noexcept;
    Iterator operator++(int) noexcept;

    friend bool operator==(Iterator const &,
                           Iterator const &) noexcept = default;
    friend bool operator==(Iterator const &iterator, Sentinel) noexcept;

  private:
    json_t *obj = nullptr;
    void *iter = nullptr;
  };

  Iterator begin() const noexcept;
  Sentinel end() const noexcept;

  template <typename T>
    requires std::is_reference_v<T>
  struct Binding {
    std::string_view name;
    T value;
    bool required;
  };

  // check if a jansson::Value is considered empty for the purposes of packing or unpacking
  // an optional value binding.
  //
  // a value is considered empty when it is one of the following:
  // - an empty object (value.isObject() and value.object().size() == 0)
  // - an empty array  (value.isArray() and value.array().size() == 0)
  // - an empty string (value.isString() and value.string().size() == 0)
  // - a null value    (value.isNull())
  static bool optionalValueIsEmpty(Value const &value) noexcept;

  template <typename... T> static Object pack(Binding<T>... binding);
  template <typename... T> void unpack(Binding<T>... binding) const;

private:
  Value inner = object();
};

// create an Object::Binding for Object::pack and Object::unpack.
template <typename T>
inline auto binding(std::string_view key, T &&value, bool required) {
  if constexpr (std::is_rvalue_reference_v<T &&>)
    return Object::Binding<T const &>{key, value, required};
  else
    return Object::Binding<T &>{key, value, required};
}

// create a *required* Object::Binding for Object::pack and Object::unpack.
//
// this is an alias for jansson::bind with required = true
template <typename T> inline auto required(std::string_view key, T &&value) {
  return binding(key, std::forward<T>(value), true);
}

// create an *optional* Object::Binding for Object::pack and Object::unpack.
//
// this is an alias for jansson::bind with required = false
template <typename T> inline auto optional(std::string_view key, T &&value) {
  return binding(key, std::forward<T>(value), false);
}

class Array {
public:
  Array() = default;

  explicit Array(Value const &value);
  explicit Array(Value &&value);
  Array(std::initializer_list<Value> list);

  Value const &value() const & noexcept;
  Value &&value() && noexcept;
  operator Value const &() const & noexcept;
  operator Value() && noexcept;

  std::size_t size() const noexcept;
  std::optional<Value> get(std::size_t index) const noexcept;
  Value operator[](std::size_t index) const noexcept;

  void set(std::size_t index, Value const &value);
  void set(std::size_t index, Value &&value);
  void append(Value const &value);
  void append(Value &&value);
  void insert(std::size_t index, Value const &value);
  void insert(std::size_t index, Value &&value);
  void remove(std::size_t index);
  void clear(std::size_t index);
  void extend(Array const &other);

  class Iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = Value;

    Iterator() = default;

    explicit Iterator(Array const &arr, std::size_t start) noexcept;

    Value operator*() const noexcept;
    Value operator[](difference_type n) const noexcept;
    Iterator &operator++() noexcept;
    Iterator operator++(int) noexcept;
    Iterator &operator--() noexcept;
    Iterator operator--(int) noexcept;
    Iterator &operator+=(difference_type n) noexcept;
    Iterator &operator-=(difference_type n) noexcept;

    friend Iterator operator+(Iterator iter, difference_type n) noexcept;
    friend Iterator operator+(difference_type n, Iterator iter) noexcept;
    friend Iterator operator-(Iterator iter, difference_type n) noexcept;
    friend difference_type operator-(Iterator const &lhs,
                                     Iterator const &rhs) noexcept;
    friend std::partial_ordering operator<=>(Iterator const &lhs,
                                             Iterator const &rhs) noexcept;
    friend bool operator==(Iterator const &lhs, Iterator const &rhs) noexcept;

  private:
    json_t *arr;
    std::size_t index;
  };

  Iterator begin() const noexcept;
  Iterator end() const noexcept;

private:
  Value inner = array();
};

void jsonUnpack(Value &v, Value const &value);
Value jsonPack(Value const &value);

void jsonUnpack(Object &obj, Value const &value);
Value jsonPack(Object const &obj);

void jsonUnpack(Array &arr, Value const &value);
Value jsonPack(Array const &arr);

void jsonUnpack(std::string_view &sv, Value const &value);
Value jsonPack(std::string_view const &sv);

void jsonUnpack(std::string &s, Value const &value);
Value jsonPack(std::string const &s);

void jsonUnpack(bool &b, Value const &value);
Value jsonPack(bool const &b);

template <std::integral T> void jsonUnpack(T &i, Value const &value);
template <std::integral T> Value jsonPack(T const &i);

template <std::floating_point T> void jsonUnpack(T &f, Value const &value);
template <std::floating_point T> Value jsonPack(T const &f);

template <std::floating_point T>
void jsonUnpack(std::complex<T> &, Value const &);
template <std::floating_point T> Value jsonPack(std::complex<T> const &);

template <typename T> void jsonUnpack(std::optional<T> &, Value const &);
template <typename T> Value jsonPack(std::optional<T> const &);

// resizable standard `SequenceContainer` types (std::list, std::vector)
template <typename T>
concept vector_like =
    requires(T container, std::size_t size, std::size_t index) {
      typename T::value_type;
      container.resize(size);
      { container[index] } -> std::same_as<typename T::value_type &>;
    };

template <vector_like T> void jsonUnpack(T &, Value const &);
template <vector_like T> Value jsonPack(T const &);

// concept for types implementing the tuple protocol (std::tuple, std::array)
template <typename T>
concept tuple_like = requires(T &tuple) {
  std::tuple_size<T>::value;
  []<std::size_t... index>(std::index_sequence<index...>) {
    static_assert(
        (... and std::same_as<typename std::tuple_element_t<index, T> &,
                              decltype(std::get<index>(tuple))>));
  }(std::make_index_sequence<std::tuple_size_v<T>>());
};

template <tuple_like T> void jsonUnpack(T &, Value const &);
template <tuple_like T> Value jsonPack(T const &);

// standard `AssociativeContainer` types (std::map, std::unordered_map)
template <typename T>
concept map_like = requires(T container, std::string_view key) {
  typename T::key_type;
  typename T::mapped_type;
  container.clear();
  {
    container[typename T::key_type{key}]
  } -> std::same_as<typename T::mapped_type &>;
};

template <map_like T> void jsonUnpack(T &, Value const &);
template <map_like T> Value jsonPack(T const &);

template <typename T>
concept unpackable = requires(T &t, Value const &v) {
  { jsonUnpack(t, v) } -> std::same_as<void>;
};

template <typename T>
concept packable = requires(T const &t) {
  { jsonPack(t) } -> std::same_as<Value>;
};

struct unpack_t {
  template <unpackable T> void operator()(T &t, Value const &value) const {
    jsonUnpack(t, value);
  }
};
constexpr static unpack_t unpack{};

template <unpackable T>
  requires std::is_default_constructible_v<T>
struct make_t {
  T operator()(Value const &value) const {
    auto t = T{};
    jsonUnpack(t, value);
    return t;
  }
};
template <unpackable T> constexpr static make_t<T> make{};

struct pack_t {
  template <packable T> Value operator()(T const &t) const {
    return jsonPack(t);
  }
};
constexpr static pack_t pack{};

} // namespace villas::jansson

#include "villas/jansson.inline.hpp"
