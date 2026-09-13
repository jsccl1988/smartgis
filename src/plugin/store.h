// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_STORE_H_
#define PLUGIN_STORE_H_

#include <string>

namespace plugin {

class Registry;

enum class StoreError {
  kNone,
  kBadZip,
  kBadHash,
  kBadSignature,
  kBadManifest,
  kApiMismatch,
  kHttp,
};

class Store {
 public:
  Store(Registry* registry, std::string user_plugins_dir);
  bool refresh_index(const std::string& url);
  bool install_from_index(const std::string& id);
  bool install_zip(const std::string& zip_path, const std::string& sig_path,
                   bool signed_official);
  bool uninstall(const std::string& id);
  const std::string& last_error() const;
  StoreError last_error_code() const { return error_code_; }

 private:
  bool fail(StoreError code, const char* msg);

  Registry* registry_ = nullptr;
  std::string user_dir_;
  std::string last_error_;
  StoreError error_code_ = StoreError::kNone;
  std::string index_body_;
};

}  // namespace plugin

#endif  // PLUGIN_STORE_H_
