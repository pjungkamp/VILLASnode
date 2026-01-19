/* implementation for the C++ libjansson C API wrapper
 *
 * Author: Philipp Jungkamp <philipp@jungkamp.dev>
 * SPDX-FileCopyrightText: 2026 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fmt/format.h>
#include <fmt/std.h>

#include <villas/jansson.hpp>

namespace villas::jansson {

Value Value::copy() const { return Value::take(json_copy(ptr.get())); }

Value Value::deepCopy() const { return Value::take(json_deep_copy(ptr.get())); }

Value Value::fromString(std::string_view string, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadb(string.data(), string.size(), flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

std::string Value::toString(size_t flags) const {
  if (auto s = json_dumps(ptr.get(), flags); s != nullptr)
    return std::string{s};

  throw internal_error{};
}

Value Value::loadFromFileStream(std::FILE *file, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadf(file, flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

void Value::dumpToFileStream(std::FILE *file, size_t flags) const {
  if (auto ret = json_dumpf(ptr.get(), file, flags); ret == -1)
    throw internal_error{};
}

Value Value::loadFromFileDescriptor(int fd, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_loadfd(fd, flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

void Value::dumpToFileDescriptor(int fd, size_t flags) const {
  if (auto ret = json_dumpfd(ptr.get(), fd, flags); ret == -1)
    throw internal_error{};
}

Value Value::loadFromFilePath(char const *path, std::size_t flags) {
  json_error_t err;
  if (auto raw = json_load_file(path, flags, &err))
    return Value{*raw};
  else
    throw parse_error{err};
}

void Value::dumpToFilePath(char const *path, size_t flags) const {
  if (auto ret = json_dump_file(ptr.get(), path, flags); ret == -1)
    throw internal_error{};
}

Object Object::copy() const {
  return Object(inner.copy());
}

Object Object::deepCopy() const {
  return Object(inner.deepCopy());
}

bool Object::optionalValueIsEmpty(Value const &value) noexcept {
  switch (value.type()) {
    using enum type_t;
  case JSON_OBJECT: {
    return value.object().size() == 0;
  } break;

  case JSON_ARRAY: {
    return value.array().size() == 0;
  } break;

  case JSON_STRING: {
    return value.string().size() == 0;
  } break;

  case JSON_NULL: {
    return true;
  } break;

  default: {
    return false;
  } break;
  }
}

Array Array::copy() const {
  return Array(inner.copy());
}

Array Array::deepCopy() const {
  return Array(inner.deepCopy());
}

bad_access::bad_access(type_t expected, type_t found)
    : bad_access(typeToString(expected), typeToString(found)) {}

bad_access::bad_access(std::string_view expected, std::string_view found)
    : message(fmt::format("expected {} but found {}", expected, found)) {}

char const *bad_access::what() const noexcept { return message.c_str(); }

internal_error::internal_error(std::source_location src)
    : message(fmt::format("internal error in {}", src)) {}

char const *internal_error::what() const noexcept { return message.c_str(); }

parse_error::parse_error(json_error_t const &err) : error(err) {}

json_error_t const &parse_error::operator*() const { return error; }

json_error_t const *parse_error::operator->() const { return &error; }

char const *parse_error::what() const noexcept { return error.text; }

} // namespace villas::jansson

