// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_IR_DOCUMENT_H_
#define IL_RUNTIME_IR_DOCUMENT_H_

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace app {
namespace ir {

// Document, present timers, and export. No script AST.

inline bool load_china_sample(content::CapabilityHost& host, bool write_stub) {
  return host.load_china_sample && host.load_china_sample(write_stub);
}

inline bool open_map(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.open_map && host.open_map(std::string(path));
}

inline void detach_maps(content::CapabilityHost& host) {
  if (host.detach_maps) {
    host.detach_maps();
  }
}

inline void stop_map_timers(content::CapabilityHost& host) {
  if (host.stop_map_present_timers) {
    host.stop_map_present_timers();
  }
}

inline void resume_map_timers(content::CapabilityHost& host) {
  if (host.resume_map_present_timers) {
    host.resume_map_present_timers();
  }
}

inline void clear_marks(content::CapabilityHost& host) {
  if (host.clear_marks) {
    host.clear_marks();
  }
}

inline bool doc_clear(content::CapabilityHost& host) {
  return host.doc_clear && host.doc_clear();
}

inline bool fit_extent(content::CapabilityHost& host) {
  return host.fit_extent && host.fit_extent();
}

inline bool export_bmp(content::CapabilityHost& host,
                       std::string_view leaf,
                       std::string_view frame) {
  return host.export_bmp &&
         host.export_bmp(std::string(leaf), std::string(frame));
}

inline bool suppress_dialogs(content::CapabilityHost& host, bool on) {
  return host.suppress_dialogs && host.suppress_dialogs(on);
}

inline bool apply_style_file(content::CapabilityHost& host,
                             std::string_view path) {
  return !path.empty() && host.apply_style_file &&
         host.apply_style_file(std::string(path));
}

inline bool invalidate_map2d(content::CapabilityHost& host) {
  return host.invalidate_map2d && host.invalidate_map2d();
}

}  // namespace ir
}  // namespace app

#endif  // IL_RUNTIME_IR_DOCUMENT_H_
