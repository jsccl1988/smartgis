// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_PROVIDER_PROVIDER_H_
#define GIS_DATASOURCE_PROVIDER_PROVIDER_H_

#include "gis/gis_export.h"
#include "gis/datasource/session/connection_spec.h"
#include "gis/datasource/session/dataset_handle.h"

namespace gis {
namespace datasource {

// Abstract adapter that opens a dataset for one ProviderKind.
class GIS_EXPORT Provider {
 public:
  virtual ~Provider() = default;

  virtual ProviderKind kind() const = 0;
  virtual DatasetHandle open(const ConnectionSpec& spec) = 0;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PROVIDER_PROVIDER_H_
