// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/scene/browse/dispatch.h"

#include "base/core/log.h"

namespace content {
namespace detail {
namespace {

struct FaceTable {
  static constexpr int kMax = 8;
  StillFaceAdapter items[kMax]{};
  int count = 0;
};

FaceTable& faces() {
  static FaceTable table;
  return table;
}

}  // namespace

void register_still_face(StillFaceAdapter adapter) {
  if (!adapter.id || !adapter.id[0] || !adapter.capture) {
    return;
  }
  FaceTable& table = faces();
  for (int i = 0; i < table.count; ++i) {
    if (table.items[i].id && std::string_view(table.items[i].id) == adapter.id) {
      table.items[i] = adapter;
      return;
    }
  }
  if (table.count >= FaceTable::kMax) {
    LOGGING(LOG_ERROR, "still face table full at %s", adapter.id);
    return;
  }
  table.items[table.count++] = adapter;
}

bool capture_still_face(content::Shell& browser,
                        const wchar_t* mark_leaf,
                        std::string_view face) {
  const FaceTable& table = faces();
  for (int i = 0; i < table.count; ++i) {
    if (table.items[i].id && face == table.items[i].id && table.items[i].capture) {
      return table.items[i].capture(browser, mark_leaf);
    }
  }
  return false;
}

}  // namespace detail
}  // namespace content
