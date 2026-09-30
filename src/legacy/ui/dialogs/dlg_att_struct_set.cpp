// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy/ui/dialogs/dlg_att_struct_set.h"

IMPLEMENT_DYNAMIC(CDlgAttStructSet, CDialog)

namespace {

const char* kOgrTypeLabels[] = {"Integer", "Integer64", "Real",
                                "String",  "Date",      "Binary"};

constexpr int kOgrTypeLabelCount =
    static_cast<int>(sizeof(kOgrTypeLabels) / sizeof(kOgrTypeLabels[0]));

void clear_list_columns(CMFCListCtrl* list) {
  if (!list) {
    return;
  }
  list->DeleteAllItems();
  while (list->DeleteColumn(0)) {
  }
}

}  // namespace

CDlgAttStructSet::CDlgAttStructSet(CWnd* pParent /*=NULL*/)
    : CDialog(CDlgAttStructSet::IDD, pParent),
      m_layer(NULL),
      m_nFixField(0),
      m_selRow(-1) {}

CDlgAttStructSet::~CDlgAttStructSet() {}

void CDlgAttStructSet::DoDataExchange(CDataExchange* pDX) {
  DDX_Control(pDX, IDC_GRID_ATT_STRUCT, m_attStruGrid);
  CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDlgAttStructSet, CDialog)
ON_BN_CLICKED(IDOK, &CDlgAttStructSet::OnBnClickedOk)
ON_NOTIFY(LVN_ENDLABELEDIT, IDC_GRID_ATT_STRUCT,
          &CDlgAttStructSet::OnEndLabelEdit)
ON_NOTIFY(NM_DBLCLK, IDC_GRID_ATT_STRUCT, &CDlgAttStructSet::OnGridDblClk)
ON_NOTIFY(NM_RCLICK, IDC_GRID_ATT_STRUCT, &CDlgAttStructSet::OnGridRClick)
ON_COMMAND(ID_ATTSTRUCT_APPEND, &CDlgAttStructSet::OnAttstructAppend)
ON_COMMAND(ID_ATTSTRUCT_REMOVE, &CDlgAttStructSet::OnAttstructRemove)
ON_COMMAND(ID_ATTSTRUCT_MOVEUP, &CDlgAttStructSet::OnAttstructMoveup)
ON_COMMAND(ID_ATTSTRUCT_MOVEDOWN, &CDlgAttStructSet::OnAttstructMovedown)
END_MESSAGE_MAP()

void CDlgAttStructSet::FillTypeNames(CStringArray* names) {
  if (!names) {
    return;
  }
  names->RemoveAll();
  for (const char* label : kOgrTypeLabels) {
    names->Add(label);
  }
}

const char* CDlgAttStructSet::TypeLabel(OGRFieldType type) {
  switch (type) {
    case OFTInteger:
      return "Integer";
    case OFTInteger64:
      return "Integer64";
    case OFTReal:
      return "Real";
    case OFTString:
      return "String";
    case OFTDate:
    case OFTDateTime:
      return "Date";
    case OFTBinary:
      return "Binary";
    default:
      return "String";
  }
}

OGRFieldType CDlgAttStructSet::TypeFromLabel(const char* label) {
  if (!label) {
    return OFTString;
  }
  if (_stricmp(label, "Integer") == 0) {
    return OFTInteger;
  }
  if (_stricmp(label, "Integer64") == 0) {
    return OFTInteger64;
  }
  if (_stricmp(label, "Real") == 0 || _stricmp(label, "Double") == 0) {
    return OFTReal;
  }
  if (_stricmp(label, "Date") == 0 || _stricmp(label, "DateTime") == 0) {
    return OFTDate;
  }
  if (_stricmp(label, "Binary") == 0) {
    return OFTBinary;
  }
  return OFTString;
}

void CDlgAttStructSet::OnBnClickedOk() {
  SyncFieldsFromGrid();
  OnOK();
}

BOOL CDlgAttStructSet::OnInitDialog() {
  CDialog::OnInitDialog();

  FillTypeNames(&m_arAllFldNames);
  m_attStruGrid.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

  InitAttStructGridHead();
  UpdateAttStructGridContent();
  return TRUE;
}

void CDlgAttStructSet::InitAttStructGridHead() {
  clear_list_columns(&m_attStruGrid);
  m_attStruGrid.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 100);
  m_attStruGrid.InsertColumn(1, _T("Type"), LVCFMT_LEFT, 80);
}

void CDlgAttStructSet::UpdateAttStructGridContent() {
  m_attStruGrid.DeleteAllItems();
  for (size_t i = 0; i < m_fields.size(); ++i) {
    const int row =
        m_attStruGrid.InsertItem(static_cast<int>(i), m_fields[i].name);
    m_attStruGrid.SetItemText(row, 1, TypeLabel(m_fields[i].type));
  }
}

void CDlgAttStructSet::SyncFieldsFromGrid() {
  const int count = m_attStruGrid.GetItemCount();
  m_fields.clear();
  m_fields.reserve(count > 0 ? count : 0);
  for (int i = 0; i < count; ++i) {
    FieldRow row;
    row.name = m_attStruGrid.GetItemText(i, 0);
    row.name.Trim();
    row.type = TypeFromLabel(m_attStruGrid.GetItemText(i, 1));
    if (!row.name.IsEmpty()) {
      m_fields.push_back(row);
    }
  }
}

void CDlgAttStructSet::cycle_field_type(int index) {
  if (index < 0 || index >= static_cast<int>(m_fields.size()) ||
      index < m_nFixField) {
    return;
  }

  const char* cur = TypeLabel(m_fields[index].type);
  int next = 0;
  for (int i = 0; i < kOgrTypeLabelCount; ++i) {
    if (_stricmp(cur, kOgrTypeLabels[i]) == 0) {
      next = (i + 1) % kOgrTypeLabelCount;
      break;
    }
  }
  m_fields[index].type = TypeFromLabel(kOgrTypeLabels[next]);
  m_attStruGrid.SetItemText(index, 1, TypeLabel(m_fields[index].type));
}

void CDlgAttStructSet::OnEndLabelEdit(NMHDR* pNotifyStruct, LRESULT* pResult) {
  NMLVDISPINFO* info = reinterpret_cast<NMLVDISPINFO*>(pNotifyStruct);
  *pResult = FALSE;
  if (!info || !info->item.pszText) {
    return;
  }

  const int index = info->item.iItem;
  if (index < 0 || index >= static_cast<int>(m_fields.size()) ||
      index < m_nFixField) {
    return;
  }

  CString name = info->item.pszText;
  name.Trim();
  if (name.IsEmpty()) {
    return;
  }

  m_fields[index].name = name;
  m_attStruGrid.SetItemText(index, 0, name);
  *pResult = TRUE;
}

void CDlgAttStructSet::OnGridDblClk(NMHDR* pNotifyStruct, LRESULT* pResult) {
  LPNMITEMACTIVATE item = reinterpret_cast<LPNMITEMACTIVATE>(pNotifyStruct);
  *pResult = 0;
  if (!item || item->iItem < 0) {
    return;
  }
  cycle_field_type(item->iItem);
}

void CDlgAttStructSet::OnGridRClick(NMHDR* pNotifyStruct, LRESULT* pResult) {
  LPNMITEMACTIVATE item = reinterpret_cast<LPNMITEMACTIVATE>(pNotifyStruct);
  *pResult = 0;
  if (!item || item->iItem < 0) {
    return;
  }

  m_selRow = item->iItem;
  CMenu menuMapMgr;
  menuMapMgr.LoadMenu(IDR_MENU_ATTSTRUCT_EDIT);
  CMenu* pMenu = menuMapMgr.GetSubMenu(0);
  if (pMenu) {
    CPoint menuPos;
    GetCursorPos(&menuPos);
    pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
                          menuPos.x, menuPos.y, this);
    pMenu->Detach();
  }
}

void CDlgAttStructSet::OnAttstructAppend() {
  FieldRow row;
  row.name = _T("new_field");
  row.type = OFTString;
  m_fields.push_back(row);
  UpdateAttStructGridContent();
  m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnAttstructRemove() {
  if (m_selRow < 0) {
    return;
  }
  if (m_selRow < m_nFixField ||
      m_selRow >= static_cast<int>(m_fields.size())) {
    return;
  }
  m_fields.erase(m_fields.begin() + m_selRow);
  m_selRow = -1;
  UpdateAttStructGridContent();
  m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnAttstructMoveup() {}

void CDlgAttStructSet::OnAttstructMovedown() {}

void CDlgAttStructSet::SetOgrLayer(OGRLayer* layer, int nFixField) {
  m_layer = layer;
  m_nFixField = nFixField;
  m_fields.clear();
  if (!layer) {
    return;
  }
  OGRFeatureDefn* defn = layer->GetLayerDefn();
  if (!defn) {
    return;
  }
  const int count = defn->GetFieldCount();
  m_fields.reserve(count);
  for (int i = 0; i < count; ++i) {
    OGRFieldDefn* fld = defn->GetFieldDefn(i);
    if (!fld) {
      continue;
    }
    FieldRow row;
    row.name = fld->GetNameRef();
    row.type = fld->GetType();
    m_fields.push_back(row);
  }
}

bool CDlgAttStructSet::ApplyToOgrLayer() {
  SyncFieldsFromGrid();
  if (!m_layer) {
    return false;
  }

  OGRFeatureDefn* defn = m_layer->GetLayerDefn();
  if (!defn) {
    return false;
  }

  // Delete removed fields (reverse order keeps indices stable).
  for (int i = defn->GetFieldCount() - 1; i >= 0; --i) {
    OGRFieldDefn* fld = defn->GetFieldDefn(i);
    if (!fld) {
      continue;
    }
    const char* name = fld->GetNameRef();
    bool keep = false;
    for (const FieldRow& row : m_fields) {
      if (row.name.CompareNoCase(name) == 0) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      m_layer->DeleteField(i);
    }
  }

  // Create / alter remaining fields.
  for (const FieldRow& row : m_fields) {
    const int index = defn->GetFieldIndex(row.name);
    if (index < 0) {
      OGRFieldDefn created(row.name, row.type);
      if (m_layer->CreateField(&created) != OGRERR_NONE) {
        return false;
      }
      continue;
    }
    OGRFieldDefn* existing = defn->GetFieldDefn(index);
    if (existing && existing->GetType() != row.type) {
      // OGRFieldDefn is non-copyable; rebuild from name/type.
      OGRFieldDefn altered(existing->GetNameRef(), row.type);
      altered.SetWidth(existing->GetWidth());
      altered.SetPrecision(existing->GetPrecision());
      if (m_layer->AlterFieldDefn(index, &altered, ALTER_TYPE_FLAG) !=
          OGRERR_NONE) {
        return false;
      }
    }
  }
  return true;
}
