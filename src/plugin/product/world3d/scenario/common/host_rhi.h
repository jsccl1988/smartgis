// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_HOST_RHI_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_HOST_RHI_H_

// Host-linked HWND/RHI types. Product scenario TUs call these; the
// implementations stay in app/views/il.runtime/backend/capture.
#include "app/views/il.runtime/backend/gdi.h"
#include "app/views/il.runtime/backend/capture_host.h"
#include "app/views/il.runtime/backend/capture.h"
#include "app/views/il.runtime/backend/gate.h"
#include "app/views/il.runtime/backend/session.h"

namespace plugin {
namespace detail {

using app::detail::GpuEnvPolicy;
using app::detail::GpuEnvOpts;
using app::detail::RhiPresentSession;
using app::detail::RhiPresentSessionOpts;
using app::detail::RhiPresentTeardownOpts;
using app::detail::Scene3dHwndCaptureOpts;
using app::detail::ShowcasePresentHwndOpts;
using app::detail::capture_scene3d_hwnd_bmp;
using app::detail::create_showcase_present_hwnd;
using app::detail::destroy_rhi_owned_present_hwnd;
using app::detail::prepare_rhi_present_session;
using app::detail::present_shell_scene3d_frame;
using app::detail::resolve_rhi_want_gpu;
using app::detail::shell_scene3d_capture_hwnd;
using app::detail::teardown_rhi_present_session;
using app::detail::blit_client_to_dib;
using app::detail::bmp_file_has_visible_signal;
using app::detail::BmpFileCheckOpts;

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_HOST_RHI_H_
