// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/embed/embed_sample.h"

namespace content {

bool open_map_host_path(EmbedMapHost* host, const char* path) {
  if (!host || !path || path[0] == '\0') {
    return false;
  }

  // Synthetic view id for sample hosts (no MapContents pipe / GPU child).
  host->view_id = 1;
  host->path = path;

  if (!host->view_host.edits() || !host->view_host.workspace() ||
      !host->view_host.events()) {
    host->view_id = 0;
    host->path.clear();
    return false;
  }

  return true;
}

}  // namespace content
