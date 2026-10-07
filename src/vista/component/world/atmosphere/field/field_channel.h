// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_FIELD_CHANNEL_H_
#define VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_FIELD_CHANNEL_H_

namespace vista {
namespace atmosphere {

// Shared atmosphere / ocean / cloud field channels on FieldStore.
enum class FieldChannel {
  kWindU = 0,
  kWindV,
  kWaveHs,
  kWaveDir,
  kCloudCover,
  kCloudBase,
  kCloudTop,
  kSeaMask,
  kCount,
};

// Provenance of a FieldLayer; External and Procedural mix by priority + mask.
enum class FieldSourceKind {
  kExternal = 0,
  kProcedural,
};

}  // namespace atmosphere
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_ATMOSPHERE_FIELD_FIELD_CHANNEL_H_
