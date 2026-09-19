#pragma once

#include <vector>

#include "legacy/ui/gui/resource.h"
#include "legacy/ui/mfc_ex/grid_ctrl_support.h"
#include "ogrsf_frmts.h"

// MFC leftover field-schema editor backed by OGRFeatureDefn / OGRLayer.
class CDlgAttStructSet : public CDialog {
  DECLARE_DYNAMIC(CDlgAttStructSet)

 public:
  explicit CDlgAttStructSet(CWnd* pParent = NULL);
  virtual ~CDlgAttStructSet();

  enum { IDD = IDD_DLG_ATT_STRUCT_SET };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);
  virtual BOOL OnInitDialog();

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedOk();
  afx_msg void OnGridClickEndEdit(NMHDR* pNotifyStruct, LRESULT* pResult);
  afx_msg void OnGridRClick(NMHDR* pNotifyStruct, LRESULT* pResult);

  afx_msg void OnAttstructAppend();
  afx_msg void OnAttstructRemove();
  afx_msg void OnAttstructMoveup();
  afx_msg void OnAttstructMovedown();

 public:
  // Load editable schema from layer->GetLayerDefn(). Does not take ownership.
  void SetOgrLayer(OGRLayer* layer, int nFixField = 0);
  // Apply grid edits via CreateField / DeleteField / AlterFieldDefn.
  bool ApplyToOgrLayer();

  void InitAttStructGridHead();
  void UpdateAttStructGridContent();

 protected:
  struct FieldRow {
    CString name;
    OGRFieldType type;
  };

  void SyncFieldsFromGrid();
  static void FillTypeNames(CStringArray* names);
  static const char* TypeLabel(OGRFieldType type);
  static OGRFieldType TypeFromLabel(const char* label);

  CGridCtrl m_attStruGrid;
  CStringArray m_arAllFldNames;
  OGRLayer* m_layer;
  std::vector<FieldRow> m_fields;
  int m_nFixField;
  int m_selRow;
};
