// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/provider/local_sdbd_provider.h"

#include "gis/datasource/sdbd/sdbd_dataset.h"

namespace gis {
namespace datasource {

DatasetHandle LocalSdbdProvider::open(const ConnectionSpec& spec) {
  // open_sdbd_dataset calls register_gdal_driver() before GDALOpenEx/Create.
  return DatasetHandle(open_sdbd_dataset(spec));
}

}  // namespace datasource
}  // namespace gis
