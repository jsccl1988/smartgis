// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _GIS_SDE_H
#define _GIS_SDE_H

#include "feature.h"
#include "geometry.h"

using namespace Smt_Geo;
using namespace Smt_Core;
using namespace Smt_Base;

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
class OGRLayer;

namespace Smt_GIS {

enum eSmtDBProvider {
  PROVIDER_ACCESS,
  PROVIDER_SQLSERVER,
  PROVIDER_ORACLE,
  PROVIDER_MYSQL,
  PROVIDER_POSTGRES,
  PROVIDER_GPKG,
  PROVIDER_SPATIALITE,
};

enum eSmtFileProvider {
  PROVIDER_SHAPE,
  PROVIDER_OGR_SUPPORT
};

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
  SmtGeometry* pQueryGeom;
  SmtSpatialRs sSRs;
  float fSmargin;

  SmtGQueryDesc()
      : pQueryGeom(nullptr), sSRs(SS_Contains), fSmargin(0.05f) {}
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
      : m_pOwnerDs(owner),
        m_pAtt(nullptr),
        m_bIsVisible(true),
        m_bOpen(false) {
    m_szLayerName[0] = '\0';
    m_szSRS[0] = '\0';
  }

  virtual ~SmtLayer() { SMT_SAFE_DELETE(m_pAtt); }

  const GDALDataset* GetDataset() const { return m_pOwnerDs; }

  virtual bool Create() = 0;
  virtual bool Open(const char* szLayerArchiveName) = 0;
  virtual bool Close() = 0;
  virtual bool Fetch(eSmtFetchType type = FETCH_ALL) = 0;

  bool IsOpen() const { return m_bOpen; }

  void SetAttribute(const SmtAttribute* pAtt) {
    SMT_SAFE_DELETE(m_pAtt);
    m_pAtt = pAtt ? pAtt->Clone() : nullptr;
  }

  SmtAttribute* GetAttribute() { return m_pAtt; }
  const SmtAttribute* GetAttribute() const { return m_pAtt; }

  void GetEnvelope(Envelope& env) const {
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
  SmtAttribute* m_pAtt;
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

}  // namespace Smt_GIS

#endif  // _GIS_SDE_H
