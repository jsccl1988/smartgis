// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_IMPL_SKIA_TRAITS_H_
#define SCENIC_RHI2D_IMPL_SKIA_TRAITS_H_

#include "scenic/render/rhi2d/public/device/viewport.h"
#include "scenic/render/rhi2d/impl/skia/backend/skia_backend.h"

namespace scenic {
namespace detail {

struct SkiaBackendTraits {
  using Backend = SkiaBackend;
  static constexpr base::RenderBaseApi k_api = base::RD_SKIA;
  static constexpr const char* k_name = "Skia";
};

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_IMPL_SKIA_TRAITS_H_
