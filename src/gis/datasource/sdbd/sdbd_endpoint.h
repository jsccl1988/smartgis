// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_
#define GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_

#include <cstdlib>
#include <string>
#include "base/process/switches.h"

namespace gis {
namespace datasource {

// One transport outcome for remote SdbdClient (HTTP/FnRPC). `message` may be
// filled when parsing a JSON error body; transport calls often leave it empty.
struct SdbdCallResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
  std::string message;
};

// Default remote HTTP base. SG_SDBD_BASE overrides. Not the local SDBD: prefix.
inline constexpr char kDefaultSdbdBase[] = "http://127.0.0.1:8021";

inline std::string sdbd_default_base_url() {
  if (const char* env = base::switch_cstr("sdbd-base")) {
    if (env[0] != '\0') {
      return std::string(env);
    }
  }
  return kDefaultSdbdBase;
}

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_
