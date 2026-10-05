// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_ATMOSPHERE_DETAIL_FIELD_TEXTURE_H_
#define VISTA_PASS_ATMOSPHERE_DETAIL_FIELD_TEXTURE_H_

#include <cstddef>
#include <cstdint>
#include <vector>


namespace render {
namespace rhi {
class Device;
class Texture;
}  // namespace rhi

}  // namespace render

#include "vista/vista_export.h"

namespace vista {

// Uploads FieldStore channel slices to RHI textures for ocean / cloud passes.
// Float samples are packed into RGBA8 (R channel normalized; CPU copy kept).
class VISTA_EXPORT FieldTexture {
 public:
  FieldTexture();
  ~FieldTexture();

  FieldTexture(const FieldTexture&) = delete;
  FieldTexture& operator=(const FieldTexture&) = delete;

  // Upload a regular-grid float slice. Creates / resizes an RHI texture.
  // value_scale maps float → [0,1] for the R byte (mask uses 1.0).
  bool upload(render::rhi::Device* device, int cols, int rows, const float* values,
              std::size_t value_count, float value_scale = 1.0f);

  void release();

  int cols() const { return cols_; }
  int rows() const { return rows_; }
  bool empty() const { return cols_ < 1 || rows_ < 1; }

  render::rhi::Texture* texture() { return texture_; }
  const render::rhi::Texture* texture() const { return texture_; }

  // CPU-side float copy for Null / CPU discard paths.
  const std::vector<float>& cpu_values() const { return cpu_values_; }

 private:
  render::rhi::Device* device_ = nullptr;
  render::rhi::Texture* texture_ = nullptr;
  int cols_ = 0;
  int rows_ = 0;
  std::vector<float> cpu_values_;
};

}  // namespace vista

#endif  // VISTA_PASS_ATMOSPHERE_DETAIL_FIELD_TEXTURE_H_
