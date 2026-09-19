// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_MGR_DATASOURCE_MGR_H_
#define GIS_DATASOURCE_MGR_DATASOURCE_MGR_H_

#include <string>
#include <vector>

#include "gis/datasource/mgr/sde_mgr_export.h"
#include "gis/layer/layer.h"

class GDALDataset;
class OGRLayer;

using namespace gis;

namespace gis {

// Memory-driver scratch layer via SDBD:MEM. Caller must destroy_mem_vec_layer.
struct ScratchLayer {
  GDALDataset* dataset = nullptr;
  OGRLayer* layer = nullptr;
};

// Owns registered GDAL datasets (DSM catalog) and scratch MEM layers.
class SDE_MGR_EXPORT DataSourceMgr {
 private:
  DataSourceMgr();

 public:
  virtual ~DataSourceMgr();

  static ScratchLayer create_mem_vec_layer();
  static void destroy_mem_vec_layer(ScratchLayer& layer);

  static SmtRasterLayer* create_mem_ras_layer();
  static void destroy_mem_ras_layer(SmtRasterLayer*& pLayer);

  static DataSourceMgr* get_singleton_ptr();
  static void destroy_instance();

  bool open(const char* szDSMFile);
  bool save();
  bool save_as(const char* szDSMFile);

  GDALDataset* open_dataset(const SmtDataSourceInfo& info);
  void close_dataset(GDALDataset*& ds);

  GDALDataset* create_tmp_data_source(eDSType type);
  void destroy_tmp_data_source(GDALDataset*& pTmp);
  void destroy_tmp_data_source(gis::SmtDataSource& tmp);

  GDALDataset* create_data_source(SmtDataSourceInfo& info);
  bool delete_data_source(const char* szName);

  int get_data_source_count() { return static_cast<int>(entries_.size()); }

  void move_first();
  void move_next();
  void move_last();
  void delete_current();
  bool is_end();

  GDALDataset* get_data_source();
  GDALDataset* get_data_source(int index);
  GDALDataset* get_data_source(const char* szName);

  bool get_data_source_info(const char* szName, SmtDataSourceInfo& info) const;
  bool get_data_source_info(int index, SmtDataSourceInfo& info) const;

  GDALDataset* get_active_data_source() { return active_; }
  void set_active_data_source(const char* szActiveDSName);
  void set_active_data_source(GDALDataset* pActive) {
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

  static DataSourceMgr* m_pSingleton;
};

}  // namespace gis

#endif  // GIS_DATASOURCE_MGR_DATASOURCE_MGR_H_
