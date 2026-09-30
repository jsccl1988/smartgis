// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_TIN_XYZ_POINTS_H_
#define ALGORITHM_TIN_XYZ_POINTS_H_


#include "gis/gis_export.h"
#include "legacy/core/macros/macros.h"
#include "base/math/math.h"

#include <vector>

namespace tin {

// ASCII XYZ → Vector3. Columns are 0-based. Blank lines are skipped.
// Missing columns on a data line return SMT_ERR_INVALID_PARAM.
GIS_EXPORT long read_xyz_points(const char* path,
                                    int skip_header_lines,
                                    char separator,
                                    int x_col,
                                    int y_col,
                                    int z_col,
                                    std::vector<render::Vector3>* out);

}  // namespace tin

#endif  // ALGORITHM_TIN_XYZ_POINTS_H_
