// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef _SMT_DSMGR_H
#define _SMT_DSMGR_H

#include "layer.h"

#include <string>
#include <vector>

class GDALDataset;
class OGRLayer;

using namespace Smt_GIS;

namespace Smt_SDEDevMgr {

// Memory-driver scratch layer via SDBD:MEM. Caller must DestoryMemVecLayer.
struct ScratchLayer {
  GDALDataset* dataset = nullptr;
  OGRLayer* layer = nullptr;
};

class SMT_EXPORT_CLASS SmtDataSourceMgr {
 private:
  SmtDataSourceMgr();

 public:
  virtual ~SmtDataSourceMgr();

  static ScratchLayer CreateMemVecLayer();
  static void DestoryMemVecLayer(ScratchLayer& layer);

  static SmtRasterLayer* CreateMemRasLayer();
  static void DestoryMemRasLayer(SmtRasterLayer*& pLayer);

  static SmtTileLayer* CreateMemTileLayer();
  static void DestoryMemTileLayer(SmtTileLayer*& pLayer);

  static SmtDataSourceMgr* GetSingletonPtr();
  static void DestoryInstance();

  bool Open(const char* szDSMFile);
  bool Save();
  bool SaveAs(const char* szDSMFile);

  GDALDataset* OpenDataset(const SmtDataSourceInfo& info);
  void CloseDataset(GDALDataset*& ds);

  GDALDataset* CreateTmpDataSource(eDSType type);
  void DestoryTmpDataSource(GDALDataset*& pTmp);

  GDALDataset* CreateDataSource(SmtDataSourceInfo& info);
  bool DeleteDataSource(const char* szName);

  int GetDataSourceCount() { return static_cast<int>(entries_.size()); }

  void MoveFirst();
  void MoveNext();
  void MoveLast();
  void Delete();
  bool IsEnd();

  GDALDataset* GetDataSource();
  GDALDataset* GetDataSource(int index);
  GDALDataset* GetDataSource(const char* szName);

  bool GetDataSourceInfo(const char* szName, SmtDataSourceInfo& info) const;
  bool GetDataSourceInfo(int index, SmtDataSourceInfo& info) const;

  GDALDataset* GetActiveDataSource() { return active_; }
  void SetActiveDataSource(const char* szActiveDSName);
  void SetActiveDataSource(GDALDataset* pActive) {
    if (pActive) {
      active_ = pActive;
    }
  }

 private:
  struct Entry {
    SmtDataSourceInfo info;
    GDALDataset* dataset = nullptr;
  };

  GDALDataset* active_ = nullptr;
  std::vector<Entry> entries_;
  int iterator_ = 0;
  std::string dsm_path_;

  static SmtDataSourceMgr* m_pSingleton;
};

}  // namespace Smt_SDEDevMgr

#if !defined(Export_SmtSDEDeviceMgr)
#if defined(_DEBUG)
#pragma comment(lib, "SmtSDEDeviceMgrD.lib")
#else
#pragma comment(lib, "SmtSDEDeviceMgr.lib")
#endif
#endif

#endif  // _SMT_DSMGR_H
