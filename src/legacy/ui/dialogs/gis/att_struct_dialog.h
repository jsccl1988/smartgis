// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#pragma once

#include <vector>

#include "legacy/ui/dialogs/resource.h"
#include "legacy/ui/widgets/feature_pack/feature_pack.h"
#include "ogrsf_frmts.h"

// GIS modal: OGR layer field-schema editor (≈ ui/gis/inspect/AttributeSchemaDialog).
class CDlgAttStructSet : public CDialog {
  DECLARE_DYNAMIC(CDlgAttStructSet)

 public:
  explicit CDlgAttStructSet(CWnd* pParent = NULL);
  ~CDlgAttStructSet() override;

  enum { IDD = IDD_DLG_ATT_STRUCT_SET };

 protected:
  void DoDataExchange(CDataExchange* pDX) override;
  BOOL OnInitDialog() override;

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedOk();
  afx_msg void OnEndLabelEdit(NMHDR* pNotifyStruct, LRESULT* pResult);
  afx_msg void OnGridDblClk(NMHDR* pNotifyStruct, LRESULT* pResult);
  afx_msg void OnGridRClick(NMHDR* pNotifyStruct, LRESULT* pResult);

  afx_msg void OnAttstructAppend();
  afx_msg void OnAttstructRemove();
  afx_msg void OnAttstructMoveup();
  afx_msg void OnAttstructMovedown();

 public:
  // Load editable schema from layer->GetLayerDefn(). Does not take ownership.
  void set_ogr_layer(OGRLayer* layer, int fix_field = 0);
  // Apply grid edits via CreateField / DeleteField / AlterFieldDefn.
  bool apply_to_ogr_layer();

  void init_att_struct_grid_head();
  void update_att_struct_grid_content();

 protected:
  struct FieldRow {
    CString name;
    OGRFieldType type;
  };

  void sync_fields_from_grid();
  void cycle_field_type(int index);

  CMFCListCtrl m_attStruGrid;
  OGRLayer* m_layer;
  std::vector<FieldRow> m_fields;
  int m_nFixField;
  int m_selRow;
};
