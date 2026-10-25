// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Feature scan verbs. The dataset kernel lives in gis::detail.

#include "content/browser/glang.runtime/exec/op/gis/geom/gis.h"

#include "gis/analysis/ops/features/feature_ops.h"

namespace content {
namespace code {

bool scan_features(std::string_view path, std::string* out, std::string* error) {
  return gis::detail::scan_features(path, out, error);
}

bool filter_features(std::string_view path,
                     std::string_view where,
                     std::string_view wkt,
                     std::string* out,
                     std::string* error) {
  return gis::detail::filter_features(path, where, wkt, out, error);
}

bool read_feature_field(std::string_view path,
                        int32_t fid,
                        std::string_view field,
                        std::string* out,
                        std::string* error) {
  return gis::detail::read_feature_field(path, fid, field, out, error);
}

bool project_features(std::string_view path,
                      std::string_view fields,
                      std::string* out,
                      std::string* error) {
  return gis::detail::project_features(path, fields, out, error);
}

}  // namespace code
}  // namespace content
