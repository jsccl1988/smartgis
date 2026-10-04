// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_RENDER_RHI_IMPL_D3D_PREREQUISITES_H_
#define LEGACY_RENDER_RHI_IMPL_D3D_PREREQUISITES_H_

#include "scenic/render/rhi3d/public/device/base.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#endif  // LEGACY_RENDER_RHI_IMPL_D3D_PREREQUISITES_H_
