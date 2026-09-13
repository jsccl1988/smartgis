// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_CURVE_H_
#define PLUGIN_ORTHOGRID_CURVE_H_

#include "plugin/orthogrid/types.h"

namespace orthogrid {

void Spline3(vCtrlPoints& ctrl_pts, vCurvePoints& inter_pts, int np);
void CurveSampling(vCurvePoints& org_curve, vCurvePoints& new_curve, int n);
void GetCtrlPointsPosOnCurve(vCtrlPoints& ctrl_pts, vCurvePoints& curve_pts);
void SetCtrlPoints(vCtrlPoints& ctrl_pts, vdbfPoints& pts);
void SetCurvePoints(vCtrlPoints& ctrl_pts, vdbfPoints& pts);

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_CURVE_H_
