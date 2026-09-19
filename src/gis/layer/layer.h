// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_SDE_H
#define _GIS_SDE_H

#include <cstddef>
#include <cstring>

#include "algorithm/geo/geometry.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/ogr_feature_codec.h"
#include "gis/feature/feature.h"

using namespace base;
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

// Leftover name for vector layers after the OGR cut-over.
using SmtVectorLayer = OGRLayer;

// Thin leftover alias; authoritative mapping lives in feature_type_of.
inline gis::SmtFeatureType leftover_layer_feature_type(OGRLayer* layer) {
  return gis::datasource::feature_type_of(layer);
}

inline OGRwkbGeometryType leftover_feature_wkb(gis::SmtFeatureType ft) {
  switch (ft) {
    case gis::SmtFtDot:
    case gis::SmtFtAnno:
      return wkbPoint;
    case gis::SmtFtCurve:
      return wkbLineString;
    case gis::SmtFtSurface:
      return wkbPolygon;
    case gis::SmtFtTin:
      return wkbTIN;
    case gis::SmtFtGrid:
      return wkbMultiPoint;
    default:
      return wkbUnknown;
  }
}

namespace gis {

enum eSmtDBProvider {
  PROVIDER_ACCESS,
  PROVIDER_SQLSERVER,
  PROVIDER_ORACLE,
  PROVIDER_MYSQL,
  PROVIDER_POSTGRES,
  PROVIDER_GPKG,
  PROVIDER_SPATIALITE,
  PROVIDER_SDBD,  // WSL mogu sdbd (HTTP :8021 / FnRPC :9032); not local Handler
};

enum eSmtFileProvider { PROVIDER_SHAPE, PROVIDER_OGR_SUPPORT };

enum eSmtWSProvider { PROVIDER_SMARTGIS };

enum eSmtMemProvider { PROVIDER_MEM_VER1 };

enum eDSType {
  DS_FILE_SMF,
  DS_DB_ADO,
  DS_DB_ODBC,
  DS_DB_MYSQL,
  DS_DB_ORACLE,
  DS_MEM,
  DS_WS,
};

struct SmtGQueryDesc {
  OGRGeometry* pQueryGeom;
  geo::SmtSpatialRs sSRs;
  float fSmargin;

  SmtGQueryDesc()
      : pQueryGeom(nullptr), sSRs(geo::SS_Contains), fSmargin(0.05f) {}
};

struct SmtPQueryDesc {
  char** szFldName;
  char** szFldQueryContent;

  SmtPQueryDesc() : szFldName(nullptr), szFldQueryContent(nullptr) {}
};

struct SmtSpIdxInfo {
  char szName[MAX_SPATIALINDEX_NAME];
  uint unType;
  char szOnwerLayerName[MAX_LAYER_NAME];

  SmtSpIdxInfo() {
    szName[0] = '\0';
    unType = 0;
    szOnwerLayerName[0] = '\0';
  }
};

struct SmtLayerInfo {
  char szName[MAX_LAYER_NAME];
  char szArchiveName[MAX_LAYER_ARCHIVE_NAME];
  char szSRS[MAX_LAYER_SRS_NAME];
  uint unFeatureType;
  uint unSIType;
  fRect lyrRect;

  SmtLayerInfo() {
    szName[0] = '\0';
    szArchiveName[0] = '\0';
    szSRS[0] = '\0';
    unFeatureType = 0;
    unSIType = 0;
  }
};

// Connection description for GDALOpenEx / Create. Not a dataset object.
struct SmtDataSourceInfo {
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

  SmtDataSourceInfo() {
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

enum eSmtFetchType { FETCH_ALL, FETCH_FILTER };

enum SmtLayerType {
  LYR_VECTOR,
  LYR_RASTER,
  LYR_TITLE,
};

enum SmtFeatureType_Ext {
  SmtLayer_Ras = SmtFtUnknown + 1,
  SmtLayer_Tile = SmtFtUnknown + 2,
};

inline const char* layer_feature_type_name(uint ftType) {
  switch (ftType) {
    case SmtFeatureType::SmtFtDot:
      return "DotFcls";
    case SmtFeatureType::SmtFtChildImage:
      return "ChildImageFcls";
    case SmtFeatureType::SmtFtAnno:
      return "AnnoFcls";
    case SmtFeatureType::SmtFtCurve:
      return "CurveFcls";
    case SmtFeatureType::SmtFtSurface:
      return "SurfaceFcls";
    case SmtFeatureType::SmtFtGrid:
      return "GridFcls";
    case SmtFeatureType::SmtFtTin:
      return "TinFcls";
    case SmtFeatureType_Ext::SmtLayer_Ras:
      return "Raster";
    default:
      return "UnKnown";
  }
}

// Raster / tile leftover. Vector layers are OGRLayer.
class SmtLayer {
 public:
  explicit SmtLayer(GDALDataset* owner = nullptr)
      : m_pOwnerDs(owner), m_bIsVisible(true), m_bOpen(false) {
    m_szLayerName[0] = '\0';
    m_szSRS[0] = '\0';
  }

  virtual ~SmtLayer() = default;

  GDALDataset* GetDataset() { return m_pOwnerDs; }
  const GDALDataset* GetDataset() const { return m_pOwnerDs; }
  GDALDataset* GetDataSource() { return m_pOwnerDs; }
  const GDALDataset* GetDataSource() const { return m_pOwnerDs; }

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(eSmtFetchType type = FETCH_ALL) = 0;

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
  virtual SmtLayerType GetLayerType() const = 0;

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

class SmtRasterLayer : public SmtLayer {
 public:
  explicit SmtRasterLayer(GDALDataset* owner = nullptr) : SmtLayer(owner) {}
  ~SmtRasterLayer() override = default;

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(eSmtFetchType type = FETCH_ALL) = 0;

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

  SmtLayerType GetLayerType() const override { return LYR_RASTER; }
};

class SmtTileLayer : public SmtLayer {
 public:
  explicit SmtTileLayer(GDALDataset* owner = nullptr)
      : SmtLayer(owner), m_lImageCode(-1) {}
  ~SmtTileLayer() override = default;

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(eSmtFetchType type = FETCH_ALL) = 0;
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

  SmtLayerType GetLayerType() const override { return LYR_TITLE; }

  void SetLayerRect(const fRect& lyrRect) override {
    m_lyrEnv.MinX = lyrRect.lb.x;
    m_lyrEnv.MinY = lyrRect.lb.y;
    m_lyrEnv.MaxX = lyrRect.rt.x;
    m_lyrEnv.MaxY = lyrRect.rt.y;
  }

 protected:
  long m_lImageCode;
};

// Leftover catalog/UI holder until those TUs speak GDALDataset directly.
class SmtDataSource {
 public:
  SmtDataSource() = default;
  SmtDataSource(std::nullptr_t) : ds_(nullptr) {}
  SmtDataSource(int) : ds_(nullptr) {}
  SmtDataSource(GDALDataset* ds) : ds_(ds) {}

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

  void GetInfo(SmtDataSourceInfo& info) const {
    info = SmtDataSourceInfo();
    if (ds_ && ds_->GetDescription()) {
      sprintf_s(info.szName, MAX_DS_NAME, "%s", ds_->GetDescription());
    }
  }

  void GetLayerInfo(SmtLayerInfo& out, int index) const {
    out = SmtLayerInfo();
    if (!ds_ || index < 0 || index >= ds_->GetLayerCount()) {
      return;
    }
    fill_layer_info(&out, ds_->GetLayer(index));
  }

  void GetLayerInfo(SmtLayerInfo& out, const char* name) const {
    out = SmtLayerInfo();
    if (!ds_ || name == nullptr) {
      return;
    }
    fill_layer_info(&out, ds_->GetLayerByName(name));
  }

  OGRLayer* OpenVectorLayer(const char* name) {
    return (ds_ && name) ? ds_->GetLayerByName(name) : nullptr;
  }

  OGRLayer* CreateVectorLayer(const char* name, const fRect&,
                              SmtFeatureType type) {
    if (!ds_ || !name) {
      return nullptr;
    }
    return ds_->CreateLayer(name, nullptr, leftover_feature_wkb(type), nullptr);
  }

  SmtRasterLayer* CreateRasterLayer(const char*, const fRect&, int) {
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

  SmtRasterLayer* OpenRasterLayer(const char* /*name*/) { return nullptr; }

  static const char* GetLayerFeatureTypeName(uint ftType) {
    return layer_feature_type_name(ftType);
  }

 private:
  static void fill_layer_info(SmtLayerInfo* out, OGRLayer* lyr) {
    if (!out || !lyr) {
      return;
    }
    sprintf_s(out->szName, MAX_LAYER_NAME, "%s", lyr->GetName());
    sprintf_s(out->szArchiveName, MAX_LAYER_ARCHIVE_NAME, "%s", lyr->GetName());
    out->unFeatureType = leftover_layer_feature_type(lyr);
  }

  GDALDataset* ds_ = nullptr;
};

inline bool operator==(std::nullptr_t, const SmtDataSource& ds) { return !ds; }

}  // namespace gis

#endif  // _GIS_SDE_H
