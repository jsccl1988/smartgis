// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_CRS_CRS_H_
#define SDB_CRS_CRS_H_

// Spatial reference identity on the GIS model (QGIS
// QgsCoordinateReferenceSystem). v1 stores the SRS name on SmtLayerInfo.szSRS
// in layer.h. Coordinate transforms stay in //src/algorithm/proj (PROJ
// analogue). Do not put projection math in content/public.

namespace gis {

struct CrsId {
  const char* name;
};

}  // namespace gis

#endif  // SDB_CRS_CRS_H_
