// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_PROVIDER_PROVIDER_REGISTRY_H_
#define GIS_DATASOURCE_PROVIDER_PROVIDER_REGISTRY_H_

#include <memory>
#include <vector>

#include "gis/gis_export.h"
#include "gis/datasource/provider/provider.h"

namespace gis {
namespace datasource {

// Owns registered Providers and dispatches open by ConnectionSpec::kind.
class GIS_EXPORT ProviderRegistry {
 public:
  ProviderRegistry() = default;
  ProviderRegistry(ProviderRegistry&&) noexcept = default;
  ProviderRegistry& operator=(ProviderRegistry&&) noexcept = default;
  ProviderRegistry(const ProviderRegistry&) = delete;
  ProviderRegistry& operator=(const ProviderRegistry&) = delete;

  void register_provider(std::unique_ptr<Provider> provider);
  Provider* find(ProviderKind kind) const;
  DatasetHandle open(const ConnectionSpec& spec) const;

  // Registers LocalSdbdProvider then RemoteSdbdProvider.
  static ProviderRegistry make_default();

 private:
  std::vector<std::unique_ptr<Provider>> providers_;
};

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PROVIDER_PROVIDER_REGISTRY_H_
