// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/dlg_2d_feature_info.h"

#include "gis/model/envelope.h"
#include "ogrsf_frmts.h"

using namespace gis;

IMPLEMENT_DYNAMIC(CDlg2DFeatureInfo, CDialog)

CDlg2DFeatureInfo::CDlg2DFeatureInfo(CWnd* pParent /*=NULL*/)
    : CDialog(CDlg2DFeatureInfo::IDD, pParent), m_pSmtFea(NULL) {}

CDlg2DFeatureInfo::~CDlg2DFeatureInfo() { m_pSmtFea = NULL; }

void CDlg2DFeatureInfo::DoDataExchange(CDataExchange* pDX) {
  DDX_Control(pDX, IDC_STEXT_GEOM, m_geomInfo);
  DDX_Control(pDX, IDC_EDIT_ATT_FILTER, m_filter_edit);
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlg2DFeatureInfo, CDialog)
ON_BN_CLICKED(IDOK, &CDlg2DFeatureInfo::OnBnClickedOk)
ON_EN_CHANGE(IDC_EDIT_ATT_FILTER, &CDlg2DFeatureInfo::OnEnChangeAttFilter)
END_MESSAGE_MAP()

bool CDlg2DFeatureInfo::create_att_property_grid() {
  CWnd* place = GetDlgItem(IDC_GRID_ATT);
  if (!place) {
    return false;
  }
  CRect rect;
  place->GetWindowRect(&rect);
  ScreenToClient(&rect);
  place->DestroyWindow();

  if (!m_attGrid.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER, rect,
                        this, IDC_GRID_ATT)) {
    return false;
  }
  m_attGrid.EnableHeaderCtrl(FALSE);
  m_attGrid.EnableDescriptionArea(TRUE);
  m_attGrid.SetVSDotNetLook(TRUE);
  m_attGrid.MarkModifiedProperties(FALSE);
  return true;
}

BOOL CDlg2DFeatureInfo::OnInitDialog() {
  CDialog::OnInitDialog();

  if (!create_att_property_grid()) {
    return FALSE;
  }

  if (m_pSmtFea) {
    UpdateGeomInfo();
    InitAttGridHead();
    UpdateAttGridContent();
  }

  return TRUE;
}

void CDlg2DFeatureInfo::UpdateGeomInfo() {
  if (!m_pSmtFea) {
    return;
  }
  CString strGeomInfo;
  OGRGeometry* pSmtGeom = m_pSmtFea->getGeometryRef();
  if (!pSmtGeom) {
    return;
  }
  if (pSmtGeom->getGeometryType() == wkbPoint) {
    OGRPoint* pPoint = pSmtGeom->toPoint();
    strGeomInfo.Format("  类型:%s\n  x:%f\ty:%f", pSmtGeom->getGeometryName(),
                       pPoint->getX(), pPoint->getY());
  } else {
    OGREnvelope env;
    pSmtGeom->getEnvelope(&env);
    strGeomInfo.Format("  类型:%s\n  x min:%f\ty min:%f\n  x max:%f\ty max:%f",
                       pSmtGeom->getGeometryName(), env.MinX, env.MinY,
                       env.MaxX, env.MaxY);
  }

  m_geomInfo.SetWindowText(strGeomInfo);
}

void CDlg2DFeatureInfo::InitAttGridHead() { m_attGrid.RemoveAll(); }

void CDlg2DFeatureInfo::rebuild_property_groups() {
  m_attGrid.RemoveAll();
  if (!m_pSmtFea) {
    return;
  }

  OGRGeometry* geom = m_pSmtFea->getGeometryRef();
  CMFCPropertyGridProperty* geom_group =
      new CMFCPropertyGridProperty(_T("几何"));
  if (geom) {
    CMFCPropertyGridProperty* type_prop = new CMFCPropertyGridProperty(
        _T("类型"), (_variant_t)(LPCTSTR)geom->getGeometryName(),
        _T("OGR geometry type"));
    type_prop->AllowEdit(FALSE);
    geom_group->AddSubItem(type_prop);
    if (geom->getGeometryType() == wkbPoint) {
      OGRPoint* pt = geom->toPoint();
      CString xs;
      xs.Format(_T("%f"), pt->getX());
      CString ys;
      ys.Format(_T("%f"), pt->getY());
      CMFCPropertyGridProperty* x_prop =
          new CMFCPropertyGridProperty(_T("X"), (_variant_t)(LPCTSTR)xs, _T(""));
      CMFCPropertyGridProperty* y_prop =
          new CMFCPropertyGridProperty(_T("Y"), (_variant_t)(LPCTSTR)ys, _T(""));
      x_prop->AllowEdit(FALSE);
      y_prop->AllowEdit(FALSE);
      geom_group->AddSubItem(x_prop);
      geom_group->AddSubItem(y_prop);
    } else {
      OGREnvelope env;
      geom->getEnvelope(&env);
      CString vmin;
      vmin.Format(_T("%f, %f"), env.MinX, env.MinY);
      CString vmax;
      vmax.Format(_T("%f, %f"), env.MaxX, env.MaxY);
      CMFCPropertyGridProperty* min_prop = new CMFCPropertyGridProperty(
          _T("Min"), (_variant_t)(LPCTSTR)vmin, _T("envelope min"));
      CMFCPropertyGridProperty* max_prop = new CMFCPropertyGridProperty(
          _T("Max"), (_variant_t)(LPCTSTR)vmax, _T("envelope max"));
      min_prop->AllowEdit(FALSE);
      max_prop->AllowEdit(FALSE);
      geom_group->AddSubItem(min_prop);
      geom_group->AddSubItem(max_prop);
    }
  }
  m_attGrid.AddProperty(geom_group);

  OGRFeature* ogr = m_pSmtFea->ogr();
  if (!ogr) {
    m_attGrid.ExpandAll();
    return;
  }
  OGRFeatureDefn* defn = ogr->GetDefnRef();
  if (!defn) {
    m_attGrid.ExpandAll();
    return;
  }

  CString filter = m_filter_text;
  filter.MakeLower();

  CMFCPropertyGridProperty* attr_group =
      new CMFCPropertyGridProperty(_T("属性"));
  const int field_count = defn->GetFieldCount();
  for (int i = 0; i < field_count; i++) {
    OGRFieldDefn* fld = defn->GetFieldDefn(i);
    if (!fld) {
      continue;
    }
    CString name = fld->GetNameRef();
    if (!filter.IsEmpty()) {
      CString lower = name;
      lower.MakeLower();
      if (lower.Find(filter) < 0) {
        continue;
      }
    }
    const CString type_name = OGRFieldDefn::GetFieldTypeName(fld->GetType());
    const CString value =
        ogr->IsFieldSet(i) ? ogr->GetFieldAsString(i) : _T("");
    CMFCPropertyGridProperty* prop = new CMFCPropertyGridProperty(
        name, (_variant_t)(LPCTSTR)value, type_name);
    prop->AllowEdit(FALSE);
    attr_group->AddSubItem(prop);
  }
  m_attGrid.AddProperty(attr_group);
  m_attGrid.ExpandAll();
}

void CDlg2DFeatureInfo::UpdateAttGridContent() { rebuild_property_groups(); }

void CDlg2DFeatureInfo::OnEnChangeAttFilter() {
  m_filter_edit.GetWindowText(m_filter_text);
  rebuild_property_groups();
}

void CDlg2DFeatureInfo::OnBnClickedOk() { OnOK(); }
