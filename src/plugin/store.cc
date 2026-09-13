// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/store.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "net/http/http.h"
#include "plugin/manifest.h"
#include "plugin/official_key.h"
#include "plugin/registry.h"
#include "plugin/signature.h"

namespace plugin {
namespace {

namespace fs = std::filesystem;

uint16_t rd16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t rd32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0] | (p[1] << 8) | (p[2] << 16) |
                               (p[3] << 24));
}

bool read_file(const std::string& path, std::string* out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>());
  return true;
}

bool write_file(const fs::path& path, std::string_view data) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out.write(data.data(), static_cast<std::streamsize>(data.size()));
  return static_cast<bool>(out);
}

bool is_safe_zip_name(std::string name) {
  for (char& c : name) {
    if (c == '\\') {
      c = '/';
    }
  }
  if (name.empty() || name[0] == '/' || name.find(':') != std::string::npos) {
    return false;
  }
  if (name.find("..") != std::string::npos) {
    return false;
  }
  return true;
}

struct ZipEntry {
  std::string name;
  std::string data;
};

bool parse_store_zip(std::string_view bytes, std::vector<ZipEntry>* out,
                     std::string* err) {
  const auto* p = reinterpret_cast<const uint8_t*>(bytes.data());
  size_t n = bytes.size();
  size_t off = 0;
  while (off + 30 <= n) {
    if (rd32(p + off) != 0x04034b50u) {
      break;
    }
    const uint16_t method = rd16(p + off + 8);
    const uint32_t comp = rd32(p + off + 18);
    const uint32_t uncomp = rd32(p + off + 22);
    const uint16_t namelen = rd16(p + off + 26);
    const uint16_t extralen = rd16(p + off + 28);
    if (off + 30 + namelen + extralen + comp > n) {
      if (err) {
        *err = "truncated zip";
      }
      return false;
    }
    std::string name(reinterpret_cast<const char*>(p + off + 30), namelen);
    if (!is_safe_zip_name(name)) {
      if (err) {
        *err = "kBadZip";
      }
      return false;
    }
    if (method != 0) {
      if (err) {
        *err = "deflate not supported";
      }
      return false;
    }
    if (comp != uncomp) {
      if (err) {
        *err = "size mismatch";
      }
      return false;
    }
    ZipEntry e;
    e.name = std::move(name);
    e.data.assign(reinterpret_cast<const char*>(p + off + 30 + namelen + extralen),
                  comp);
    out->push_back(std::move(e));
    off += 30 + namelen + extralen + comp;
  }
  return !out->empty();
}

bool json_quoted(const std::string& body, const char* key, std::string* out) {
  if (!out || !key) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = body.find(needle);
  if (pos == std::string::npos) {
    return false;
  }
  const size_t colon = body.find(':', pos + needle.size());
  if (colon == std::string::npos) {
    return false;
  }
  const size_t q1 = body.find('"', colon + 1);
  if (q1 == std::string::npos) {
    return false;
  }
  std::string value;
  for (size_t i = q1 + 1; i < body.size(); ++i) {
    if (body[i] == '\\' && i + 1 < body.size()) {
      value.push_back(body[i + 1]);
      ++i;
      continue;
    }
    if (body[i] == '"') {
      *out = std::move(value);
      return true;
    }
    value.push_back(body[i]);
  }
  return false;
}

bool find_index_plugin(const std::string& body, const std::string& id,
                       std::string* download, std::string* sha,
                       std::string* sig) {
  const std::string needle = std::string("\"id\"") ;
  size_t pos = 0;
  while ((pos = body.find(needle, pos)) != std::string::npos) {
    std::string found;
    const std::string slice = body.substr(pos, 4000);
    if (!json_quoted(slice, "id", &found) || found != id) {
      pos += needle.size();
      continue;
    }
    if (!json_quoted(slice, "download_url", download) ||
        !json_quoted(slice, "sha256", sha) ||
        !json_quoted(slice, "sig_url", sig)) {
      return false;
    }
    return true;
  }
  return false;
}

std::string file_url_path(std::string url) {
  const std::string kFile = "file://";
  if (url.compare(0, kFile.size(), kFile) != 0) {
    return {};
  }
  url.erase(0, kFile.size());
  if (url.compare(0, 10, "localhost/") == 0) {
    url.erase(0, 9);
  }
  if (url.size() >= 3 && url[0] == '/' &&
      ((url[1] >= 'A' && url[1] <= 'Z') || (url[1] >= 'a' && url[1] <= 'z')) &&
      url[2] == ':') {
    url.erase(0, 1);
  }
  for (char& c : url) {
    if (c == '/') {
      c = '\\';
    }
  }
  return url;
}

bool fetch_url(const std::string& url, std::string* body, std::string* err) {
  if (url.compare(0, 7, "file://") == 0) {
    const std::string path = file_url_path(url);
    if (path.empty() || !read_file(path, body)) {
      if (err) {
        *err = "file:// read failed";
      }
      return false;
    }
    return true;
  }
  net::HttpClient client;
  const net::HttpResult r = client.get(url);
  if (!r.ok) {
    if (err) {
      *err = r.error.empty() ? "http failed" : r.error;
    }
    return false;
  }
  *body = r.body;
  return true;
}

void unwrap_single_dir(std::vector<ZipEntry>* files) {
  if (files->empty()) {
    return;
  }
  std::string prefix;
  for (const ZipEntry& e : *files) {
    const size_t slash = e.name.find('/');
    if (slash == std::string::npos) {
      return;
    }
    const std::string top = e.name.substr(0, slash + 1);
    if (prefix.empty()) {
      prefix = top;
    } else if (prefix != top) {
      return;
    }
  }
  for (ZipEntry& e : *files) {
    e.name = e.name.substr(prefix.size());
  }
}

}  // namespace

Store::Store(Registry* registry, std::string user_plugins_dir)
    : registry_(registry), user_dir_(std::move(user_plugins_dir)) {}

const std::string& Store::last_error() const {
  return last_error_;
}

bool Store::fail(StoreError code, const char* msg) {
  error_code_ = code;
  last_error_ = msg;
  return false;
}

bool Store::refresh_index(const std::string& url) {
  last_error_.clear();
  error_code_ = StoreError::kNone;
  std::string body;
  std::string err;
  if (!fetch_url(url, &body, &err)) {
    return fail(StoreError::kHttp, err.empty() ? "index fetch failed" : err.c_str());
  }
  if (body.find("\"api_version\"") == std::string::npos ||
      (body.find(": 1") == std::string::npos &&
       body.find(":1") == std::string::npos)) {
    return fail(StoreError::kApiMismatch, "index api_version must be 1");
  }
  index_body_ = std::move(body);
  return true;
}

bool Store::install_from_index(const std::string& id) {
  last_error_.clear();
  error_code_ = StoreError::kNone;
  if (id.empty() || index_body_.empty()) {
    return fail(StoreError::kHttp, "no index or empty id");
  }
  std::string download;
  std::string sha;
  std::string sig_url;
  if (!find_index_plugin(index_body_, id, &download, &sha, &sig_url)) {
    return fail(StoreError::kHttp, "plugin not in index");
  }
  std::string zip;
  std::string ferr;
  if (!fetch_url(download, &zip, &ferr)) {
    return fail(StoreError::kHttp,
                ferr.empty() ? "zip fetch failed" : ferr.c_str());
  }
  const std::string got = SignatureVerifier::sha256_hex(zip);
  if (got != sha) {
    return fail(StoreError::kBadHash, "sha256 mismatch");
  }
  std::string sig;
  if (!fetch_url(sig_url, &sig, &ferr) || sig.size() != 64) {
    return fail(StoreError::kBadSignature, "sig fetch failed");
  }
  const fs::path tmp = fs::temp_directory_path() / ("smt_store_" + id);
  fs::create_directories(tmp);
  const fs::path zip_path = tmp / "plugin.zip";
  const fs::path sig_path = tmp / "plugin.zip.sig";
  if (!write_file(zip_path, zip) || !write_file(sig_path, sig)) {
    return fail(StoreError::kHttp, "temp write failed");
  }
  const bool ok = install_zip(zip_path.string(), sig_path.string(), true);
  std::error_code ec;
  fs::remove_all(tmp, ec);
  return ok;
}

bool Store::install_zip(const std::string& zip_path, const std::string& sig_path,
                        bool signed_official) {
  last_error_.clear();
  error_code_ = StoreError::kNone;
  std::string zip;
  if (!read_file(zip_path, &zip)) {
    return fail(StoreError::kBadZip, "cannot read zip");
  }
  if (zip.size() < 4 || zip[0] != 'P' || zip[1] != 'K') {
    return fail(StoreError::kBadZip, "not-a-zip");
  }
  if (signed_official) {
    std::string sig;
    if (!read_file(sig_path, &sig) || sig.size() != 64) {
      return fail(StoreError::kBadSignature, "missing or short .sig");
    }
    uint8_t sig64[64];
    memcpy(sig64, sig.data(), 64);
    if (!SignatureVerifier::verify(zip, sig64, kOfficialPublicKey)) {
      return fail(StoreError::kBadSignature, "bad signature");
    }
  }
  std::vector<ZipEntry> files;
  std::string zerr;
  if (!parse_store_zip(zip, &files, &zerr)) {
    return fail(StoreError::kBadZip, zerr.empty() ? "kBadZip" : zerr.c_str());
  }
  unwrap_single_dir(&files);
  const ZipEntry* manifest = nullptr;
  for (const ZipEntry& e : files) {
    if (e.name == "plugin.json") {
      manifest = &e;
      break;
    }
  }
  if (!manifest) {
    return fail(StoreError::kBadManifest, "missing plugin.json");
  }
  Manifest m;
  std::string perr;
  if (!parse_manifest(manifest->data, &m, &perr)) {
    return fail(StoreError::kBadManifest, perr.c_str());
  }
  const fs::path dest = fs::path(user_dir_) / m.id;
  std::error_code ec;
  fs::remove_all(dest, ec);
  for (const ZipEntry& e : files) {
    if (e.name.empty() || e.name.back() == '/') {
      continue;
    }
    if (!write_file(dest / e.name, e.data)) {
      fs::remove_all(dest, ec);
      return fail(StoreError::kBadZip, "write failed");
    }
  }
  if (registry_) {
    const TrustClass trust = signed_official ? TrustClass::kSignedOfficial
                                             : TrustClass::kDenied;
    registry_->add_manifest(m, trust);
    if (PluginRecord* rec =
            const_cast<PluginRecord*>(registry_->find(m.id))) {
      rec->directory = dest.string();
    }
  }
  return true;
}

bool Store::uninstall(const std::string& id) {
  last_error_.clear();
  if (id.empty()) {
    return fail(StoreError::kBadManifest, "empty id");
  }
  const fs::path dest = fs::path(user_dir_) / id;
  std::error_code ec;
  fs::remove_all(dest, ec);
  if (registry_) {
    registry_->unload(id, nullptr);
  }
  return true;
}

}  // namespace plugin
