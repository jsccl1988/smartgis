// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CONTENTS_CONTENTS_HOST_PIPE_H_
#define CONTENT_BROWSER_CONTENTS_CONTENTS_HOST_PIPE_H_

#include <cstddef>
#include <cstdint>

#include "content/public/types.h"

namespace content {

class GisContents;
class GisContentsObserver;

namespace detail {

// OOP host-pipe ops owned by GisContentsImpl. PluginHost and document
// adapters call through as_contents_host_pipe — not an embedder API.
class ContentsHostPipe {
 public:
  virtual ~ContentsHostPipe() = default;

  virtual void pipe_set_selection(uint32_t view_id,
                                  const FeatureId* ids,
                                  size_t n) = 0;
  virtual void pipe_legend_snapshot(uint32_t view_id) = 0;
  virtual void pipe_catalog_call(const char* json_op) = 0;
  virtual void pipe_dispatch_plugin(uint32_t view_id,
                                    const char* plugin_id,
                                    const char* method,
                                    const void* bytes,
                                    size_t n) = 0;
  virtual void pipe_activate_tool(uint32_t view_id, const char* tool_id) = 0;

  virtual GisContentsObserver* contents_observer() const = 0;
};

ContentsHostPipe* as_contents_host_pipe(GisContents* contents);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_CONTENTS_CONTENTS_HOST_PIPE_H_
