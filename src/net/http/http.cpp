// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

// Must precede httplib.h. OpenSSL linked via //third_party:openssl.
#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include "httplib.h"

#include "net/http/http.h"

#include <string>

namespace net {
namespace {

bool split_url(const std::string& url, std::string* origin, std::string* path) {
  if (origin == nullptr || path == nullptr) {
    return false;
  }
  const auto scheme = url.find("://");
  if (scheme == std::string::npos) {
    return false;
  }
  const auto rest = scheme + 3;
  const auto slash = url.find('/', rest);
  if (slash == std::string::npos) {
    *origin = url;
    *path = "/";
    return true;
  }
  *origin = url.substr(0, slash);
  *path = url.substr(slash);
  if (path->empty()) {
    *path = "/";
  }
  return true;
}

void apply_timeout(httplib::Client* cli, int timeout_sec) {
  const int sec = timeout_sec > 0 ? timeout_sec : 5;
  cli->set_connection_timeout(sec, 0);
  cli->set_read_timeout(sec, 0);
  cli->set_write_timeout(sec, 0);
}

void apply_ssl(httplib::Client* cli, bool verify) {
  if (cli == nullptr) {
    return;
  }
  cli->enable_server_certificate_verification(verify);
}

HttpResult from_response(const httplib::Result& res, const char* what) {
  HttpResult out;
  if (!res) {
    out.error = what;
    return out;
  }
  out.status = res->status;
  out.body = res->body;
  out.ok = res->status >= 200 && res->status < 300;
  if (!out.ok) {
    out.error = "http status";
  }
  return out;
}

}  // namespace

HttpResult HttpClient::get(const std::string& url, int timeout_sec) {
  std::string origin;
  std::string path;
  if (!split_url(url, &origin, &path)) {
    HttpResult out;
    out.error = "invalid url";
    return out;
  }
  httplib::Client cli(origin);
  apply_timeout(&cli, timeout_sec);
  apply_ssl(&cli, ssl_verify_);
  return from_response(cli.Get(path), "connect failed");
}

HttpResult HttpClient::post(const std::string& url, const std::string& body,
                            const std::string& content_type, int timeout_sec) {
  std::string origin;
  std::string path;
  if (!split_url(url, &origin, &path)) {
    HttpResult out;
    out.error = "invalid url";
    return out;
  }
  httplib::Client cli(origin);
  apply_timeout(&cli, timeout_sec);
  apply_ssl(&cli, ssl_verify_);
  return from_response(cli.Post(path, body, content_type), "connect failed");
}

}  // namespace net
