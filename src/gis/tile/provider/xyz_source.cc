// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/provider/xyz_source.h"

#include "base/core/log.h"

#include <fstream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wininet.h>

namespace gis {
namespace tile {
namespace {

constexpr int k_cache_cap = 64;

bool starts_with(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string file_url_to_path(std::string url) {
  if (starts_with(url, "file:///")) {
    url = url.substr(8);
  } else if (starts_with(url, "file://")) {
    url = url.substr(7);
  }
  for (char& ch : url) {
    if (ch == '/') {
      ch = '\\';
    }
  }
  return url;
}

bool is_file_url(std::string_view url) {
  if (starts_with(url, "file:")) {
    return true;
  }
  if (starts_with(url, "http://") || starts_with(url, "https://")) {
    return false;
  }
  return url.find('\\') != std::string_view::npos ||
         (url.size() > 2 && url[1] == ':');
}

bool fetch_wininet(const std::string& url, std::string* body, std::string* error,
                   int timeout_sec) {
  HINTERNET inet = InternetOpenA("smartgis-tile", INTERNET_OPEN_TYPE_PRECONFIG,
                                 nullptr, nullptr, 0);
  if (!inet) {
    if (error) {
      *error = "InternetOpen failed";
    }
    return false;
  }
  DWORD ms = static_cast<DWORD>(timeout_sec > 0 ? timeout_sec : 5) * 1000u;
  InternetSetOptionA(inet, INTERNET_OPTION_CONNECT_TIMEOUT, &ms, sizeof(ms));
  InternetSetOptionA(inet, INTERNET_OPTION_RECEIVE_TIMEOUT, &ms, sizeof(ms));
  DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                INTERNET_FLAG_NO_UI;
  if (starts_with(url, "https://")) {
    flags |= INTERNET_FLAG_SECURE;
  }
  HINTERNET file = InternetOpenUrlA(inet, url.c_str(), nullptr, 0, flags, 0);
  if (!file) {
    if (error) {
      *error = "InternetOpenUrl failed";
    }
    InternetCloseHandle(inet);
    return false;
  }
  DWORD status = 0;
  DWORD status_len = sizeof(status);
  if (HttpQueryInfoA(file, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                     &status, &status_len, nullptr) &&
      (status < 200 || status >= 300)) {
    if (error) {
      *error = "http status " + std::to_string(status);
    }
    InternetCloseHandle(file);
    InternetCloseHandle(inet);
    return false;
  }
  std::string bytes;
  char buf[8192];
  DWORD read = 0;
  while (InternetReadFile(file, buf, sizeof(buf), &read) && read > 0) {
    bytes.append(buf, buf + read);
    read = 0;
  }
  InternetCloseHandle(file);
  InternetCloseHandle(inet);
  if (bytes.empty()) {
    if (error) {
      *error = "empty body";
    }
    return false;
  }
  *body = std::move(bytes);
  return true;
}

bool read_file(const std::string& path, std::string* body, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    if (error) {
      *error = "open failed: " + path;
    }
    return false;
  }
  in.seekg(0, std::ios::end);
  const std::streamoff n = in.tellg();
  if (n < 0) {
    if (error) {
      *error = "size failed: " + path;
    }
    return false;
  }
  in.seekg(0, std::ios::beg);
  body->assign(static_cast<std::size_t>(n), '\0');
  if (n > 0) {
    in.read(body->data(), n);
  }
  if (!in && !in.eof()) {
    if (error) {
      *error = "read failed: " + path;
    }
    body->clear();
    return false;
  }
  return !body->empty();
}

}  // namespace

bool network_available() {
  DWORD flags = 0;
  return InternetGetConnectedState(&flags, 0) != FALSE;
}

bool fetch_url(const std::string& url, std::string* body, std::string* error,
               int timeout_sec) {
  if (!body) {
    return false;
  }
  body->clear();
  if (url.empty()) {
    if (error) {
      *error = "empty url";
    }
    return false;
  }
  if (is_file_url(url)) {
    return read_file(file_url_to_path(url), body, error);
  }
  if (timeout_sec < 1) {
    timeout_sec = 1;
  }
  // Debug httplib is built without OpenSSL, so HTTPS goes through WinINet.
  if (!fetch_wininet(url, body, error, timeout_sec)) {
    LOGGING(LOG_WARNING, "tile fetch failed: %s (%s)", url.c_str(),
            error && !error->empty() ? error->c_str() : "empty");
    return false;
  }
  return true;
}

void XyzSource::set_url_template(std::string url_template) {
  std::lock_guard<std::mutex> lock(mu_);
  url_template_ = std::move(url_template);
  cache_.clear();
}

void XyzSource::set_fetch(TileFetchFn fetch) {
  std::lock_guard<std::mutex> lock(mu_);
  fetch_ = std::move(fetch);
}

bool XyzSource::try_cached(int z, int x, int y, std::string* body) const {
  if (!body) {
    return false;
  }
  std::lock_guard<std::mutex> lock(mu_);
  for (const Entry& entry : cache_) {
    if (entry.index.z == z && entry.index.x == x && entry.index.y == y) {
      *body = entry.body;
      return !body->empty();
    }
  }
  return false;
}

bool XyzSource::get(int z, int x, int y, std::string* body, std::string* error) {
  if (!body) {
    return false;
  }
  if (try_cached(z, x, y, body)) {
    return true;
  }
  std::string tmpl;
  TileFetchFn fetch;
  {
    std::lock_guard<std::mutex> lock(mu_);
    tmpl = url_template_;
    fetch = fetch_;
  }
  if (tmpl.find("{z}") == std::string::npos ||
      tmpl.find("{x}") == std::string::npos ||
      tmpl.find("{y}") == std::string::npos) {
    if (error) {
      *error = "url template needs {z}/{x}/{y}";
    }
    return false;
  }
  const std::string url = format_xyz_url(tmpl, z, x, y);
  std::string bytes;
  std::string fetch_error;
  const bool ok = fetch ? fetch(url, &bytes, &fetch_error)
                        : fetch_url(url, &bytes, &fetch_error, 5);
  if (!ok || bytes.empty()) {
    if (error) {
      *error = fetch_error.empty() ? "tile miss" : fetch_error;
    }
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (static_cast<int>(cache_.size()) >= k_cache_cap) {
      cache_.erase(cache_.begin());
    }
    cache_.push_back(Entry{TileIndex{clamp_zoom(z), x, y}, bytes});
  }
  *body = std::move(bytes);
  return true;
}

void XyzSource::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  cache_.clear();
}

}  // namespace tile
}  // namespace gis
