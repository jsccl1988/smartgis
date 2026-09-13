#include "render/renderdevice.h"
#include "render/rhi/rhi.h"

using namespace base;

namespace render
{
	RENDER_EXPORT_API void bind_rhi_present(void* native_window) {
		static render::rhi::Device* device = nullptr;
		if (!device) {
			device = render::rhi::create_device(render::rhi::Backend::kGdi);
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
