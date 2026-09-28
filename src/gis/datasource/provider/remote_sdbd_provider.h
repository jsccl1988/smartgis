// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_PROVIDER_REMOTE_SDBD_PROVIDER_H_
#define GIS_DATASOURCE_PROVIDER_REMOTE_SDBD_PROVIDER_H_

#include "gis/gis_export.h"
#include "gis/datasource/provider/provider.h"

namespace gis {
namespace datasource {

// Opens PROVIDER_SDBD remotes via open_provider_sdbd_dataset.
class GIS_EXPORT RemoteSdbdProvider final : public Provider {
 public:
  ProviderKind kind() const override { return ProviderKind::kRemoteSdbd; }
  DatasetHandle open(const ConnectionSpec& spec) override;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PROVIDER_REMOTE_SDBD_PROVIDER_H_
