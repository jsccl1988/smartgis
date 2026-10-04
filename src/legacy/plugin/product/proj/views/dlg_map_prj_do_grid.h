// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include "plugin/product/world3d/grid/orthogrid/lattice/ortho_lattice.h"

class CDlgMapPrjDoGrid : public CDialog {
  DECLARE_DYNAMIC(CDlgMapPrjDoGrid)

 public:
  CDlgMapPrjDoGrid(CWnd* pParent = NULL);
  virtual ~CDlgMapPrjDoGrid();

  enum { IDD = IDD_DLG_PRJ_DOGRID };

  long ScaleRuler();

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedBtnDogrid();

  void OutputRes(plugin::detail::OrthoLattice& lattice);

 private:
  double m_fDL;
  double m_fDB;
  double m_fLmin;
  double m_fBmin;
  double m_fLmax;
  double m_fBmax;
  long m_lScaleRuler;
};
