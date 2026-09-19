// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SDB_DATASOURCE_GDAL_SDBD_JSON_H_
#define SDB_DATASOURCE_GDAL_SDBD_JSON_H_

#include <map>
#include <string>
#include <vector>

#include "gis/datasource/gdal/sdbd_types.h"

namespace gis {
namespace datasource {

struct Json {
  enum Type { kNull, kBool, kNumber, kString, kArray, kObject };
  Type type = kNull;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<Json> a;
  std::map<std::string, Json> o;

  bool has(const char* key) const;
  const Json* get(const char* key) const;
  std::string string(const char* key, const std::string& def = {}) const;
  double number(const char* key, double def = 0) const;
  bool boolean(const char* key, bool def = false) const;
  std::string as_string() const;
};

bool parse_json(const std::string& text, Json* out, std::string* err);
std::string json_escape(const std::string& s);

std::string open_request_to_json(const OpenRequest& req);
std::string layer_info_to_json(const LayerInfo& info);
std::string feature_set_to_json(const FeatureSet& set);
bool parse_layer_info(const Json& json, LayerInfo* out);
bool parse_layer_info(const std::string& text, LayerInfo* out,
                      std::string* err);
bool parse_catalog(const std::string& text, std::vector<LayerInfo>* out,
                   std::string* err);
bool parse_feature_set(const std::string& text, FeatureSet* out,
                       std::string* err);
bool parse_error_body(const std::string& text, SdbdResult* out);
bool body_has_forbidden_sql(const std::string& body);

}  // namespace datasource
}  // namespace gis

#endif  // SDB_DATASOURCE_GDAL_SDBD_JSON_H_
