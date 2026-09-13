// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_FEATURE_H
#define _GIS_FEATURE_H

#include "sdb/feature/attribute.h"
#include "sdb/gis_export.h"
#include "base/style/style.h"

class OGRGeometry;
class OGRLayer;

namespace geo {
class Grid;
class Tin;
}

namespace sdb {

// Product feature kind. Storage and map documents use OGRFeature; this enum
// only labels layer geometry / extra fields (anno, tin, grid).
enum SmtFeatureType {
  SmtFtDot,
  SmtFtAnno,
  SmtFtChildImage,
  SmtFtCurve,
  SmtFtSurface,
  SmtFtGrid,
  SmtFtTin,
  SmtFtUnknown
};

// Leftover tool/UI holder until those TUs speak OGRFeature directly.
class GIS_EXPORT SmtFeature {
 public:
  SmtFeature();
  ~SmtFeature();

  long GetID() const { return id_; }
  void SetID(long id) { id_ = id; }

  SmtFeatureType GetFeatureType() const { return type_; }
  void SetFeatureType(SmtFeatureType type);

  OGRGeometry* GetGeometryRef() { return geom_; }
  const OGRGeometry* GetGeometryRef() const { return geom_; }
  OGRGeometry* getGeometryRef() { return geom_; }
  void SetGeometryDirectly(OGRGeometry* geom);
  void SetGeometry(OGRGeometry* geom);
  void SetGeometry(geo::Grid* grid);
  void SetGeometry(geo::Tin* tin);

  SmtAttribute* GetAttributeRef() { return att_; }
  const SmtAttribute* GetAttributeRef() const { return att_; }

  base::SmtStyle* get_style() { return style_; }
  void SetStyle(base::SmtStyle* style);
  void SetStyle(const char* style_name);

  int GetFieldIndexByName(const char* name);
  int SetFieldValue(int index, int nValue);
  int SetFieldValue(int index, double dfValue);
  int SetFieldValue(int index, const char* pszValue);

 private:
  long id_ = 0;
  SmtFeatureType type_ = SmtFtUnknown;
  OGRGeometry* geom_ = nullptr;
  geo::Grid* grid_ = nullptr;
  geo::Tin* tin_ = nullptr;
  SmtAttribute* att_ = nullptr;
  base::SmtStyle* style_ = nullptr;
};

bool leftover_append_feature(OGRLayer* layer, SmtFeature* feature);

}  // namespace sdb

#endif  // _GIS_FEATURE_H
