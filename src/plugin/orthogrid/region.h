// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_REGION_H_
#define PLUGIN_ORTHOGRID_REGION_H_

#include "plugin/orthogrid/types.h"

namespace orthogrid {

enum Region_type {
  typeMain,
  typeDigged,
};

enum ShowType {
  typeSel,
  typeGeneral,
};

class Orthogrid;

// One control region (main or dug) and its sampled boundaries.
class Region {
 public:
  friend class Orthogrid;

  Region(Region_type rType);
  ~Region(void);
  void Release(void);

 public:
  void SetType(Region_type rType);
  void SetSlided(int SelBnd, bool slided = true);
  void SetSlided(bool slided = true);
  void AppendBoudary(vdbfPoints& pts,
                     int start,
                     int end,
                     int index,
                     int flag,
                     bool slided = true,
                     bool sampled = true);
  void ReSizeBoudary(int ib, int start, int end, int index, int flag);

  bool HitTestOn(int i, int j);
  bool HitTestOnCorner(int i, int j);
  bool HitTestIn(int i, int j);
  bool HitTestBoudaryOn(Boundary* pBnd, int i, int j);
  bool HitTestBoudaryOnCorner(Boundary* pBnd, int i, int j);
  bool HitTestBoudaryRightHand(Boundary* pBnd, int i, int j);

 public:
  void SmoothBoundary(int N = 30);

 public:
  bool AddCtrlPt(dbfPoint pt, int a = 10);
  bool DelCtrlPt(dbfPoint pt, int a = 10);
  void ModCtrlPt(dbfPoint pt);

 private:
  bool AddCtrlPt(Boundary* pBnd, dbfPoint pt, int a = 10);
  bool DelCtrlPt(Boundary* pBnd, dbfPoint pt, int a);
  bool SearchCtrlPt(dbfPoint pt, int a = 10);
  bool SearchCtrlPt(Boundary* pBnd, dbfPoint pt, int a = 10);
  void InsertCtrlPt(vCtrlPoints& vbnd, CtrlPoint pt);

 private:
  Region_type m_rType;
  vBoudaryPtrs m_vBnds;

  int m_selBnd;
  int m_modCtrlPtsIndex;
};

typedef std::vector<Region*> vRegionPtrs;

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_REGION_H_
