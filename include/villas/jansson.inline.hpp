/* inline implementation for the C++ libjansson C API wrapper
 *
 * Author: Philipp Jungkamp <philipp@jungkamp.dev>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <stdexcept>
#include <utility>

#include <villas/jansson.hpp>

namespace villas::jansson {

// get a static string representation of the given `type_t`.
inline std::string_view typeToString(type_t type) {
  switch (type) {
    using namespace std::string_view_literals;
  case JSON_OBJECT:
    return "object"sv;
  case JSON_ARRAY:
    return "array"sv;
  case JSON_STRING:
    return "string"sv;
  case JSON_REAL:
    return "real"sv;
  case JSON_INTEGER:
    return "integer"sv;
  case JSON_TRUE:
    return "true"sv;
  case JSON_FALSE:
    return "false"sv;
  case JSON_NULL:
    return "null"sv;
  default:
    return "invalid"sv;
  }
}

inline Value object() { return Value::take(json_object()); }

inline Value array() { return Value::take(json_array()); }

inline Value string(std::string_view s) {
  return Value::take(json_stringn(s.data(), s.size()));
}

inline Value integer(int_t i) { return Value::take(json_integer(i)); }

inline Value real(double d) { return Value::take(json_real(d)); }

inline Value boolean(bool value) { return Value::take(*json_boolean(value)); }

inline Value null() { return Value::take(*json_null()); }

inline Value::Value() noexcept : ptr(*json_null()) {}

inline Value::Value(Value const &other) noexcept
    : ptr(*json_incref(other.ptr.get())) {}

inline Value::Value(Value &&other) noexcept
    : ptr(std::exchange(other.ptr, *json_null())) {}

inline Value &Value::operator=(Value const &other) noexcept {
  auto new_ptr = non_null{*json_incref(other.ptr.get())};
  auto old_ptr = std::exchange(ptr, new_ptr);
  json_decref(old_ptr.get());
  return *this;
}

inline Value &Value::operator=(Value &&other) noexcept {
  std::swap(ptr, other.ptr);
  return *this;
}

inline Value::~Value() noexcept { json_decref(ptr.get()); }

inline Value Value::copy() { return Value::take(json_copy(ptr.get())); }

inline Value Value::deepCopy() {
  return Value::take(json_deep_copy(ptr.get()));
}

inline Value Value::fromString(std::string_view string, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadb(string.data(), string.size(), flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

inline std::string Value::toString(size_t flags) const {
  return internal_error::check(json_dumps(ptr.get(), flags)).get();
}

inline Value Value::loadFromFileStream(std::FILE *file, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadf(file, flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

inline void Value::dumpToFileStream(std::FILE *file, size_t flags) const {
  internal_error::check(json_dumpf(ptr.get(), file, flags));
}

inline Value Value::loadFromFileDescriptor(int fd, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadfd(fd, flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

inline void Value::dumpToFileDescriptor(int fd, size_t flags) const {
  internal_error::check(json_dumpfd(ptr.get(), fd, flags));
}

inline Value Value::loadFromFilePath(std::filesystem::path const &path,
                                     std::size_t flags) {
  json_error_t err;
  if (auto raw = json_load_file(path.c_str(), flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

inline void Value::dumpToFilePath(std::filesystem::path const &path,
                                  size_t flags) const {
  internal_error::check(json_dump_file(ptr.get(), path.c_str(), flags));
}

template <typename Callback>
inline Value Value::loadWithCallback(Callback cb, std::size_t flags) {
  struct Data {
    Callback callback;
    std::exception_ptr exception;
  };

  auto trampoline =
      +[](void *buffer, std::size_t buflen, void *opaque) -> std::size_t {
    auto data = reinterpret_cast<Data *>(opaque);
    try {
      return data->callback(buffer, buflen);
    } catch (...) {
      data->exception = std::current_exception();
      return (std::size_t)-1;
    }
  };

  json_error_t err;
  Data data = {.callback = cb, .exception = nullptr};
  auto opaque = reinterpret_cast<void *>(&data);
  if (auto raw = json_load_callback(trampoline, opaque, flags, &err))
    return Value{raw};
  else if (data.exception != nullptr)
    std::rethrow_exception(data.exception);
  else
    throw parse_error{err};
}

template <typename Callback>
inline void Value::dumpWithCallback(Callback cb, size_t flags) const {
  struct Data {
    Callback callback;
    std::exception_ptr exception;
  };

  auto trampoline = +[](void *buffer, std::size_t buflen, void *opaque) -> int {
    auto data = reinterpret_cast<Data *>(opaque);
    try {
      data->callback(buffer, buflen);
      return 0;
    } catch (...) {
      data->exception = std::current_exception();
      return -1;
    }
  };

  Data data = {.callback = cb, .exception = nullptr};
  auto opaque = reinterpret_cast<void *>(&data);
  if (auto ret = json_dump_callback(ptr.get(), trampoline, opaque, flags);
      data.exception != nullptr)
    std::rethrow_exception(data.exception);
  else
    internal_error::check(ret);
}

inline Value Value::take(json_t *owned_raw) {
  return Value{non_null{owned_raw}};
}

inline Value Value::take(non_null<json_t> owned_ptr) noexcept {
  return Value{owned_ptr};
}

inline Value Value::borrow(json_t *borrowed_raw) {
  return Value{non_null{json_incref(borrowed_raw)}};
}

inline Value Value::borrow(non_null<json_t> borrowed_ptr) noexcept {
  json_incref(borrowed_ptr.get());
  return Value{borrowed_ptr};
}

inline json_t *Value::takeRawPointer() noexcept {
  return std::exchange(ptr, *json_null()).get();
}

inline json_t *Value::borrowRawPointer() const noexcept { return ptr.get(); }

inline type_t Value::type() const noexcept { return json_typeof(ptr.get()); }

inline bool Value::isObject() const noexcept {
  return json_is_object(ptr.get());
}

inline bool Value::isArray() const noexcept { return json_is_array(ptr.get()); }

inline bool Value::isString() const noexcept {
  return json_is_string(ptr.get());
}

inline bool Value::isInteger() const noexcept {
  return json_is_integer(ptr.get());
}

inline bool Value::isReal() const noexcept { return json_is_real(ptr.get()); }

inline bool Value::isNumber() const noexcept {
  return json_is_number(ptr.get());
}

inline bool Value::isTrue() const noexcept { return json_is_true(ptr.get()); }

inline bool Value::isFalse() const noexcept { return json_is_false(ptr.get()); }

inline bool Value::isBoolean() const noexcept {
  return json_is_boolean(ptr.get());
}

inline bool Value::isNull() const noexcept { return json_is_null(ptr.get()); }

inline void Value::expect(type_t expected) const {
  auto found = json_typeof(ptr.get());
  if (found != expected)
    throw bad_access{expected, found};
};

inline Object Value::object() const { return Object(*this); }

inline Array Value::array() const { return Array(*this); }

inline std::string_view Value::string() const {
  expect(JSON_STRING);
  return {json_string_value(ptr.get()), json_string_length(ptr.get())};
}

inline Value &Value::operator=(std::string_view s) {
  if (json_is_string(ptr.get())) {
    internal_error::check(json_string_setn(ptr.get(), s.data(), s.size()));
    return *this;
  }
  return *this = jansson::string(s);
}

inline int_t Value::integer() const {
  expect(JSON_INTEGER);
  return json_integer_value(ptr.get());
}

template <typename I>
  requires std::integral<I> and requires(I i) { int_t{i}; }
inline Value &Value::operator=(I i) {
  if (json_is_integer(ptr.get())) {
    json_integer_set(ptr.get(), static_cast<int_t>(i));
    return *this;
  }
  return *this = jansson::integer(i);
}

inline double Value::real() const {
  expect(JSON_REAL);
  return json_real_value(ptr.get());
}

template <typename F>
  requires std::floating_point<F> and requires(F f) { double{f}; }
inline Value &Value::operator=(F d) {
  if (json_is_real(ptr.get())) {
    json_real_set(ptr.get(), d);
    return *this;
  }
  return *this = jansson::real(d);
}

inline double Value::number() const {
  expect(JSON_REAL);
  return json_number_value(ptr.get());
}

inline bool Value::boolean() const {
  if (not json_is_boolean(ptr.get()))
    throw bad_access{"boolean", typeToString(json_typeof(ptr.get()))};

  return json_boolean(ptr.get());
}

template <typename B>
  requires std::same_as<B, bool>
inline Value &Value::operator=(B b) {
  return *this = jansson::boolean(b);
}

inline bool operator==(Value const &lhs, Value const &rhs) {
  return json_equal(lhs.ptr.get(), rhs.ptr.get());
}

inline Value::Value(non_null<json_t> owned_ptr) noexcept : ptr(owned_ptr) {}

inline Object::Object(Value const &value) : inner(value) {
  value.expect(JSON_OBJECT);
}

inline Object::Object(Value &&value) : inner(std::move(value)) {
  value.expect(JSON_OBJECT);
}

inline Object::Object(std::initializer_list<Entry> list) {
  for (auto const &[key, value] : list)
    set(key, value);
}

inline Value const &Object::value() const & noexcept { return inner; }

inline Value &&Object::value() && noexcept { return std::move(inner); }

inline Object::operator Value const &() const & noexcept { return inner; }

inline Object::operator Value &&() && noexcept { return std::move(inner); }

inline std::size_t Object::size() const noexcept {
  return json_object_size(inner.borrowRawPointer());
}

inline std::optional<Value> Object::get(std::string_view key) const noexcept {
  auto raw = json_object_getn(inner.borrowRawPointer(), key.data(), key.size());
  if (raw == nullptr)
    return std::nullopt;

  return Value::borrow(*raw);
}

inline Value Object::operator[](std::string_view key) const noexcept {
  return get(key).value_or(jansson::null());
}

inline void Object::set(std::string_view key, Value const &val) {
  internal_error::check(json_object_setn(inner.borrowRawPointer(), key.data(),
                                         key.size(), val.borrowRawPointer()));
}

inline void Object::set(std::string_view key, Value &&val) {
  internal_error::check(json_object_setn_new(
      inner.borrowRawPointer(), key.data(), key.size(), val.takeRawPointer()));
}

inline void Object::del(std::string_view key) {
  json_object_deln(inner.borrowRawPointer(), key.data(), key.size());
};

inline void Object::clear() { json_object_clear(inner.borrowRawPointer()); }

inline void Object::update(Object const &other) {
  internal_error::check(json_object_update(inner.borrowRawPointer(),
                                           other.inner.borrowRawPointer()));
}

inline void Object::update(Object &&other) {
  internal_error::check(json_object_update_new(inner.borrowRawPointer(),
                                               other.inner.takeRawPointer()));
}

inline void Object::updateExisting(Object const &other) {
  internal_error::check(json_object_update_existing(
      inner.borrowRawPointer(), other.inner.borrowRawPointer()));
}

inline void Object::updateExisting(Object &&other) {
  internal_error::check(json_object_update_existing_new(
      inner.borrowRawPointer(), other.inner.takeRawPointer()));
}

inline void Object::updateMissing(Object const &other) {
  internal_error::check(json_object_update_missing(
      inner.borrowRawPointer(), other.inner.borrowRawPointer()));
}

inline void Object::updateMissing(Object &&other) {
  internal_error::check(json_object_update_missing_new(
      inner.borrowRawPointer(), other.inner.takeRawPointer()));
}

inline Object::Iterator::Iterator(Object const &obj) noexcept
    : obj(obj.inner.borrowRawPointer()),
      iter(json_object_iter(obj.inner.borrowRawPointer())) {}

inline Object::Iterator::value_type
Object::Iterator::operator*() const noexcept {
  return {std::string_view{json_object_iter_key(iter),
                           json_object_iter_key_len(iter)},
          Value::borrow(*json_object_iter_value(iter))};
}

inline Object::Iterator &Object::Iterator::operator++() noexcept {
  iter = json_object_iter_next(obj, iter);
  return *this;
}

inline Object::Iterator Object::Iterator::operator++(int) noexcept {
  auto old = *this;
  ++*this;
  return old;
}

inline bool operator==(Object::Iterator const &iterator,
                       Object::Sentinel) noexcept {
  return iterator.iter == nullptr;
}

inline Object::Iterator Object::begin() const noexcept {
  return Iterator{*this};
}
inline Object::Sentinel Object::end() const noexcept {
  return std::default_sentinel;
}

template <typename... T>
inline Object Object::pack(Object::Binding<T>... bindings) {
  auto object = Object{};

  (..., [&](auto &binding) {
    auto value = jansson::pack(binding.value);
    if (binding.required or not value.isNull())
      object.set(binding.name, value);
  }(bindings));

  return object;
}

template <typename... T>
inline void Object::unpack(Object::Binding<T>... bindings) const {
  auto check = [](auto &binding, std::string_view key, Value const &value) {
    if (binding.name != key)
      return false;

    if (binding.required or not value.isNull()) {
      jansson::unpack(binding.value, value);
      binding.required = false;
    }

    return true;
  };

  for (auto [key, value] : *this) {
    if (not(... or check(bindings, key, value)))
      throw std::out_of_range{
          fmt::format("found unexpected key {} in json object", key)};
  }

  (..., [](auto const &binding) {
    if (binding.required)
      throw std::out_of_range{
          fmt::format("missing required key {} in json object", binding.name)};
  }(bindings));
}

inline Array::Array(Value const &value) : inner(value) {
  value.expect(JSON_ARRAY);
}

inline Array::Array(Value &&value) : inner(std::move(value)) {
  value.expect(JSON_ARRAY);
}

inline Array::Array(std::initializer_list<Value> list) {
  for (auto const &value : list)
    append(value);
}

inline Value const &Array::value() const & noexcept { return inner; }

inline Value &&Array::value() && noexcept { return std::move(inner); }

inline Array::operator Value const &() const & noexcept { return inner; }

inline Array::operator Value() && noexcept { return std::move(inner); }

inline std::size_t Array::size() const noexcept {
  return json_array_size(inner.borrowRawPointer());
}

inline std::optional<Value> Array::get(std::size_t index) const noexcept {
  auto raw = json_array_get(inner.borrowRawPointer(), index);
  if (raw == nullptr)
    return std::nullopt;

  return Value::borrow(*raw);
}

inline Value Array::operator[](std::size_t index) const noexcept {
  return get(index).value_or(jansson::null());
}

inline void Array::set(std::size_t index, Value const &value) {
  internal_error::check(json_array_set(inner.borrowRawPointer(), index,
                                       value.borrowRawPointer()));
}

inline void Array::set(std::size_t index, Value &&value) {
  internal_error::check(json_array_set_new(inner.borrowRawPointer(), index,
                                           value.takeRawPointer()));
}

inline void Array::append(Value const &value) {
  internal_error::check(
      json_array_append(inner.borrowRawPointer(), value.borrowRawPointer()));
}

inline void Array::append(Value &&value) {
  internal_error::check(
      json_array_append_new(inner.borrowRawPointer(), value.takeRawPointer()));
}

inline void Array::insert(std::size_t index, Value const &value) {
  internal_error::check(json_array_insert(inner.borrowRawPointer(), index,
                                          value.borrowRawPointer()));
}

inline void Array::insert(std::size_t index, Value &&value) {
  internal_error::check(json_array_insert_new(inner.borrowRawPointer(), index,
                                              value.takeRawPointer()));
}

inline void Array::remove(std::size_t index) {
  internal_error::check(json_array_remove(inner.borrowRawPointer(), index));
}

inline void Array::clear(std::size_t index) {
  internal_error::check(json_array_clear(inner.borrowRawPointer()));
}

inline void Array::extend(Array const &other) {
  internal_error::check(json_array_extend(inner.borrowRawPointer(),
                                          other.inner.borrowRawPointer()));
}

inline Array::Iterator::Iterator(Array const &arr, std::size_t start) noexcept
    : arr(arr.inner.borrowRawPointer()), index(start) {}

inline Value Array::Iterator::operator*() const noexcept {
  return Value::borrow(json_array_get(arr, index));
}

inline Value Array::Iterator::operator[](difference_type n) const noexcept {
  auto raw = json_array_get(arr, index + n);
  if (raw == nullptr)
    return jansson::null();

  return Value::borrow(*raw);
}

inline Array::Iterator &Array::Iterator::operator++() noexcept {
  ++index;
  return *this;
}

inline Array::Iterator Array::Iterator::operator++(int) noexcept {
  auto ret = *this;
  ++*this;
  return ret;
}

inline Array::Iterator &Array::Iterator::operator--() noexcept {
  --index;
  return *this;
}

inline Array::Iterator Array::Iterator::operator--(int) noexcept {
  auto old = *this;
  --*this;
  return old;
}

inline Array::Iterator &
Array::Iterator::operator+=(difference_type n) noexcept {
  index += n;
  return *this;
}

inline Array::Iterator &
Array::Iterator::operator-=(difference_type n) noexcept {
  index -= n;
  return *this;
}

inline Array::Iterator operator+(Array::Iterator iter,
                                 Array::Iterator::difference_type n) noexcept {
  return iter += n;
}

inline Array::Iterator operator+(Array::Iterator::difference_type n,
                                 Array::Iterator iter) noexcept {
  return iter += n;
}

inline Array::Iterator operator-(Array::Iterator iter,
                                 Array::Iterator::difference_type n) noexcept {
  return iter -= n;
}

inline Array::Iterator::difference_type
operator-(Array::Iterator const &lhs, Array::Iterator const &rhs) noexcept {
  return lhs.index - rhs.index;
}

inline std::partial_ordering operator<=>(Array::Iterator const &lhs,
                                         Array::Iterator const &rhs) noexcept {
  return lhs.arr != rhs.arr ? std::partial_ordering::unordered
                            : lhs.index <=> rhs.index;
}

inline bool operator==(Array::Iterator const &lhs,
                       Array::Iterator const &rhs) noexcept {
  return lhs.arr == rhs.arr and lhs.index == rhs.index;
}

inline Array::Iterator Array::begin() const noexcept {
  return Iterator{*this, 0};
}

inline Array::Iterator Array::end() const noexcept {
  return Iterator{*this, size()};
}

inline void jsonUnpack(Value &v, Value const &value) { v = value; }

inline Value jsonPack(Value const &value) { return value; }

inline void jsonUnpack(Object &obj, Value const &value) {
  obj = value.object();
}

inline Value jsonPack(Object const &obj) { return obj; }

inline void jsonUnpack(Array &arr, Value const &value) { arr = value.array(); }

inline Value jsonPack(Array const &arr) { return arr; }

inline void jsonUnpack(std::string_view &sv, Value const &value) {
  sv = value.string();
}

inline Value jsonPack(std::string_view const &sv) {
  return jansson::string(sv);
}

inline void jsonUnpack(std::string &s, Value const &value) {
  s = value.string();
}

inline Value jsonPack(std::string const &s) { return jansson::string(s); }

inline void jsonUnpack(bool &b, Value const &value) { b = value.boolean(); }

inline Value jsonPack(bool const &b) { return jansson::boolean(b); }

template <std::integral T> inline void jsonUnpack(T &i, Value const &value) {
  auto val = value.integer();
  if (not std::in_range<T>(val))
    throw std::out_of_range{fmt::format("value {} is out of range for type",
                                        val, typeid(T).name())};
  i = val;
}

template <std::integral T> inline Value jsonPack(T const &i) {
  if (not std::in_range<int_t>(i))
    throw std::out_of_range{fmt::format("value {} is out of range for type", i,
                                        typeid(int_t).name())};

  return jansson::integer(i);
}

template <std::floating_point T>
inline void jsonUnpack(T &f, Value const &value) {
  f = value.number();
}

template <std::floating_point T> inline Value jsonPack(T const &f) {
  return jansson::real(f);
}

template <std::floating_point T>
inline void jsonUnpack(std::complex<T> &c, Value const &value) {
  T real = 0, imag = 0;
  value.object().unpack(jansson::bind("real", real), //
                        jansson::bind("imag", imag));
  c = {real, imag};
}

template <std::floating_point T>
inline Value jsonPack(std::complex<T> const &c) {
  return jansson::Object::pack(jansson::bind("real", c.real()), //
                               jansson::bind("imag", c.imag()));
}

template <typename T>
inline void jsonUnpack(std::optional<T> &opt, Value const &value) {
  if (not value.isNull())
    jansson::unpack(opt.emplace(), value);
}

template <typename T> inline Value jsonPack(std::optional<T> const &opt) {
  return opt ? jansson::pack(*opt) : jansson::null();
}

template <vector_like T> inline void jsonUnpack(T &vector, Value const &value) {
  auto array = value.array();
  vector.resize(array.size());
  for (size_t i = 0; i < array.size(); ++i)
    jansson::unpack(vector[i], array[i]);
}

template <vector_like T> inline Value jsonPack(T const &vector) {
  auto array = Array{};
  for (auto const &v : vector)
    array.append(jansson::pack(v));
  return array;
}

template <tuple_like T> inline void jsonUnpack(T &tuple, Value const &value) {
  auto array = value.array();
  [&]<size_t... index>(std::index_sequence<index...>) {
    (..., jansson::unpack(std::get<index>(tuple), array[index]));
  }(std::make_index_sequence<std::tuple_size_v<T>>());
}

template <tuple_like T> inline Value jsonPack(T const &tuple) {
  auto array = Array{};
  [&]<size_t... index>(std::index_sequence<index...>) {
    (..., array.append(jansson::pack(std::get<index>(tuple))));
  }(std::make_index_sequence<std::tuple_size_v<T>>());
  return array;
}

template <map_like T> inline void jsonUnpack(T &map, Value const &value) {
  map.clear();
  for (auto [key, val] : value.object())
    jansson::unpack(map[typename T::key_type{key}], val);
}

template <map_like T> inline Value jsonPack(T &map) {
  auto object = Object{};
  for (auto const &[key, value] : map)
    object.set(key, jansson::pack(value));
  return object;
}

} // namespace villas::jansson
