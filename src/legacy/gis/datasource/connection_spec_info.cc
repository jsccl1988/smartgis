// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/gis/datasource/connection_spec_info.h"

#include <cstddef>
#include <cstdio>
#include <string>

namespace gis {
namespace datasource {
namespace {

void copy_fixed(char* dest, size_t dest_len, const std::string& src) {
  if (!dest || dest_len == 0) {
    return;
  }
  if (src.empty()) {
    dest[0] = '\0';
    return;
  }
  sprintf_s(dest, dest_len, "%s", src.c_str());
}

}  // namespace

ProviderKind provider_kind_from_info(const DataSourceInfo& info) {
  if (info.unProvider == PROVIDER_SDBD) {
    return ProviderKind::kRemoteSdbd;
  }
  return ProviderKind::kLocalSdbd;
}

DataSourceInfo connection_spec_to_info(const ConnectionSpec& spec) {
  DataSourceInfo info;
  info.unProvider = spec.provider_id;
  info.unType = spec.ds_type;
  copy_fixed(info.szName, MAX_DS_NAME, spec.name);
  copy_fixed(info.szUrl, MAX_URL_LENGTH, spec.url);
  copy_fixed(info.szUID, MAX_UID_NAME, spec.uid);
  copy_fixed(info.szPWD, MAX_PWD_NAME, spec.pwd);

  // file.* and db.* share a union; prefer explicit path/file_name when set.
  if (!spec.path.empty() || !spec.file_name.empty()) {
    copy_fixed(info.file.szPath, MAX_FILE_PATH, spec.path);
    copy_fixed(info.file.szFileName, MAX_FILE_NAME, spec.file_name);
  } else {
    copy_fixed(info.db.szService, MAX_SVR_NAME, spec.service);
    copy_fixed(info.db.szDBName, MAX_DB_NAME, spec.db_name);
  }
  return info;
}

ConnectionSpec connection_spec_from_info(const DataSourceInfo& info) {
  ConnectionSpec spec;
  spec.kind = provider_kind_from_info(info);
  spec.provider_id = info.unProvider;
  spec.ds_type = info.unType;
  spec.name = info.szName;
  spec.url = info.szUrl;
  spec.uid = info.szUID;
  spec.pwd = info.szPWD;
  // Union overlay: expose both views so callers can read either pair.
  spec.path = info.file.szPath;
  spec.file_name = info.file.szFileName;
  spec.service = info.db.szService;
  spec.db_name = info.db.szDBName;
  return spec;
}

}  // namespace datasource
}  // namespace gis
