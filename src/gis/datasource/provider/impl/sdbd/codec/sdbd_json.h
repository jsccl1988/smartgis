// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_DATASOURCE_SDBD_CODEC_SDBD_JSON_H_
#define GIS_DATASOURCE_SDBD_CODEC_SDBD_JSON_H_

#include <string>
#include <vector>

// Windows.h may already have defined min/max; RapidJSON needs the names free.
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <rapidjson/document.h>

namespace gis {
namespace datasource {

// Parse sdbd wire JSON into a RapidJSON Document. On failure *err may receive a
// short message and *out is left null.
bool parse_json(const std::string& text,
                rapidjson::Document* out,
                std::string* err = nullptr);

// Extract collection ids from a mogu collections JSON object body.
std::vector<std::string> parse_collection_ids(const std::string& body);

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_SDBD_CODEC_SDBD_JSON_H_
