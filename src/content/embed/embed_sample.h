// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_EMBED_EMBED_SAMPLE_H_
#define CONTENT_EMBED_EMBED_SAMPLE_H_

#include <cstdint>
#include <string>

#include "content/content_export.h"
#include "content/public/view_host.h"

namespace content {

// Reserved process exit codes for product hosts that surface M4 outcomes
// (documented for optional main / shell wiring; this helper does not exit):
//   100 — edit conflict (stale optimistic version token)
//   101 — embed open_map_host_path failure
constexpr int kExitEditConflict = 100;
constexpr int kExitEmbedOpenFailed = 101;

// Minimal embed host path: ViewHost shell binding + recorded map path.
// Product hosts that own a live GPU child additionally call
// MapContents::Create / OpenView / CatalogCall after StartRenderProcess.
struct EmbedMapHost {
  ViewHost view_host;
  uint32_t view_id = 0;
  std::string path;
};

// Bind ViewHost for |path| (HWND-free console / sample hosts).
// Returns false when |host| is null or |path| is empty.
CONTENT_EXPORT bool open_map_host_path(EmbedMapHost* host, const char* path);

}  // namespace content

#endif  // CONTENT_EMBED_EMBED_SAMPLE_H_
