// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/provider/provider_registry.h"

#include "gis/datasource/provider/local_sdbd_provider.h"
#include "gis/datasource/provider/remote_sdbd_provider.h"

namespace gis {
namespace datasource {

void ProviderRegistry::register_provider(std::unique_ptr<Provider> provider) {
  if (!provider) {
    return;
  }
  const ProviderKind kind = provider->kind();
  for (auto& existing : providers_) {
    if (existing && existing->kind() == kind) {
      existing = std::move(provider);
      return;
    }
  }
  providers_.push_back(std::move(provider));
}

Provider* ProviderRegistry::find(ProviderKind kind) const {
  for (const auto& provider : providers_) {
    if (provider && provider->kind() == kind) {
      return provider.get();
    }
  }
  return nullptr;
}

DatasetHandle ProviderRegistry::open(const ConnectionSpec& spec) const {
  Provider* provider = find(spec.kind);
  if (!provider) {
    return DatasetHandle{};
  }
  return provider->open(spec);
}

ProviderRegistry ProviderRegistry::make_default() {
  ProviderRegistry registry;
  registry.register_provider(std::make_unique<LocalSdbdProvider>());
  registry.register_provider(std::make_unique<RemoteSdbdProvider>());
  return registry;
}

}  // namespace datasource
}  // namespace gis
