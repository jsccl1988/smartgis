// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_NET_HTTP_H
#define SMT_NET_HTTP_H

#include "net/net_export.h"

#include <string>

namespace net {

// Outcome of one HTTP(S) request. ok is false on transport failure.
struct HttpResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
};

// Product HTTP(S) facade. cpp-httplib (+ OpenSSL when enabled) stays in the .cc.
class NET_EXPORT HttpClient {
 public:
  // When false, TLS peers are not certificate-verified (loopback / self-signed
  // tests). Default true for product HTTPS.
  void set_ssl_verify(bool verify) { ssl_verify_ = verify; }
  bool ssl_verify() const { return ssl_verify_; }

  HttpResult get(const std::string& url, int timeout_sec = 5);
  HttpResult post(const std::string& url, const std::string& body,
                   const std::string& content_type =
                       "application/x-www-form-urlencoded",
                   int timeout_sec = 5);

 private:
  bool ssl_verify_ = true;
};

}  // namespace net

#endif  // SMT_NET_HTTP_H
