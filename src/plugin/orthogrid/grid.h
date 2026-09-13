// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_ORTHOGRID_GRID_H_
#define PLUGIN_ORTHOGRID_GRID_H_

#include <fstream>

#include "algorithm/geo/geometry.h"
#include "base/core/matrix2d.h"
#include "plugin/orthogrid/region.h"
#include "plugin/orthogrid/types.h"

using geo::RawPoint;
using geo::Grid;
using geo::SmtGrid;

namespace orthogrid {

// Boundary-adaptive orthogonal grid. Dirichlet boundaries plus Eigen
// SparseLU Laplace / Thompson elliptic interior.
class Orthogrid {
 public:
  Orthogrid(void);
  Orthogrid(int nX, int nY);
  virtual ~Orthogrid(void);

 protected:
  void Release(void);

 public:
  void SetSize(int nX, int nY);
  void ReSize(int nX, int nY);
  void GetSize(int& nX, int& nY);

  long LoadGridBndFromFile(const char* file);
  long SaveGridBndToFile(const char* file);

 public:
  void SetMainRegoinBoudary(vdbfPoints& bnd0,
                            vdbfPoints& bnd1,
                            vdbfPoints& bnd2,
                            vdbfPoints& bnd3);
  void SetAppendMainRegoinBoudary(vdbfPoints& bnd0,
                                  vdbfPoints& bnd1,
                                  vdbfPoints& bnd2,
                                  vdbfPoints& bnd3);

  long CreateOrthGrid(void);
  long CvtToGrid(SmtGrid& oSmtGrid);

  void InitalCell(bool sel);
  void SetGridCell(void);
  void CalGridOrthogonality(void);

  bool AddCtrlPt(dbfPoint pt, int a = 10);
  void ModCtrlPt(dbfPoint pt);
  bool DelCtrlPt(dbfPoint pt, int a = 10);

  bool DigRectRegion(int iCellStart, int jCellStart, int iCellEnd, int jCellEnd);
  void GetRegionBoudaryX(vCurvePoints& bnd, int iStart, int iEnd, int J);
  void GetRegionBoudaryY(vCurvePoints& bnd, int jStart, int jEnd, int I);
  void SetDiggedRegionInvalid(void);

  void GetBoudaryNode(int start, int end, int index, int flag, vdbfPoints& nodes);
  void GetBoudaryNodeX(int start, int end, int index, vdbfPoints& nodes);
  void GetBoudaryNodeY(int start, int end, int index, vdbfPoints& nodes);

 protected:
  void InitialGrid(void);
  void Orhogonal_SOR(void);

 protected:
  void SlideBoudary(void);
  void SlideRegionBoudary(void);
  void SlideRegionBoudary(Region* pRegion);
  void AjustRegionCornerNode(Region* pRegion);
  void AjustRegionCornerNode(int i, int j);
  void SlideBoudary(Boundary* pBnd);
  void SlideBoudaryX(vCurvePoints& bnd, int iStart, int iEnd, int J);
  void SlideBoudaryY(vCurvePoints& bnd, int jStart, int jEnd, int I);

  void InitialBoudary();
  void InitialRegionBoudary();
  void InitialRegionBoudary(Region* pRegion);
  void SampleBoudary(Boundary* pBnd);
  void SampleBoudaryX(vCurvePoints& bnd, int iStart, int iEnd, int J);
  void SampleBoudaryY(vCurvePoints& bnd, int jStart, int jEnd, int I);
  void SetBoudaryX(vCurvePoints& bnd, int iStart, int iEnd, int J);
  void SetBoudaryY(vCurvePoints& bnd, int jStart, int jEnd, int I);

  void InitialInternal();

 protected:
  void WriteMainRegion(std::ofstream& fout);
  void ReadMainRegion(std::ifstream& fin);
  void WriteDiggedRegion(std::ofstream& fout);
  void ReadDiggedRegion(std::ifstream& fin);

 protected:
  void AddOrth(int i, int j, float add);
  bool IsOnDiggedRegion(int ii, int jj);
  bool IsInDiggedRegion(int ii, int jj);

 protected:
  vRegionPtrs m_rRegions;
  Region m_rMainRegion;

 protected:
  Matrix2D<dbfPoint>* m_pNodes;
  Matrix2D<GridCell>* m_pCells;
  Matrix2D<float>* m_pOrthogonality;

  int m_nX;
  int m_nY;
};

}  // namespace orthogrid

#endif  // PLUGIN_ORTHOGRID_GRID_H_
