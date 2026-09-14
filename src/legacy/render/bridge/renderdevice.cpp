// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/bridge/renderdevice.h"
#include "render/rhi/rhi.h"

using namespace base;

namespace render
{
	RENDER_EXPORT_API void bind_rhi_present(void* /*native_window*/) {
		// Intentionally a no-op for the GDI MDI host. Creating D3D/Vulkan on
		// the same HWND as SmtGdiRenderDevice caused STATUS_FATAL_APP_EXIT
		// (0xC000041D) during view bring-up. FlyCube/3D attaches RHI on its
		// own HWND via MapViewport / scene controllers.
	}
}
