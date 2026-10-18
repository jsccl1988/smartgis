// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Viewport pass/fail policy. Reads generic view facts and applies gate codes.
// Scene adapters decide which face string means which product view.

#ifndef IL_RUNTIME_HOST_SCENE_POLICY_GATE_H_
#define IL_RUNTIME_HOST_SCENE_POLICY_GATE_H_

#include <string_view>

#include "content/browser/capability/host.h"

namespace content {
namespace detail {

// Wait until the face has a pane. Scene faces miss at rc 9 (frame at 10).
// Other faces miss at rc 3. A pane that is not a content view is not a failure.
bool wait_viewport(content::CapabilityHost& host,
                   std::string_view face,
                   int timeout_ms,
                   int want_frame);

bool expect_scene_visible(content::CapabilityHost& host);
bool expect_orbit_moved(content::CapabilityHost& host);
bool expect_map_hwnd_sync(content::CapabilityHost& host);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_HOST_SCENE_POLICY_GATE_H_
