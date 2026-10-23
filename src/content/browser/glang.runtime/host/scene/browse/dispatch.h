// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Still-capture face table. Adapters register an id; the binder looks up.

#ifndef IL_RUNTIME_HOST_SCENE_BROWSE_DISPATCH_H_
#define IL_RUNTIME_HOST_SCENE_BROWSE_DISPATCH_H_

#include <string_view>

#include "content/browser/glang.runtime/host/bind/shell.h"

namespace content {
namespace detail {

struct StillFaceAdapter {
  const char* id = nullptr;
  bool (*capture)(content::Shell& browser, const wchar_t* mark_leaf) =
      nullptr;
};

void register_still_face(StillFaceAdapter adapter);

// False when |face| was not registered. A registered adapter's own result
// is returned unchanged.
bool capture_still_face(content::Shell& browser,
                        const wchar_t* mark_leaf,
                        std::string_view face);

}  // namespace detail
}  // namespace content

#endif  // IL_RUNTIME_HOST_SCENE_BROWSE_DISPATCH_H_
