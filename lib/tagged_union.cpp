/* A type-safe tagged_union sum type for C++
 *
 * Author: Philipp Jungkamp <philipp.jungkamp@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/tagged_union.hpp>

namespace villas::tagged_union_impl {

void copy(copy_fn const vtable[], std::size_t index, std::byte *self,
          std::byte const *other) {
  vtable[index](self, other);
}

void move(move_fn const vtable[], std::size_t index, std::byte *self,
          std::byte *other) {
  vtable[index](self, other);
}

void destruct(destruct_fn const vtable[], std::size_t index, std::byte *self) {
  vtable[index](self);
}

} // namespace villas::tagged_union_impl
