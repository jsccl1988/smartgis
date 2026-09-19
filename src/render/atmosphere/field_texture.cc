// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/atmosphere/field_texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "render/rhi/rhi.h"

namespace render {
namespace atmosphere {
namespace {

uint8_t float_to_u8(float v, float scale) {
  const float s = (scale > 1.0e-8f) ? scale : 1.0f;
  const float n = std::max(0.0f, std::min(1.0f, v / s));
  return static_cast<uint8_t>(n * 255.0f + 0.5f);
}

}  // namespace

FieldTexture::FieldTexture() = default;

FieldTexture::~FieldTexture() {
  release();
}

bool FieldTexture::upload(rhi::Device* device, int cols, int rows,
                          const float* values, std::size_t value_count,
                          float value_scale) {
  if (!device || !values || cols < 1 || rows < 1) {
    return false;
  }
  const std::size_t need =
      static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows);
  if (value_count != need) {
    return false;
  }

  const bool size_changed = (cols_ != cols || rows_ != rows || !texture_ ||
                             device_ != device);
  if (size_changed) {
    if (device_ && texture_) {
      device_->destroy_texture(texture_);
      texture_ = nullptr;
    }
    device_ = device;
    rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(cols);
    desc.height = static_cast<uint32_t>(rows);
    desc.format = rhi::TextureFormat::kRgba8;
    texture_ = device_->create_texture(desc);
    if (!texture_) {
      return false;
    }
    cols_ = cols;
    rows_ = rows;
  }

  cpu_values_.assign(values, values + value_count);

  std::vector<uint8_t> rgba(need * 4u);
  for (std::size_t i = 0; i < need; ++i) {
    const uint8_t r = float_to_u8(values[i], value_scale);
    rgba[i * 4 + 0] = r;
    rgba[i * 4 + 1] = r;
    rgba[i * 4 + 2] = r;
    rgba[i * 4 + 3] = 255;
  }
  const uint32_t bytes = static_cast<uint32_t>(rgba.size());
  if (!device_->upload_texture(texture_, rgba.data(), bytes)) {
    return false;
  }
  return true;
}

void FieldTexture::release() {
  if (device_ && texture_) {
    device_->destroy_texture(texture_);
  }
  texture_ = nullptr;
  device_ = nullptr;
  cols_ = 0;
  rows_ = 0;
  cpu_values_.clear();
}

}  // namespace atmosphere
}  // namespace render
