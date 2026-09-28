// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/session/connection_spec.h"

#include <cstdio>

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

ProviderKind ConnectionSpec::kind_from_info(const SmtDataSourceInfo& info) {
  if (info.unProvider == PROVIDER_SDBD) {
    return ProviderKind::kRemoteSdbd;
  }
  return ProviderKind::kLocalSdbd;
}

SmtDataSourceInfo ConnectionSpec::to_info() const {
  SmtDataSourceInfo info;
  info.unProvider = provider_id;
  info.unType = ds_type;
  copy_fixed(info.szName, MAX_DS_NAME, name);
  copy_fixed(info.szUrl, MAX_URL_LENGTH, url);
  copy_fixed(info.szUID, MAX_UID_NAME, uid);
  copy_fixed(info.szPWD, MAX_PWD_NAME, pwd);

  // file.* and db.* share a union; prefer explicit path/file_name when set.
  if (!path.empty() || !file_name.empty()) {
    copy_fixed(info.file.szPath, MAX_FILE_PATH, path);
    copy_fixed(info.file.szFileName, MAX_FILE_NAME, file_name);
  } else {
    copy_fixed(info.db.szService, MAX_SVR_NAME, service);
    copy_fixed(info.db.szDBName, MAX_DB_NAME, db_name);
  }
  return info;
}

ConnectionSpec ConnectionSpec::from_info(const SmtDataSourceInfo& info) {
  ConnectionSpec spec;
  spec.kind = kind_from_info(info);
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
