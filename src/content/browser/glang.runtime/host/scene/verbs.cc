// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scene and browse script verbs. They occupy host opcode slots. The language
// table does not name these commands.

#include "content/browser/glang.runtime/host/scene/verbs.h"

#include "content/browser/glang.runtime/exec/op/host_verb.h"

#include "content/browser/glang.runtime/host/scene/policy/gate.h"

namespace content {
namespace code {

bool eval_invalidate(content::CapabilityHost& host,
                     const Module&,
                     const Inst& inst,
                     SlotTable&,
                     std::string* error) {
  const bool ok = host.view.invalidate_map2d && host.view.invalidate_map2d();
  return ok_step(inst, ok, error);
}

bool eval_viewport(content::CapabilityHost& host,
                   const Module& module,
                   const Inst& inst,
                   SlotTable& slots,
                   std::string* error) {
  return ok_step(inst,
                 detail::wait_viewport(host, arg_str_param(module, inst, slots, 0),
                                       arg_i32_param(module, inst, slots, 1),
                                       arg_i32_param(module, inst, slots, 2)),
                 error);
}

bool eval_browse(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  const int count = arg_i32_param(module, inst, slots, 0);
  const bool ok = host.view.browse_stress && host.view.browse_stress(count);
  return ok_step(inst, ok, error);
}

bool eval_still(content::CapabilityHost& host,
                const Module& module,
                const Inst& inst,
                SlotTable& slots,
                std::string* error) {
  const std::string face = arg_str_param(module, inst, slots, 0);
  const bool ok =
      host.view.capture_browse_still && host.view.capture_browse_still(face);
  return ok_step(inst, ok, error);
}

bool eval_bench(content::CapabilityHost& host,
                const Module&,
                const Inst& inst,
                SlotTable&,
                std::string* error) {
  const bool ok = host.view.fps_bench && host.view.fps_bench();
  return ok_step(inst, ok, error);
}

bool eval_fit(content::CapabilityHost& host,
              const Module&,
              const Inst& inst,
              SlotTable&,
              std::string* error) {
  const bool ok = host.view.fit_scene_box && host.view.fit_scene_box();
  return ok_step(inst, ok, error);
}

bool eval_camera(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 std::string* error) {
  const bool ok = host.view.camera_fly &&
                  host.view.camera_fly(arg_str_param(module, inst, slots, 0),
                                       arg_i32_param(module, inst, slots, 1),
                                       arg_i32_param(module, inst, slots, 2));
  return ok_step(inst, ok, error);
}

bool eval_visible(content::CapabilityHost& host,
                  const Module&,
                  const Inst& inst,
                  SlotTable&,
                  std::string* error) {
  return ok_step(inst, detail::expect_scene_visible(host), error);
}

bool eval_orbit(content::CapabilityHost& host,
                const Module&,
                const Inst& inst,
                SlotTable&,
                std::string* error) {
  return ok_step(inst, detail::expect_orbit_moved(host), error);
}

bool eval_hwnd(content::CapabilityHost& host,
               const Module&,
               const Inst& inst,
               SlotTable&,
               std::string* error) {
  return ok_step(inst, detail::expect_map_hwnd_sync(host), error);
}

constexpr Param kViewportParams[] = {
    {"face", 0, ArgKind::kStr, false, 0, "map"},
    {"ms", 1, ArgKind::kI32, false, 90000, nullptr},
    {"frame", -1, ArgKind::kI32, false, 1, nullptr},
};
constexpr Param kCountParams[] = {
    {"count", 0, ArgKind::kI32, false, 24, nullptr},
};
constexpr Param kFaceParams[] = {
    {"face", 0, ArgKind::kStr, false, 0, "map2d"},
};
constexpr Param kFlyParams[] = {
    {"mode", 0, ArgKind::kStr, false, 0, "orbit"},
    {"ms", 1, ArgKind::kI32, false, 1600, nullptr},
    {"steps", 2, ArgKind::kI32, false, 20, nullptr},
};

const OpDesc kInvalidate{
    Op::kHostSlotInvalidate, "invalidate_map2d", nullptr, nullptr, 0, false,
    &eval_invalidate,
};
const OpDesc kViewport{
    Op::kHostSlotViewport, "wait_viewport", nullptr, kViewportParams, 3, false,
    &eval_viewport,
};
const OpDesc kBrowse{
    Op::kHostSlotBrowse, "browse_stress", nullptr, kCountParams, 1, false,
    &eval_browse,
};
const OpDesc kStill{
    Op::kHostSlotStill, "capture_browse_still", nullptr, kFaceParams, 1, false,
    &eval_still,
};
const OpDesc kBench{
    Op::kHostSlotBench, "fps_bench", nullptr, nullptr, 0, false, &eval_bench,
};
const OpDesc kFit{
    Op::kHostSlotFit, "fit_scene_box", nullptr, nullptr, 0, false, &eval_fit,
};
const OpDesc kCamera{
    Op::kHostSlotCamera, "camera_fly", nullptr, kFlyParams, 3, false, &eval_camera,
};
const OpDesc kVisible{
    Op::kHostSlotVisible, "expect_scene_visible", nullptr, nullptr, 0, false,
    &eval_visible,
};
const OpDesc kOrbit{
    Op::kHostSlotOrbit, "expect_orbit_moved", nullptr, nullptr, 0, false,
    &eval_orbit,
};
const OpDesc kHwnd{
    Op::kHostSlotHwnd, "expect_map_hwnd_sync", nullptr, nullptr, 0, false,
    &eval_hwnd,
};

void install_scene_verbs() {
  register_host_verb(&kInvalidate);
  register_host_verb(&kViewport);
  register_host_verb(&kBrowse);
  register_host_verb(&kStill);
  register_host_verb(&kBench);
  register_host_verb(&kFit);
  register_host_verb(&kCamera);
  register_host_verb(&kVisible);
  register_host_verb(&kOrbit);
  register_host_verb(&kHwnd);
}

void install_scene_verb_table() {
  set_host_verb_installer(&install_scene_verbs);
}

struct SceneVerbInstaller {
  SceneVerbInstaller() { install_scene_verb_table(); }
};

const SceneVerbInstaller kSceneVerbInstaller;

}  // namespace code
}  // namespace content
