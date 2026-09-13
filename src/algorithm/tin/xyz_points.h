// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_TIN_XYZ_POINTS_H_
#define ALGORITHM_TIN_XYZ_POINTS_H_

#include "base/core/core.h"
#include "render/math/mathlib_3d.h"

#include <vector>

#if defined(TIN_EXPORTS)
#define TIN_EXPORT_API __declspec(dllexport)
#else
#define TIN_EXPORT_API __declspec(dllimport)
#endif

namespace tin {

// ASCII XYZ → Vector3. Columns are 0-based. Blank lines are skipped.
// Missing columns on a data line return SMT_ERR_INVALID_PARAM.
TIN_EXPORT_API long read_xyz_points(const char* path,
                                    int skip_header_lines,
                                    char separator,
                                    int x_col,
                                    int y_col,
                                    int z_col,
                                    std::vector<render::Vector3>* out);

}  // namespace tin

#endif  // ALGORITHM_TIN_XYZ_POINTS_H_
