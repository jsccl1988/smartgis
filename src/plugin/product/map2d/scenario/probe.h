// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SCENARIO_PROBE_H_
#define PLUGIN_MAP2D_SCENARIO_PROBE_H_

#include "plugin/runtime/host/capability/shell.h"

namespace ui {
namespace views {
class DrawHost;
}
}  // namespace ui

namespace plugin {

bool viewport_has_presented_frame(ui::views::DrawHost* pane);

// Resolve + canonicalize out/data china sample under the exe dir, then
// GisScene::open_path. |city_pack| is set when the chosen file is china_city.
bool try_open_china_sample(HarnessShell& browser, bool* city_pack = nullptr);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SCENARIO_PROBE_H_
