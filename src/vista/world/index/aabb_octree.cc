// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/index/aabb_octree.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "Octree.hpp"

namespace vista {
namespace {

struct Center {
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};

bool boxes_overlap(const AabbBox& a, float min_x, float min_y, float min_z,
                   float max_x, float max_y, float max_z) {
  return a.min_x <= max_x && a.max_x >= min_x && a.min_y <= max_y &&
         a.max_y >= min_y && a.min_z <= max_z && a.max_z >= min_z;
}

constexpr size_t kUnibnMinBoxes = 8;

void fill_frustum_linear(const std::vector<AabbBox>& boxes,
                         const FrustumPlanes& frustum,
                         std::vector<uint32_t>* hits) {
  hits->clear();
  hits->reserve(boxes.size());
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    const AabbBox& b = boxes[i];
    if (aabb_intersects_frustum(b.min_x, b.min_y, b.min_z, b.max_x, b.max_y,
                                b.max_z, frustum)) {
      hits->push_back(i);
    }
  }
}

void fill_aabb_linear(const std::vector<AabbBox>& boxes, float min_x,
                      float min_y, float min_z, float max_x, float max_y,
                      float max_z, std::vector<uint32_t>* hits) {
  hits->clear();
  hits->reserve(boxes.size());
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    if (boxes_overlap(boxes[i], min_x, min_y, min_z, max_x, max_y, max_z)) {
      hits->push_back(i);
    }
  }
}

}  // namespace

struct AabbOctree::Impl {
  std::vector<AabbBox> boxes;
  std::vector<Center> centers;
  float max_half_diag = 0.f;
  unibn::Octree<Center> tree;
  bool built = false;

  void reset_tree() {
    tree.clear();
    built = false;
  }
};

AabbOctree::AabbOctree() : impl_(new Impl()) {}

AabbOctree::~AabbOctree() {
  delete impl_;
  impl_ = nullptr;
}

AabbOctree::AabbOctree(AabbOctree&& other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

AabbOctree& AabbOctree::operator=(AabbOctree&& other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

void AabbOctree::clear() {
  if (!impl_) {
    impl_ = new Impl();
    return;
  }
  impl_->reset_tree();
  impl_->boxes.clear();
  impl_->centers.clear();
  impl_->max_half_diag = 0.f;
}

void AabbOctree::rebuild(const AabbBox* boxes, size_t count) {
  clear();
  if (!boxes || count == 0) {
    return;
  }
  impl_->boxes.assign(boxes, boxes + count);
  impl_->centers.resize(count);
  impl_->max_half_diag = 0.f;
  for (size_t i = 0; i < count; ++i) {
    const AabbBox& b = impl_->boxes[i];
    const float hx = 0.5f * (b.max_x - b.min_x);
    const float hy = 0.5f * (b.max_y - b.min_y);
    const float hz = 0.5f * (b.max_z - b.min_z);
    impl_->centers[i].x = b.min_x + hx;
    impl_->centers[i].y = b.min_y + hy;
    impl_->centers[i].z = b.min_z + hz;
    const float hd = std::sqrt(hx * hx + hy * hy + hz * hz);
    if (hd > impl_->max_half_diag) {
      impl_->max_half_diag = hd;
    }
  }
  impl_->built = true;
  if (count >= kUnibnMinBoxes) {
    unibn::OctreeParams params(32, /*copyPoints=*/true);
    impl_->tree.initialize(impl_->centers, params);
  }
}

size_t AabbOctree::size() const {
  return impl_ ? impl_->boxes.size() : 0;
}

bool AabbOctree::query_frustum(const FrustumPlanes& frustum,
                               const float view[16], const float proj[16],
                               std::vector<uint32_t>* hits) const {
  if (!hits || !view || !proj) {
    return false;
  }
  hits->clear();
  if (!impl_ || !impl_->built || impl_->boxes.empty()) {
    return true;
  }
  if (impl_->boxes.size() < kUnibnMinBoxes) {
    fill_frustum_linear(impl_->boxes, frustum, hits);
    return true;
  }
  float fmin_x = 0.f;
  float fmin_y = 0.f;
  float fmin_z = 0.f;
  float fmax_x = 0.f;
  float fmax_y = 0.f;
  float fmax_z = 0.f;
  if (!frustum_world_aabb(view, proj, &fmin_x, &fmin_y, &fmin_z, &fmax_x, &fmax_y,
                          &fmax_z)) {
    return false;
  }
  const float hx = 0.5f * (fmax_x - fmin_x);
  const float hy = 0.5f * (fmax_y - fmin_y);
  const float hz = 0.5f * (fmax_z - fmin_z);
  const Center query{fmin_x + hx, fmin_y + hy, fmin_z + hz};
  const float fr_hd = std::sqrt(hx * hx + hy * hy + hz * hz);
  const float radius = fr_hd + impl_->max_half_diag + 1e-3f;
  std::vector<uint32_t> cand;
  impl_->tree.radiusNeighbors<unibn::L2Distance<Center>>(query, radius, cand);
  hits->reserve(cand.size());
  for (uint32_t i : cand) {
    if (i >= impl_->boxes.size()) {
      continue;
    }
    const AabbBox& b = impl_->boxes[i];
    if (aabb_intersects_frustum(b.min_x, b.min_y, b.min_z, b.max_x, b.max_y,
                                b.max_z, frustum)) {
      hits->push_back(i);
    }
  }
  return true;
}

void AabbOctree::query_aabb(float min_x, float min_y, float min_z, float max_x,
                            float max_y, float max_z,
                            std::vector<uint32_t>* hits) const {
  if (!hits) {
    return;
  }
  hits->clear();
  if (!impl_ || !impl_->built || impl_->boxes.empty()) {
    return;
  }
  if (impl_->boxes.size() < kUnibnMinBoxes) {
    fill_aabb_linear(impl_->boxes, min_x, min_y, min_z, max_x, max_y, max_z,
                     hits);
    return;
  }
  const float hx = 0.5f * (max_x - min_x);
  const float hy = 0.5f * (max_y - min_y);
  const float hz = 0.5f * (max_z - min_z);
  const Center query{min_x + hx, min_y + hy, min_z + hz};
  const float q_hd = std::sqrt(hx * hx + hy * hy + hz * hz);
  const float radius = q_hd + impl_->max_half_diag + 1e-3f;
  std::vector<uint32_t> cand;
  impl_->tree.radiusNeighbors<unibn::L2Distance<Center>>(query, radius, cand);
  hits->reserve(cand.size());
  for (uint32_t i : cand) {
    if (i >= impl_->boxes.size()) {
      continue;
    }
    if (boxes_overlap(impl_->boxes[i], min_x, min_y, min_z, max_x, max_y,
                      max_z)) {
      hits->push_back(i);
    }
  }
}

}  // namespace vista
