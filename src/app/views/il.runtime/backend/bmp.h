// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_BMP_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_BMP_H_

#include <cstddef>
#include <windows.h>

namespace app {
namespace detail {

// Shared IL / browse.3d software SoT size (not browse 2d 1280x720).
constexpr int kCaptureW = 640;
constexpr int kCaptureH = 480;
// Reject corrupt or hostile headers before allocating the pixel buffer.
constexpr int kMaxBmpEdge = 8192;

// Emit a BMP file. Does not judge whether the pixels are a lit map.
bool write_bmp_file(const wchar_t* path,
                    const BITMAPINFOHEADER& bi,
                    const void* pixels,
                    size_t nbytes);

// Writes <bmp>.engine.txt next to the capture (FlyCube/DX12 vs GDI).
bool write_engine_sidecar(const wchar_t* bmp_w, const char* engine);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_BMP_H_
