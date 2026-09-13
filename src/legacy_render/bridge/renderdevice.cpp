// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy_render/bridge/renderdevice.h"
#include "render/rhi/rhi.h"

using namespace base;

namespace render
{
	RENDER_EXPORT_API void bind_rhi_present(void* native_window) {
		static render::rhi::Device* device = nullptr;
		if (!device) {
			device = render::rhi::create_device(
			    render::rhi::preferred_gpu_backend());
			render::rhi::DeviceDesc probe;
			probe.native_window = native_window;
			if (!device || !device->initialize(probe)) {
				if (device) {
					device->shutdown();
					delete device;
				}
				device = render::rhi::create_device(
				    render::rhi::Backend::kGdi);
			}
		}
		if (!device) {
			return;
		}
		render::rhi::DeviceDesc desc;
		desc.native_window = native_window;
		if (device->initialize(desc)) {
			device->present();
		}
	}
}
