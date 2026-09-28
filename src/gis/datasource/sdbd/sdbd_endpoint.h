// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_
#define GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_

#include <cstdlib>
#include <string>

namespace gis {
namespace datasource {

// One transport outcome for remote SdbdClient (HTTP/FnRPC) and for the local
// handler error body. `message` is filled only when parsing a handler JSON
// error; transport calls leave it empty.
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
  if (const char* env = std::getenv("SG_SDBD_BASE")) {
    if (env[0] != '\0') {
      return std::string(env);
    }
  }
  return kDefaultSdbdBase;
}

// Shared JSON string escape for sdbd wire text. Not the style/json_mini parser.
inline std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(static_cast<char>(c));
        break;
    }
  }
  return out;
}

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_SDBD_ENDPOINT_H_
