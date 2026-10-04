// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_GIS_LAYER_LAYER_H_
#define SMT_LEGACY_GIS_LAYER_LAYER_H_

#include <cstddef>
#include <cstdio>
#include <cstring>

#include "gis/datasource/ogr/ogr_feature_kind.h"
#include "gis/datasource/session/connection_spec.h"
#include "gis/envelope.h"
#include "gis/feature/feature.h"
#include "gis/map/layer_kind.h"
#include "legacy/core/types/types.h"
#include "ogrsf_frmts.h"

using namespace base;

#define MAX_DS_NAME MAX_NAME_LENGTH
#define MAX_SVR_NAME MAX_FILE_PATH
#define MAX_DB_NAME MAX_NAME_LENGTH
#define MAX_FILE_NAME MAX_NAME_LENGTH
#define MAX_UID_NAME MAX_NAME_LENGTH
#define MAX_PWD_NAME MAX_NAME_LENGTH
#define MAX_SPATIALINDEX_NAME MAX_NAME_LENGTH
#define MAX_LAYER_NAME MAX_NAME_LENGTH
#define MAX_URL_LENGTH MAX_FILE_PATH
#define MAX_LAYER_ARCHIVE_NAME (MAX_FILE_PATH + MAX_NAME_LENGTH)
#define MAX_LAYER_SRS_NAME MAX_NAME_LENGTH

class GDALDataset;

namespace gis {

// Leftover catalog kinds (unFeatureType). Not product Feature geometry.
enum FeatureType {
  FtDot = 0,
  FtAnno,
  FtChildImage,
  FtCurve,
  FtSurface,
  FtGrid,
  FtTin,
  FtUnknown
};

enum FeatureTypeExt {
  LayerRas = FtUnknown + 1,
  LayerTile = FtUnknown + 2,
};

}  // namespace gis

inline gis::FeatureType leftover_layer_feature_type(OGRLayer* layer) {
  if (!layer) {
    return gis::FtUnknown;
  }
  const gis::VectorSchema schema = gis::datasource::layer_vector_schema(layer);
  switch (schema) {
    case gis::VectorSchema::kAnno:
      return gis::FtAnno;
    case gis::VectorSchema::kChildImage:
      return gis::FtChildImage;
    case gis::VectorSchema::kGrid:
      return gis::FtGrid;
    case gis::VectorSchema::kTin:
      return gis::FtTin;
    default:
      break;
  }
  switch (wkbFlatten(layer->GetGeomType())) {
    case wkbPoint:
      return gis::FtDot;
    case wkbLineString:
    case wkbMultiLineString:
      return gis::FtCurve;
    case wkbPolygon:
    case wkbLinearRing:
      return gis::FtSurface;
    case wkbMultiPolygon:
    case wkbTIN:
    case wkbTriangle:
      return gis::FtTin;
    case wkbMultiPoint:
      return gis::FtDot;
    default:
      return gis::FtUnknown;
  }
}

inline OGRwkbGeometryType leftover_feature_wkb(gis::FeatureType ft) {
  switch (ft) {
    case gis::FtDot:
    case gis::FtAnno:
      return wkbPoint;
    case gis::FtCurve:
      return wkbLineString;
    case gis::FtSurface:
      return wkbPolygon;
    case gis::FtTin:
      return wkbTIN;
    case gis::FtGrid:
      return wkbMultiPoint;
    case gis::FtChildImage:
      return wkbNone;
    default:
      return wkbUnknown;
  }
}

inline const char* layer_feature_type_name(uint ftType) {
  switch (ftType) {
    case gis::FtDot:
      return "DotFcls";
    case gis::FtChildImage:
      return "ChildImageFcls";
    case gis::FtAnno:
      return "AnnoFcls";
    case gis::FtCurve:
      return "CurveFcls";
    case gis::FtSurface:
      return "SurfaceFcls";
    case gis::FtGrid:
      return "GridFcls";
    case gis::FtTin:
      return "TinFcls";
    case gis::FeatureTypeExt::LayerRas:
      return "Raster";
    default:
      return "UnKnown";
  }
}

namespace gis {

enum FetchType { FETCH_ALL, FETCH_FILTER };

// Leftover 2010 raster/tile virtual ABI. Product map slots are MapLayer.
class Layer {
 public:
  explicit Layer(GDALDataset* owner = nullptr)
      : m_pOwnerDs(owner), m_bIsVisible(true), m_bOpen(false) {
    m_szLayerName[0] = '\0';
    m_szSRS[0] = '\0';
  }

  virtual ~Layer() = default;

  GDALDataset* GetDataset() { return m_pOwnerDs; }
  const GDALDataset* GetDataset() const { return m_pOwnerDs; }
  GDALDataset* GetDataSource() { return m_pOwnerDs; }
  const GDALDataset* GetDataSource() const { return m_pOwnerDs; }

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(FetchType type = FETCH_ALL) = 0;

  bool IsOpen() const { return m_bOpen; }

  void get_envelope(Envelope& env) const {
    memcpy(&env, &m_lyrEnv, sizeof(Envelope));
  }
  virtual void CalEnvelope() {}

  virtual long StartTransaction() { return SMT_ERR_NONE; }
  virtual long CommitTransaction() { return SMT_ERR_NONE; }
  virtual long RollbackTransaction() { return SMT_ERR_NONE; }

  void SetLayerName(const char* szName) {
    sprintf_s(m_szLayerName, MAX_LAYER_NAME, szName);
  }
  const char* GetLayerName() const { return m_szLayerName; }

  void SetSRS(const char* szSRS) {
    sprintf_s(m_szSRS, MAX_LAYER_SRS_NAME, szSRS);
  }
  const char* GetSRS() const { return m_szSRS; }

  virtual void SetLayerRect(const fRect& lyrRect) = 0;
  virtual LayerType GetLayerType() const = 0;

  void SetVisible(bool bVisible = true) { m_bIsVisible = bVisible; }
  bool IsVisible() const { return m_bIsVisible; }

 protected:
  GDALDataset* m_pOwnerDs;
  Envelope m_lyrEnv;
  char m_szLayerName[MAX_LAYER_NAME];
  char m_szSRS[MAX_LAYER_SRS_NAME];
  bool m_bIsVisible;
  bool m_bOpen;
};

class RasterLayer : public Layer {
 public:
  explicit RasterLayer(GDALDataset* owner = nullptr) : Layer(owner) {}
  ~RasterLayer() override = default;

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(FetchType type = FETCH_ALL) = 0;

  virtual long CreaterRaster(const char* pRasterBuf, long lRasterBufSize,
                             const fRect& fLocRect, long lImageCode) = 0;
  virtual long SetRasterRect(const fRect& fLocRect) = 0;
  virtual long GetRaster(char*& pRasterBuf, long& lRasterBufSize,
                         fRect& fLocRect, long& lImageCode) const = 0;
  virtual long GetRasterNoClone(char*& pRasterBuf, long& lRasterBufSize,
                                fRect& fLocRect, long& lImageCode) const = 0;
  virtual long GetRasterRect(fRect& fLocRect) const = 0;

  void SetLayerRect(const fRect& lyrRect) override {
    SetRasterRect(lyrRect);
    m_lyrEnv.MinX = lyrRect.lb.x;
    m_lyrEnv.MinY = lyrRect.lb.y;
    m_lyrEnv.MaxX = lyrRect.rt.x;
    m_lyrEnv.MaxY = lyrRect.rt.y;
  }

  LayerType GetLayerType() const override { return LYR_RASTER; }
};

class TileLayer : public Layer {
 public:
  explicit TileLayer(GDALDataset* owner = nullptr)
      : Layer(owner), m_lImageCode(-1) {}
  ~TileLayer() override = default;

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(FetchType type = FETCH_ALL) = 0;
  virtual void CalEnvelope() = 0;

  virtual int GetTileCount() const = 0;
  virtual void MoveFirst() const = 0;
  virtual void MoveNext() const = 0;
  virtual void MoveLast() const = 0;
  virtual void Delete() = 0;
  virtual bool IsEnd() const = 0;
  virtual void DeleteAll() = 0;

  virtual long AppendTile(const SmtTile* pTile, bool bClone = false) = 0;
  virtual long UpdateTile(const SmtTile* pTile) = 0;
  virtual long DeleteTile(const SmtTile* pTile) = 0;
  virtual SmtTile* GetTile() const = 0;
  virtual SmtTile* GetTile(int index) const = 0;
  virtual SmtTile* GetTileByID(uint unID) const = 0;

  LayerType GetLayerType() const override { return LYR_TITLE; }

  void SetLayerRect(const fRect& lyrRect) override {
    m_lyrEnv.MinX = lyrRect.lb.x;
    m_lyrEnv.MinY = lyrRect.lb.y;
    m_lyrEnv.MaxX = lyrRect.rt.x;
    m_lyrEnv.MaxY = lyrRect.rt.y;
  }

 protected:
  long m_lImageCode;
};

struct SpIdxInfo {
  char szName[MAX_SPATIALINDEX_NAME];
  uint unType;
  char szOnwerLayerName[MAX_LAYER_NAME];

  SpIdxInfo() {
    szName[0] = '\0';
    unType = 0;
    szOnwerLayerName[0] = '\0';
  }
};

struct LayerInfo {
  char szName[MAX_LAYER_NAME];
  char szArchiveName[MAX_LAYER_ARCHIVE_NAME];
  char szSRS[MAX_LAYER_SRS_NAME];
  uint unFeatureType;
  uint unSIType;
  fRect lyrRect;

  LayerInfo() {
    szName[0] = '\0';
    szArchiveName[0] = '\0';
    szSRS[0] = '\0';
    unFeatureType = 0;
    unSIType = 0;
  }
};

struct DataSourceInfo {
  uint unProvider;
  uint unType;
  char szName[MAX_DS_NAME];
  char szUrl[MAX_URL_LENGTH];

  union {
    struct {
      char szService[MAX_SVR_NAME];
      char szDBName[MAX_DB_NAME];
    } db;

    struct {
      char szPath[MAX_FILE_PATH];
      char szFileName[MAX_FILE_NAME];
    } file;
  };

  char szUID[MAX_UID_NAME];
  char szPWD[MAX_PWD_NAME];

  DataSourceInfo() {
    unType = 0;
    unProvider = 0;
    szName[0] = '\0';
    szUrl[0] = '\0';
    db.szService[0] = '\0';
    db.szDBName[0] = '\0';
    szUID[0] = '\0';
    szPWD[0] = '\0';
  }
};

class CatalogSource {
 public:
  CatalogSource() = default;
  CatalogSource(std::nullptr_t) : ds_(nullptr) {}
  CatalogSource(int) : ds_(nullptr) {}
  CatalogSource(GDALDataset* ds) : ds_(ds) {}

  explicit operator bool() const { return ds_ != nullptr; }
  bool operator==(std::nullptr_t) const { return ds_ == nullptr; }
  bool operator!=(std::nullptr_t) const { return ds_ != nullptr; }

  bool Open() { return ds_ != nullptr; }
  void Close() {}

  GDALDataset* dataset() const { return ds_; }
  operator GDALDataset*() const { return ds_; }

  const char* GetName() const {
    return (ds_ && ds_->GetDescription()) ? ds_->GetDescription() : "";
  }
  const char* GetUrl() const { return GetName(); }

  int GetLayerCount() const { return ds_ ? ds_->GetLayerCount() : 0; }

  void GetInfo(DataSourceInfo& info) const {
    info = DataSourceInfo();
    if (ds_ && ds_->GetDescription()) {
      sprintf_s(info.szName, MAX_DS_NAME, "%s", ds_->GetDescription());
    }
  }

  void GetLayerInfo(LayerInfo& out, int index) const {
    out = LayerInfo();
    if (!ds_ || index < 0 || index >= ds_->GetLayerCount()) {
      return;
    }
    fill_layer_info(&out, ds_->GetLayer(index));
  }

  void GetLayerInfo(LayerInfo& out, const char* name) const {
    out = LayerInfo();
    if (!ds_ || name == nullptr) {
      return;
    }
    fill_layer_info(&out, ds_->GetLayerByName(name));
  }

  OGRLayer* OpenVectorLayer(const char* name) {
    return (ds_ && name) ? ds_->GetLayerByName(name) : nullptr;
  }

  OGRLayer* CreateVectorLayer(const char* name, const fRect&,
                              FeatureType type) {
    if (!ds_ || !name) {
      return nullptr;
    }
    return ds_->CreateLayer(name, nullptr, leftover_feature_wkb(type), nullptr);
  }

  RasterLayer* CreateRasterLayer(const char*, const fRect&, int) {
    return nullptr;
  }

  bool DeleteVectorLayer(const char* name) {
    if (!ds_ || !name) {
      return false;
    }
    const int n = ds_->GetLayerCount();
    for (int i = 0; i < n; ++i) {
      OGRLayer* lyr = ds_->GetLayer(i);
      if (lyr && lyr->GetName() && std::strcmp(lyr->GetName(), name) == 0) {
        return ds_->DeleteLayer(i) == OGRERR_NONE;
      }
    }
    return false;
  }

  RasterLayer* OpenRasterLayer(const char* /*name*/) { return nullptr; }

  static const char* GetLayerFeatureTypeName(uint ftType) {
    return layer_feature_type_name(ftType);
  }

 private:
  static void fill_layer_info(LayerInfo* out, OGRLayer* lyr) {
    if (!out || !lyr) {
      return;
    }
    sprintf_s(out->szName, MAX_LAYER_NAME, "%s", lyr->GetName());
    sprintf_s(out->szArchiveName, MAX_LAYER_ARCHIVE_NAME, "%s", lyr->GetName());
    out->unFeatureType = ::leftover_layer_feature_type(lyr);
  }

  GDALDataset* ds_ = nullptr;
};

inline bool operator==(std::nullptr_t, const CatalogSource& ds) { return !ds; }

}  // namespace gis

#endif  // SMT_LEGACY_GIS_LAYER_LAYER_H_
