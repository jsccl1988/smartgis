// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/browser/report_browser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace plugin {
namespace {

std::string normalize_path(std::string_view in_path) {
  std::error_code ec;
  const std::filesystem::path fs_path{std::string(in_path)};
  std::filesystem::path abs = std::filesystem::absolute(fs_path, ec);
  if (!ec) {
    abs = std::filesystem::weakly_canonical(abs, ec);
  }
  if (ec) {
    abs = fs_path;
  }
  std::string out = abs.generic_string();
  for (char& c : out) {
    if (c == '\\') {
      c = '/';
    }
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

bool starts_with_path(std::string_view path, std::string_view root) {
  if (root.empty() || path.size() < root.size()) {
    return false;
  }
  if (path.compare(0, root.size(), root) != 0) {
    return false;
  }
  return path.size() == root.size() || path[root.size()] == '/';
}

}  // namespace

void ReportBrowser::add_allowed_root(std::string root) {
  if (root.empty()) {
    return;
  }
  allowed_roots_.push_back(normalize_path(root));
}

void ReportBrowser::clear_allowed_roots() {
  allowed_roots_.clear();
}

bool ReportBrowser::is_path_allowed(std::string_view path) const {
  if (path.empty()) {
    return false;
  }
  if (allowed_roots_.empty()) {
    return true;
  }
  const std::string norm = normalize_path(path);
  for (const std::string& root : allowed_roots_) {
    if (starts_with_path(norm, root)) {
      return true;
    }
  }
  return false;
}

std::string ReportBrowser::file_url_for_report_dir(std::string_view report_dir) {
  if (report_dir.empty()) {
    return {};
  }
  std::error_code ec;
  const std::filesystem::path dir{std::string(report_dir)};
  const std::filesystem::path index = dir / "index.html";
  if (!std::filesystem::exists(index, ec)) {
    return {};
  }
  std::filesystem::path abs = std::filesystem::weakly_canonical(index, ec);
  if (ec) {
    abs = std::filesystem::absolute(index, ec);
  }
  std::string path_utf8 = abs.string();
  for (char& c : path_utf8) {
    if (c == '\\') {
      c = '/';
    }
  }
  // file:///C:/... on Windows
  std::ostringstream url;
  url << "file:///";
  url << path_utf8;
  return url.str();
}

bool ReportBrowser::is_blocked_navigation_url(std::string_view url) {
  std::string lower(url);
  for (char& c : lower) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  auto is_scheme = [&](std::string_view scheme) {
    return lower.size() >= scheme.size() &&
           lower.compare(0, scheme.size(), scheme) == 0;
  };
  if (is_scheme("http:") || is_scheme("https:") || is_scheme("ws:") ||
      is_scheme("wss:") || is_scheme("ftp:")) {
    return true;
  }
  return false;
}

}  // namespace plugin
