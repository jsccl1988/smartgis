// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/detail/feature_info_grid.h"

#include "ogrsf_frmts.h"

namespace ui {
namespace detail {

CString format_feature_geom_summary(gis::Feature* feature) {
  CString summary;
  if (!feature) {
    return summary;
  }
  OGRGeometry* geom = feature->geometry();
  if (!geom) {
    return summary;
  }
  if (geom->getGeometryType() == wkbPoint) {
    OGRPoint* point = geom->toPoint();
    summary.Format("  类型:%s\n  x:%f\ty:%f", geom->getGeometryName(),
                   point->getX(), point->getY());
  } else {
    OGREnvelope env;
    geom->getEnvelope(&env);
    summary.Format("  类型:%s\n  x min:%f\ty min:%f\n  x max:%f\ty max:%f",
                   geom->getGeometryName(), env.MinX, env.MinY, env.MaxX,
                   env.MaxY);
  }
  return summary;
}

void rebuild_feature_info_grid(CMFCPropertyGridCtrl* grid, gis::Feature* feature,
                               const CString& name_filter) {
  if (!grid) {
    return;
  }
  grid->RemoveAll();
  if (!feature) {
    return;
  }

  OGRGeometry* geom = feature->geometry();
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
  grid->AddProperty(geom_group);

  OGRFeature* ogr = feature->ogr();
  if (!ogr) {
    grid->ExpandAll();
    return;
  }
  OGRFeatureDefn* defn = ogr->GetDefnRef();
  if (!defn) {
    grid->ExpandAll();
    return;
  }

  CString filter = name_filter;
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
  grid->AddProperty(attr_group);
  grid->ExpandAll();
}

}  // namespace detail
}  // namespace ui
