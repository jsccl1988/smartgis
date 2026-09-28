// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/provider/remote_sdbd_provider.h"

#include "gis/datasource/provider/impl/sdbd/remote/sdbd_remote_dataset.h"

namespace gis {
namespace datasource {

DatasetHandle RemoteSdbdProvider::open(const ConnectionSpec& spec) {
  return DatasetHandle(open_provider_sdbd_dataset(spec.to_info()));
}

}  // namespace datasource
}  // namespace gis
