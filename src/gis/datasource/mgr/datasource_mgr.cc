// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/mgr/datasource_mgr.h"

#include <cstring>
#include <fstream>

#include "base/core/api.h"
#include "gdal_priv.h"
#include "ogrsf_frmts.h"
#include "gis/datasource/gdal/gdal_driver.h"
#include "gis/datasource/gdal/ogr_raster_layer.h"
#include "gis/datasource/gdal/sdbd_dataset.h"
#include "gis/datasource/gdal/sdbd_remote_dataset.h"

using namespace gis;
using namespace base;

namespace gis {

DataSourceMgr* DataSourceMgr::m_pSingleton = nullptr;

DataSourceMgr* DataSourceMgr::get_singleton_ptr() {
  if (!m_pSingleton) {
    m_pSingleton = new DataSourceMgr();
  }
  return m_pSingleton;
}

void DataSourceMgr::destroy_instance() { SMT_SAFE_DELETE(m_pSingleton); }

ScratchLayer DataSourceMgr::create_mem_vec_layer() {
  ScratchLayer sl;
  SmtDataSourceInfo info;
  info.unType = DS_MEM;
  info.unProvider = PROVIDER_MEM_VER1;
  std::strcpy(info.szName, "scratch");
  sl.dataset = gis::datasource::open_sdbd_dataset(info);
  if (sl.dataset) {
    sl.layer = sl.dataset->CreateLayer("scratch", nullptr, wkbUnknown, nullptr);
    if (sl.layer) {
      OGRFieldDefn style("style", OFTBinary);
      sl.layer->CreateField(&style);
    }
  }
  return sl;
}

void DataSourceMgr::destroy_mem_vec_layer(ScratchLayer& layer) {
  if (layer.dataset) {
    GDALClose(layer.dataset);
  }
  layer.dataset = nullptr;
  layer.layer = nullptr;
}

SmtRasterLayer* DataSourceMgr::create_mem_ras_layer() {
  gis::datasource::register_gdal_driver();
  auto* layer = new gis::datasource::OgrRasterLayer(nullptr);
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

void DataSourceMgr::destroy_mem_ras_layer(SmtRasterLayer*& pLayer) {
  SMT_SAFE_DELETE(pLayer);
}

DataSourceMgr::DataSourceMgr() = default;

DataSourceMgr::~DataSourceMgr() {
  // Best-effort persist; never throw from a destructor during app teardown.
  try {
    save();
  } catch (...) {
  }
  for (Entry& e : entries_) {
    if (e.dataset) {
      GDALClose(e.dataset);
      e.dataset = nullptr;
    }
  }
  entries_.clear();
  active_ = nullptr;
}

GDALDataset* DataSourceMgr::open_dataset(const SmtDataSourceInfo& info) {
  if (info.unProvider == PROVIDER_SDBD) {
    return gis::datasource::open_provider_sdbd_dataset(info);
  }
  return gis::datasource::open_sdbd_dataset(info);
}

void DataSourceMgr::close_dataset(GDALDataset*& ds) {
  if (ds) {
    GDALClose(ds);
    if (active_ == ds) {
      active_ = nullptr;
    }
    ds = nullptr;
  }
}

GDALDataset* DataSourceMgr::get_data_source(const char* szName) {
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

bool DataSourceMgr::get_data_source_info(const char* szName,
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

bool DataSourceMgr::get_data_source_info(int index,
                                         SmtDataSourceInfo& info) const {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return false;
  }
  info = entries_[index].info;
  return true;
}

GDALDataset* DataSourceMgr::create_tmp_data_source(eDSType type) {
  if (type != DS_MEM) {
    return nullptr;
  }
  SmtDataSourceInfo info;
  info.unType = DS_MEM;
  info.unProvider = PROVIDER_MEM_VER1;
  std::strcpy(info.szName, "tmp");
  return open_dataset(info);
}

void DataSourceMgr::destroy_tmp_data_source(GDALDataset*& pTmp) {
  close_dataset(pTmp);
}

void DataSourceMgr::destroy_tmp_data_source(gis::SmtDataSource& tmp) {
  GDALDataset* ds = tmp.dataset();
  destroy_tmp_data_source(ds);
  tmp = gis::SmtDataSource();
}

GDALDataset* DataSourceMgr::create_data_source(SmtDataSourceInfo& info) {
  if (info.szName[0] == '\0' || get_data_source(info.szName)) {
    return nullptr;
  }
  GDALDataset* ds = open_dataset(info);
  if (!ds) {
    return nullptr;
  }
  entries_.push_back(Entry{info, ds});
  return ds;
}

bool DataSourceMgr::delete_data_source(const char* szName) {
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

void DataSourceMgr::set_active_data_source(const char* szActiveDSName) {
  active_ = get_data_source(szActiveDSName);
}

void DataSourceMgr::move_first() { iterator_ = 0; }

void DataSourceMgr::move_next() {
  if (iterator_ < static_cast<int>(entries_.size())) {
    ++iterator_;
  }
}

void DataSourceMgr::move_last() {
  iterator_ = static_cast<int>(entries_.size()) - 1;
}

void DataSourceMgr::delete_current() {
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

bool DataSourceMgr::is_end() {
  return iterator_ == static_cast<int>(entries_.size());
}

GDALDataset* DataSourceMgr::get_data_source() {
  return get_data_source(iterator_);
}

GDALDataset* DataSourceMgr::get_data_source(int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    return nullptr;
  }
  return entries_[index].dataset;
}

bool DataSourceMgr::open(const char* szDSMFile) {
  if (!szDSMFile || szDSMFile[0] == '\0') {
    return false;
  }
  dsm_path_ = szDSMFile;
  std::ifstream infile;
  // Binary DSM I/O; avoid locale(".936") which can throw and abort on teardown.
  infile.open(dsm_path_.c_str(), std::ios::in | std::ios::binary);
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
    create_data_source(info);
  }
  infile.close();
  return true;
}

bool DataSourceMgr::save() {
  if (dsm_path_.empty()) {
    dsm_path_ = get_app_path() + "sys\\smartgis.dsm";
  }
  return save_as(dsm_path_.c_str());
}

bool DataSourceMgr::save_as(const char* szDSMFile) {
  if (!szDSMFile || szDSMFile[0] == '\0') {
    return false;
  }
  std::ofstream outfile;
  // Binary DSM I/O; avoid locale(".936") which can throw and abort on teardown.
  outfile.open(szDSMFile, std::ios::out | std::ios::binary);
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

}  // namespace gis
