// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_PROVIDER_LOCAL_SDBD_PROVIDER_H_
#define GIS_DATASOURCE_PROVIDER_LOCAL_SDBD_PROVIDER_H_

#include "gis/gis_export.h"
#include "gis/datasource/provider/provider.h"

namespace gis {
namespace datasource {

// Opens local SDBD targets (MEM / GPKG / file / etc.) via open_sdbd_dataset.
class GIS_EXPORT LocalSdbdProvider final : public Provider {
 public:
  ProviderKind kind() const override { return ProviderKind::kLocalSdbd; }
  DatasetHandle open(const ConnectionSpec& spec) override;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PROVIDER_LOCAL_SDBD_PROVIDER_H_
