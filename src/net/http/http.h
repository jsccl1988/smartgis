// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_HTTP_H
#define SMT_NET_HTTP_H

#include "net/net_export.h"

#include <string>

namespace net {

// Outcome of one HTTP request. ok is false on transport failure.
struct HttpResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
};

// Product HTTP facade. cpp-httplib stays in the .cc.
class NET_EXPORT HttpClient {
 public:
  HttpResult get(const std::string& url, int timeout_sec = 5);
  HttpResult post(const std::string& url, const std::string& body,
                   const std::string& content_type =
                       "application/x-www-form-urlencoded",
                   int timeout_sec = 5);
};

}  // namespace net

#endif  // SMT_NET_HTTP_H
