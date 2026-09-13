// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/datasource/mgr/datasourcemgr.h"

#include "base/core/api.h"
#include "sdb/datasource/mem/mem.h"
#include "sdb/datasource/gdal/gdal_driver.h"
#include "sdb/datasource/gdal/ogr_raster_layer.h"
#include "sdb/datasource/gdal/sdbd_dataset.h"

#include "gdal_priv.h"
#include "ogrsf_frmts.h"

#include <cstring>
#include <fstream>
#include <locale>

using namespace sdb;
using namespace base;

namespace sdb {

SmtDataSourceMgr* SmtDataSourceMgr::m_pSingleton = nullptr;

SmtDataSourceMgr* SmtDataSourceMgr::get_singleton_ptr() {
  if (!m_pSingleton) {
    m_pSingleton = new SmtDataSourceMgr();
  }
  return m_pSingleton;
}

void SmtDataSourceMgr::DestoryInstance() { SMT_SAFE_DELETE(m_pSingleton); }

ScratchLayer SmtDataSourceMgr::CreateMemVecLayer() {
  ScratchLayer sl;
  SmtDataSourceInfo info;
  info.unType = DS_MEM;
  info.unProvider = PROVIDER_MEM_VER1;
  std::strcpy(info.szName, "scratch");
  sl.dataset = sdb::datasource::open_sdbd_dataset(info);
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
  sdb::datasource::register_gdal_driver();
  auto* layer = new sdb::datasource::OgrRasterLayer(nullptr);
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
  for (Entry& e : entries_) {
    if (e.dataset) {
      GDALClose(e.dataset);
    }
  }
  entries_.clear();
  active_ = nullptr;
}

GDALDataset* SmtDataSourceMgr::OpenDataset(const SmtDataSourceInfo& info) {
  return sdb::datasource::open_sdbd_dataset(info);
}

void SmtDataSourceMgr::CloseDataset(GDALDataset*& ds) {
  if (ds) {
    GDALClose(ds);
    if (active_ == ds) {
      active_ = nullptr;
    }
    ds = nullptr;
  }
}

GDALDataset* SmtDataSourceMgr::GetDataSource(const char* szName) {
  if (!szName) {
    return nullptr;
  }
  for (Entry& e : entries_) {
    if (std::strcmp(e.info.szName, szName) == 0) {
      return e.dataset;
    }
  }
  return nullptr;
}

bool SmtDataSourceMgr::GetDataSourceInfo(const char* szName,
                                         SmtDataSourceInfo& info) const {
  if (!szName) {
    return false;
  }
  for (const Entry& e : entries_) {
    if (std::strcmp(e.info.szName, szName) == 0) {
      info = e.info;
      return true;
    }
  }
  return false;
}

bool SmtDataSourceMgr::GetDataSourceInfo(int index,
                                         SmtDataSourceInfo& info) const {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return false;
  }
  info = entries_[index].info;
  return true;
}

GDALDataset* SmtDataSourceMgr::CreateTmpDataSource(eDSType type) {
  if (type != DS_MEM) {
    return nullptr;
  }
  SmtDataSourceInfo info;
  info.unType = DS_MEM;
  info.unProvider = PROVIDER_MEM_VER1;
  std::strcpy(info.szName, "tmp");
  return OpenDataset(info);
}

void SmtDataSourceMgr::DestoryTmpDataSource(GDALDataset*& pTmp) {
  CloseDataset(pTmp);
}

void SmtDataSourceMgr::DestoryTmpDataSource(sdb::SmtDataSource& tmp) {
  GDALDataset* ds = tmp.dataset();
  DestoryTmpDataSource(ds);
  tmp = sdb::SmtDataSource();
}

GDALDataset* SmtDataSourceMgr::CreateDataSource(SmtDataSourceInfo& info) {
  if (info.szName[0] == '\0' || GetDataSource(info.szName)) {
    return nullptr;
  }
  GDALDataset* ds = OpenDataset(info);
  if (!ds) {
    return nullptr;
  }
  entries_.push_back(Entry{info, ds});
  return ds;
}

bool SmtDataSourceMgr::DeleteDataSource(const char* szName) {
  for (auto it = entries_.begin(); it != entries_.end(); ++it) {
    if (std::strcmp(it->info.szName, szName) == 0) {
      if (it->dataset == active_) {
        active_ = nullptr;
      }
      if (it->dataset) {
        GDALClose(it->dataset);
      }
      entries_.erase(it);
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
  if (iterator_ < static_cast<int>(entries_.size())) {
    ++iterator_;
  }
}

void SmtDataSourceMgr::MoveLast() {
  iterator_ = static_cast<int>(entries_.size()) - 1;
}

void SmtDataSourceMgr::Delete() {
  if (iterator_ < 0 || iterator_ >= static_cast<int>(entries_.size())) {
    return;
  }
  if (entries_[iterator_].dataset == active_) {
    active_ = nullptr;
  }
  if (entries_[iterator_].dataset) {
    GDALClose(entries_[iterator_].dataset);
  }
  entries_.erase(entries_.begin() + iterator_);
}

bool SmtDataSourceMgr::IsEnd() {
  return iterator_ == static_cast<int>(entries_.size());
}

GDALDataset* SmtDataSourceMgr::GetDataSource() {
  return GetDataSource(iterator_);
}

GDALDataset* SmtDataSourceMgr::GetDataSource(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return nullptr;
  }
  return entries_[index].dataset;
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
    dsm_path_ = get_app_path() + "sys\\smartgis.dsm";
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
  int nDSs = static_cast<int>(entries_.size());
  outfile.write(reinterpret_cast<char*>(&nDSs), sizeof(int));
  for (const Entry& e : entries_) {
    outfile.write(reinterpret_cast<const char*>(&e.info),
                  sizeof(SmtDataSourceInfo));
  }
  outfile.close();
  return true;
}

}  // namespace sdb
