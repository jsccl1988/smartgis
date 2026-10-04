// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/provider/remote_sdbd_provider.h"

#include "gis/datasource/sdbd/sdbd_remote_dataset.h"

namespace gis {
namespace datasource {

DatasetHandle RemoteSdbdProvider::open(const ConnectionSpec& spec) {
  return DatasetHandle(open_provider_sdbd_dataset(spec));
}

}  // namespace datasource
}  // namespace gis
