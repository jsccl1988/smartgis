// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_HANDLER_H_
#define SDB_DATASOURCE_GDAL_SDBD_HANDLER_H_

#include "sdb/datasource/gdal/ogr_export.h"
#include "sdb/datasource/gdal/sdbd_types.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class GDALDataset;

namespace sdb {
namespace datasource {

// mgis /sdbd/api/v1 JSON routes implemented on a SDBD GDALDataset
// (GDALOpenEx("SDBD:...")). No SmtDataSource / SmtVectorLayer / SmtFeature.
class SMT_SDE_GDAL_EXPORT SdbdHandler {
 public:
  SdbdHandler();
  ~SdbdHandler();

  SdbdHandler(const SdbdHandler&) = delete;
  SdbdHandler& operator=(const SdbdHandler&) = delete;

  void set_ready(bool ready) { ready_ = ready; }
  bool is_ready() const { return ready_; }
  GDALDataset* dataset() { return dataset_; }

  void handle(const std::string& method, const std::string& path,
              const std::string& body, int* status, std::string* response);

 private:
  struct Cursor {
    std::string layer;
    std::string crs;
    std::vector<std::int64_t> fids;
  };

  void write_error(int http, const char* code, const char* message, int* status,
                   std::string* response) const;
  void write_ok(const std::string& body, int* status,
                std::string* response) const;
  bool parse_open_request(const std::string& body, OpenRequest* req,
                          int* status, std::string* response) const;
  LayerInfo layer_info_of(const std::string& name) const;
  std::vector<LayerInfo> catalog() const;
  std::vector<std::int64_t> query_hits(const OpenRequest& req) const;
  FeatureSet features_of(const std::string& layer, const std::string& crs,
                         int offset,
                         const std::vector<std::int64_t>& fids) const;
  bool open_dataset(const std::string& body, int* status,
                    std::string* response);
  bool create_layer(const std::string& body, int* status,
                    std::string* response);
  bool append_features(const std::string& body, int* status,
                       std::string* response);
  void close_dataset();

  GDALDataset* dataset_ = nullptr;
  bool ready_ = true;
  std::uint64_t next_handle_ = 1;
  std::map<std::string, Cursor> cursors_;
};

}  // namespace datasource
}  // namespace sdb

#endif  // SDB_DATASOURCE_GDAL_SDBD_HANDLER_H_
