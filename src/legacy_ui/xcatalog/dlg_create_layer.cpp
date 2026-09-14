// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy_ui/xcatalog/xcatalog_core.h"
#include "legacy_ui/xcatalog/dlg_create_layer.h"

#include "sdb/feature/feature.h"
#include "sdb/layer/layer.h"

using namespace sdb;
using namespace base;

IMPLEMENT_DYNAMIC(CDlgCreateLayer, CDialog)

CDlgCreateLayer::CDlgCreateLayer(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgCreateLayer::IDD, pParent), m_strLayerName(_T("")) {
  m_lyrRect.lb.x = 0;
  m_lyrRect.lb.y = 0;
  m_lyrRect.rt.x = 500;
  m_lyrRect.rt.y = 500;
}

CDlgCreateLayer::~CDlgCreateLayer() {}

void CDlgCreateLayer::DoDataExchange(CDataExchange* pDX) {
  CDialog::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_FCLS_TREE, m_FclsTree);
  DDX_Text(pDX, IDC_EDIT_LAYER_NAME, m_strLayerName);
  DDX_Text(pDX, IDC_EDIT_LAYER_TYPE, m_strSelFclsName);
  DDX_Text(pDX, IDC_EDIT_XMIN, m_lyrRect.lb.x);
  DDX_Text(pDX, IDC_EDIT_YMIN, m_lyrRect.lb.y);
  DDX_Text(pDX, IDC_EDIT_XMAX, m_lyrRect.rt.x);
  DDX_Text(pDX, IDC_EDIT_YMAX, m_lyrRect.rt.y);
}

BEGIN_MESSAGE_MAP(CDlgCreateLayer, CDialog)
  ON_BN_CLICKED(IDOK, &CDlgCreateLayer::OnBnClickedOk)
  ON_NOTIFY(NM_CLICK, IDC_FCLS_TREE, &CDlgCreateLayer::OnNMClickFclsTree)
  ON_EN_CHANGE(IDC_EDIT_LAYER_NAME, &CDlgCreateLayer::OnEnChangeEditLayerName)
  ON_EN_CHANGE(IDC_EDIT_XMIN, &CDlgCreateLayer::OnEnChangeEditXmin)
  ON_EN_CHANGE(IDC_EDIT_YMIN, &CDlgCreateLayer::OnEnChangeEditYmin)
  ON_EN_CHANGE(IDC_EDIT_XMAX, &CDlgCreateLayer::OnEnChangeEditXmax)
  ON_EN_CHANGE(IDC_EDIT_YMAX, &CDlgCreateLayer::OnEnChangeEditYmax)
END_MESSAGE_MAP()

void CDlgCreateLayer::OnBnClickedOk() {
  UpdateData(TRUE);
  OnOK();
}

void CDlgCreateLayer::OnNMClickFclsTree(NMHDR* pNMHDR, LRESULT* pResult) {
  *pResult = 0;
  CPoint point;
  UINT flag;
  GetCursorPos(&point);
  m_FclsTree.ScreenToClient(&point);

  HTREEITEM hItem = m_FclsTree.HitTest(point, &flag);
  m_FclsTree.SelectItem(hItem);

  HTREEITEM hParentItem = m_FclsTree.GetParentItem(hItem);
  if (hParentItem == m_hFclsRoot) {
    m_strSelFclsName = m_FclsTree.GetItemText(hItem);
  }

  UpdateData(FALSE);
  *pResult = 0;
}

void CDlgCreateLayer::UpdateFclsTree(void) {
  m_FclsTree.SetRedraw(FALSE);
  m_FclsTree.DeleteAllItems();
  m_FclsTree.SetTextColor(RGB(0, 0, 255));

  m_FclsTree.DeleteItem(m_hFclsRoot);
  m_hFclsRoot = m_FclsTree.InsertItem("Feature types");

  m_FclsTree.InsertItem("Point", m_hFclsRoot);
  m_FclsTree.InsertItem("Child image", m_hFclsRoot);
  m_FclsTree.InsertItem("Annotation", m_hFclsRoot);
  m_FclsTree.InsertItem("Curve", m_hFclsRoot);
  m_FclsTree.InsertItem("Surface", m_hFclsRoot);
  m_FclsTree.InsertItem("Grid", m_hFclsRoot);
  m_FclsTree.InsertItem("Tin", m_hFclsRoot);
  m_FclsTree.InsertItem("Raster", m_hFclsRoot);

  m_FclsTree.Expand(m_hFclsRoot, TVE_EXPAND);
  m_FclsTree.SetRedraw(TRUE);
  m_FclsTree.RedrawWindow();
}

BOOL CDlgCreateLayer::OnInitDialog() {
  CDialog::OnInitDialog();
  UpdateFclsTree();
  return TRUE;
}

UINT CDlgCreateLayer::GetSelFcls(void) {
  UINT unFcls = SmtFtUnknown;

  if (m_strSelFclsName == "Point") {
    unFcls = SmtFtDot;
  } else if (m_strSelFclsName == "Child image") {
    unFcls = SmtFtChildImage;
  } else if (m_strSelFclsName == "Annotation") {
    unFcls = SmtFtAnno;
  } else if (m_strSelFclsName == "Curve") {
    unFcls = SmtFtCurve;
  } else if (m_strSelFclsName == "Surface") {
    unFcls = SmtFtSurface;
  } else if (m_strSelFclsName == "Grid") {
    unFcls = SmtFtGrid;
  } else if (m_strSelFclsName == "Tin") {
    unFcls = SmtFtTin;
  } else if (m_strSelFclsName == "Raster") {
    unFcls = SmtLayer_Ras;
  }

  return unFcls;
}

void CDlgCreateLayer::OnEnChangeEditLayerName() { UpdateData(TRUE); }

void CDlgCreateLayer::OnEnChangeEditXmin() { UpdateData(TRUE); }

void CDlgCreateLayer::OnEnChangeEditYmin() { UpdateData(TRUE); }

void CDlgCreateLayer::OnEnChangeEditXmax() { UpdateData(TRUE); }

void CDlgCreateLayer::OnEnChangeEditYmax() { UpdateData(TRUE); }
