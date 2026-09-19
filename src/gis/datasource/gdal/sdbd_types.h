// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_TYPES_H_
#define SDB_DATASOURCE_GDAL_SDBD_TYPES_H_

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace gis {
namespace datasource {

// mgis /sdbd/api/v1 contract (catalog + recordset). Keys match
// C:\Dev\src\gis\mgis\core\sdb\sdbd_types.h.

struct BBox {
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;

  bool intersects(const BBox& other) const {
    return min_x <= other.max_x && max_x >= other.min_x &&
           min_y <= other.max_y && max_y >= other.min_y;
  }
};

struct FieldInfo {
  std::string name;
  std::string type;
};

struct LayerInfo {
  std::string name;
  std::string crs;
  std::string geom_type;
  std::string geom_column;
  BBox extent;
  std::vector<FieldInfo> fields;
};

struct OpenRequest {
  std::string layer;
  std::vector<std::string> columns;
  BBox bbox;
  bool has_bbox = false;
  std::string predicate{"intersects"};
  std::string crs;
  int limit = 10000;
};

struct Feature {
  std::string id;
  std::string geom_wkt;
  std::map<std::string, std::string> attrs;
};

struct FeatureSet {
  std::string layer;
  std::string crs;
  int count = 0;
  int offset = 0;
  std::vector<Feature> features;

  void move_first() { index_ = 0; }
  bool move_next() {
    if (index_ < features.size()) {
      ++index_;
    }
    return !is_end();
  }
  bool is_end() const { return index_ >= features.size(); }
  const Feature* get_feature() const {
    return is_end() ? nullptr : &features[index_];
  }
  std::size_t get_feature_count() const { return features.size(); }

 private:
  std::size_t index_ = 0;
};

struct SdbdResult {
  bool ok = false;
  int status = 0;
  std::string body;
  std::string error;
  std::string message;
};

inline constexpr char kDefaultSdbdBase[] = "http://127.0.0.1:8021";
inline constexpr char kSdbdApiPrefix[] = "/sdbd/api/v1";
inline constexpr int kSdbdLimitHardCap = 100000;
inline constexpr int kSdbdFetchDefault = 1000;
inline constexpr int kSdbdFetchMax = 10000;

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_SDBD_TYPES_H_
