// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/mesh/mesh_scratch.h"

namespace vista {
namespace detail {

base::ObjectPool<std::vector<PolyPt>>& poly_pt_vec_pool() {
  thread_local base::ObjectPool<std::vector<PolyPt>> pool(
      16, nullptr, [](std::vector<PolyPt>* v) { v->clear(); });
  return pool;
}

base::ObjectPool<std::vector<Vec2>>& vec2_vec_pool() {
  thread_local base::ObjectPool<std::vector<Vec2>> pool(
      8, nullptr, [](std::vector<Vec2>* v) { v->clear(); });
  return pool;
}

base::ObjectPool<std::vector<std::vector<PolyPt>>>& dash_vec_pool() {
  thread_local base::ObjectPool<std::vector<std::vector<PolyPt>>> pool(
      4, nullptr, [](std::vector<std::vector<PolyPt>>* v) { v->clear(); });
  return pool;
}

}  // namespace detail
}  // namespace vista
