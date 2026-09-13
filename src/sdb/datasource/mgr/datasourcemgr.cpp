// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "datasourcemgr.h"

#include "api.h"
#include "mem.h"
#include "sdb/datasource/gdal/gdal_driver.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>
#include <fstream>
#include <locale>

using namespace Smt_SDEMem;
using namespace Smt_Core;

namespace Smt_SDEDevMgr {

SmtDataSourceMgr* SmtDataSourceMgr::m_pSingleton = nullptr;

SmtDataSourceMgr* SmtDataSourceMgr::GetSingletonPtr() {
  if (!m_pSingleton) {
    m_pSingleton = new SmtDataSourceMgr();
  }
  return m_pSingleton;
}

void SmtDataSourceMgr::DestoryInstance() { SMT_SAFE_DELETE(m_pSingleton); }

ScratchLayer SmtDataSourceMgr::CreateMemVecLayer() {
  sdb::datasource::register_gdal_driver();
  ScratchLayer sl;
  sdb::datasource::OgrDataSource store;
  SmtDataSourceInfo info;
  info.unType = DS_MEM;
  info.unProvider = PROVIDER_MEM_VER1;
  std::strcpy(info.szName, "scratch");
  store.SetInfo(info);
  if (!store.Open() || !store.dataset()) {
    return sl;
  }
  sl.dataset = store.release();
  if (sl.dataset) {
    sl.layer = sl.dataset->CreateLayer("scratch", nullptr, wkbUnknown, nullptr);
    if (sl.layer) {
      OGRFieldDefn style("style", OFTBinary);
      sl.layer->CreateField(&style);
    }
  }
  return sl;
}

void SmtDataSourceMgr::DestoryMemVecLayer(ScratchLayer& layer) {
  if (layer.dataset) {
    GDALClose(layer.dataset);
  }
  layer.dataset = nullptr;
  layer.layer = nullptr;
}

SmtRasterLayer* SmtDataSourceMgr::CreateMemRasLayer() {
  auto* layer = new SmtMemRasLayer();
  fRect lyrRect;
  lyrRect.lb.x = 0;
  lyrRect.lb.y = 0;
  lyrRect.rt.x = 500;
  lyrRect.rt.y = 500;
  layer->SetLayerName("SmtMemRasLayer");
  layer->SetLayerRect(lyrRect);
  layer->Create();
  return layer;
}

void SmtDataSourceMgr::DestoryMemRasLayer(SmtRasterLayer*& pLayer) {
  SMT_SAFE_DELETE(pLayer);
}

SmtTileLayer* SmtDataSourceMgr::CreateMemTileLayer() {
  auto* layer = new SmtMemTileLayer();
  fRect lyrRect;
  lyrRect.lb.x = 0;
  lyrRect.lb.y = 0;
  lyrRect.rt.x = 500;
  lyrRect.rt.y = 500;
  layer->SetLayerName("SmtMemTileLayer");
  layer->SetLayerRect(lyrRect);
  layer->Create();
  return layer;
}

void SmtDataSourceMgr::DestoryMemTileLayer(SmtTileLayer*& pLayer) {
  SMT_SAFE_DELETE(pLayer);
}

SmtDataSourceMgr::SmtDataSourceMgr() = default;

SmtDataSourceMgr::~SmtDataSourceMgr() {
  Save();
  for (auto* ds : sources_) {
    delete ds;
  }
  sources_.clear();
  active_ = nullptr;
}

sdb::datasource::OgrDataSource* SmtDataSourceMgr::GetDataSource(
    const char* szName) {
  if (!szName) {
    return nullptr;
  }
  for (auto* ds : sources_) {
    SmtDataSourceInfo info;
    ds->GetInfo(info);
    if (std::strcmp(info.szName, szName) == 0) {
      return ds;
    }
  }
  return nullptr;
}

sdb::datasource::OgrDataSource* SmtDataSourceMgr::CreateTmpDataSource(
    eDSType type) {
  if (type == DS_WS || type == DS_DB_ODBC || type == DS_DB_MYSQL ||
      type == DS_DB_ORACLE) {
    return nullptr;
  }
  auto* ds = new sdb::datasource::OgrDataSource();
  SmtDataSourceInfo info;
  info.unType = type;
  if (type == DS_MEM) {
    info.unProvider = PROVIDER_MEM_VER1;
  }
  ds->SetInfo(info);
  return ds;
}

void SmtDataSourceMgr::DestoryTmpDataSource(
    sdb::datasource::OgrDataSource*& pTmp) {
  SMT_SAFE_DELETE(pTmp);
}

sdb::datasource::OgrDataSource* SmtDataSourceMgr::CreateDataSource(
    SmtDataSourceInfo& info) {
  if (info.szName[0] == '\0' || GetDataSource(info.szName)) {
    return nullptr;
  }
  if (info.unType == DS_WS || info.unType == DS_DB_ODBC ||
      info.unType == DS_DB_MYSQL || info.unType == DS_DB_ORACLE) {
    return nullptr;
  }
  auto* ds = new sdb::datasource::OgrDataSource();
  ds->SetInfo(info);
  if (!ds->Open()) {
    delete ds;
    return nullptr;
  }
  ds->Close();
  sources_.push_back(ds);
  return ds;
}

bool SmtDataSourceMgr::DeleteDataSource(const char* szName) {
  for (auto it = sources_.begin(); it != sources_.end(); ++it) {
    SmtDataSourceInfo info;
    (*it)->GetInfo(info);
    if (std::strcmp(info.szName, szName) == 0) {
      if (*it == active_) {
        active_ = nullptr;
      }
      delete *it;
      sources_.erase(it);
      return true;
    }
  }
  return false;
}

void SmtDataSourceMgr::SetActiveDataSource(const char* szActiveDSName) {
  active_ = GetDataSource(szActiveDSName);
}

void SmtDataSourceMgr::MoveFirst() { iterator_ = 0; }

void SmtDataSourceMgr::MoveNext() {
  if (iterator_ < static_cast<int>(sources_.size())) {
    ++iterator_;
  }
}

void SmtDataSourceMgr::MoveLast() {
  iterator_ = static_cast<int>(sources_.size()) - 1;
}

void SmtDataSourceMgr::Delete() {
  if (iterator_ < 0 || iterator_ >= static_cast<int>(sources_.size())) {
    return;
  }
  if (sources_[iterator_] == active_) {
    active_ = nullptr;
  }
  delete sources_[iterator_];
  sources_.erase(sources_.begin() + iterator_);
}

bool SmtDataSourceMgr::IsEnd() {
  return iterator_ == static_cast<int>(sources_.size());
}

sdb::datasource::OgrDataSource* SmtDataSourceMgr::GetDataSource() {
  return GetDataSource(iterator_);
}

sdb::datasource::OgrDataSource* SmtDataSourceMgr::GetDataSource(int index) {
  if (index < 0 || index >= static_cast<int>(sources_.size())) {
    return nullptr;
  }
  return sources_[index];
}

bool SmtDataSourceMgr::Open(const char* szDSMFile) {
  if (!szDSMFile || szDSMFile[0] == '\0') {
    return false;
  }
  dsm_path_ = szDSMFile;
  std::ifstream infile;
  std::locale loc = std::locale::global(std::locale(".936"));
  infile.open(dsm_path_.c_str(), std::ios::in | std::ios::binary);
  std::locale::global(std::locale(loc));
  if (!infile.is_open()) {
    return false;
  }
  char szHead[4] = {};
  infile.read(szHead, 4);
  int nDSs = 0;
  infile.read(reinterpret_cast<char*>(&nDSs), sizeof(int));
  for (int i = 0; i < nDSs; ++i) {
    SmtDataSourceInfo info;
    infile.read(reinterpret_cast<char*>(&info), sizeof(SmtDataSourceInfo));
    CreateDataSource(info);
  }
  infile.close();
  return true;
}

bool SmtDataSourceMgr::Save() {
  if (dsm_path_.empty()) {
    dsm_path_ = GetAppPath() + "sys\\smartgis.dsm";
  }
  return SaveAs(dsm_path_.c_str());
}

bool SmtDataSourceMgr::SaveAs(const char* szDSMFile) {
  if (!szDSMFile || szDSMFile[0] == '\0') {
    return false;
  }
  std::ofstream outfile;
  std::locale loc = std::locale::global(std::locale(".936"));
  outfile.open(szDSMFile, std::ios::out | std::ios::binary);
  std::locale::global(std::locale(loc));
  if (!outfile.is_open()) {
    return false;
  }
  char szHead[4] = "DSM";
  outfile.write(szHead, 4);
  int nDSs = static_cast<int>(sources_.size());
  outfile.write(reinterpret_cast<char*>(&nDSs), sizeof(int));
  for (auto* ds : sources_) {
    SmtDataSourceInfo info;
    ds->GetInfo(info);
    outfile.write(reinterpret_cast<char*>(&info), sizeof(SmtDataSourceInfo));
  }
  outfile.close();
  return true;
}

}  // namespace Smt_SDEDevMgr
