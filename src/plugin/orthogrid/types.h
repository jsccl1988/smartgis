// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_TYPES_H_
#define PLUGIN_ORTHOGRID_TYPES_H_

#include "base/core/bas_struct.h"

using namespace base;

namespace orthogrid {

struct GridCell {
  bool IsSelected;
  int MaskerClr;
  dbfPoint P;
};

struct CtrlPoint {
  dbfPoint P;
  int PreIndex;
  int NexIndex;
};

typedef std::vector<CtrlPoint> vCtrlPoints;
typedef std::vector<dbfPoint> vdbfPoints;
typedef std::vector<dbfPoint> vCurvePoints;

// One grid-aligned boundary segment with control and curve samples.
struct Boundary {
  int flag;  // 0,1,2,3: bottom, right, top, left
  int start, end;
  int index;
  bool can_slide;
  bool can_sample;
  vCtrlPoints ctrlPts;
  vCurvePoints curvePts;

  void Clear() {
    flag = -1;
    start = -1;
    end = -1;
    index = -1;
    can_slide = true;
    can_sample = true;
    ctrlPts.clear();
    curvePts.clear();
  }
};

typedef std::vector<Boundary*> vBoudaryPtrs;

const double fInvalidNum = 1.00000e+006;

#define InValid(P) ((P.x) == fInvalidNum || (P.y) == fInvalidNum)

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_TYPES_H_
