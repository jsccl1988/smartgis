// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_PIPELINE_HLSL_H_
#define RENDER_RHI_FLYCUBE_PIPELINE_HLSL_H_

#include <string>

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

// DXC must sit beside the exe; CompileShader aborts otherwise.
bool file_exists(const char* path);
bool copy_if_needed(const char* src, const char* dest);
bool ensure_dxc_beside_exe();
bool write_temp_hlsl(const char* name, const char* source, std::string* path);

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_PIPELINE_HLSL_H_
