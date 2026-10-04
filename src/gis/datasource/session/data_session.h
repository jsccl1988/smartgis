// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SESSION_DATA_SESSION_H_
#define GIS_DATASOURCE_SESSION_DATA_SESSION_H_

#include "gis/datasource/provider/provider_registry.h"
#include "gis/datasource/session/connection_spec.h"
#include "gis/datasource/session/dataset_handle.h"
#include "gis/gis_export.h"
#include "gis/map/map_layer.h"

namespace gis {
namespace datasource {

// New-tree entry for opening datasets. Not a process singleton; callers own it.
class GIS_EXPORT DataSession {
 public:
  DataSession();  // make_default registry
  explicit DataSession(ProviderRegistry registry);

  DatasetHandle open(const ConnectionSpec& spec);

  // Scratch Memory vector layer (owned MapLayer via adopt_dataset).
  MapLayer create_mem_vector_layer(const char* name = "scratch");

 private:
  ProviderRegistry registry_;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SESSION_DATA_SESSION_H_
