// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_

#include <string>
#include <vector>

#include "content/browser/capability/horizon.h"
#include "content/browser/capability/view.h"

namespace ui {
namespace views {
class View;
}
}  // namespace ui

namespace app {

class Browser;

namespace detail {

// Violation and sibling-overlap lines. Counts are the collector return
// values; the vectors are the lines those collectors recorded. No pass/fail.
struct LayoutIssues {
  int violation_count = 0;
  int overlap_count = 0;
  std::vector<std::string> violations;
  std::vector<std::string> overlaps;
};

LayoutIssues collect_layout_issues(ui::views::View* root);

// One layout-forensics dump under ui_forensics/. |run_prefix| becomes
// "<prefix>_<stamp>". |marks_json| when set is a raw JSON array inserted
// after issue_count. |beside_config| uses ../ui_forensics when cwd sits
// next to Debug or Release (ui capture); otherwise out/ui_forensics.
struct LayoutForensics {
  const char* run_prefix = "harness";
  const char* scenario = "--harness";
  const char* marks_json = nullptr;
  bool beside_config = false;
};

void write_layout_forensics(const LayoutForensics& note,
                            const std::vector<std::string>& issues);

// True when the ui-forensics switch is set and not exactly "0".
bool layout_forensics_forced();

// Contents tree counts. No pass/fail.
bool fill_shell_status(Browser& browser, content::ShellStatus* out);

// Layout violation and overlap counts. Dumps forensics when violations
// exist or the ui-forensics switch is on. No pass/fail.
bool fill_layout_status(Browser& browser, content::LayoutStatus* out);

// Fill load facts for |face| ("map" or "scene"). |timeout_ms| > 0 waits
// for a presented frame before the snapshot. No pass/fail.
bool fill_map_load_status(Browser& browser,
                          const std::string& face,
                          int timeout_ms,
                          content::ViewLoadStatus* out);

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_SEMA_EXPECT_H_
