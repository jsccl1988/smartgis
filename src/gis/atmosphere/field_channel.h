// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ATMOSPHERE_FIELD_CHANNEL_H_
#define GIS_ATMOSPHERE_FIELD_CHANNEL_H_

namespace gis {
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
}  // namespace gis

#endif  // GIS_ATMOSPHERE_FIELD_CHANNEL_H_
