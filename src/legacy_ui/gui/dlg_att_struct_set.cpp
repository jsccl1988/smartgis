// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"
#include "legacy_ui/gui/dlg_att_struct_set.h"

IMPLEMENT_DYNAMIC(CDlgAttStructSet, CDialog)

namespace {

const char* kOgrTypeLabels[] = {"Integer", "Integer64", "Real", "String",
                                "Date",    "Binary"};

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
  ON_NOTIFY(GVN_ENDLABELEDIT, IDC_GRID_ATT_STRUCT,
            &CDlgAttStructSet::OnGridClickEndEdit)
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

  m_attStruGrid.SetTextBkColor(RGB(0xFF, 0xFF, 0xE0));
  m_attStruGrid.SetEditable(TRUE);

  InitAttStructGridHead();
  UpdateAttStructGridContent();
  return TRUE;
}

void CDlgAttStructSet::InitAttStructGridHead() {
  m_attStruGrid.DeleteAllItems();
  m_attStruGrid.SetRowCount(1);
  m_attStruGrid.SetColumnCount(3);
  m_attStruGrid.SetFixedColumnCount(1);

  GV_ITEM item;
  item.mask = GVIF_TEXT | GVIF_FORMAT;
  item.nFormat = DT_CENTER;

  item.row = 0;
  item.col = 0;
  item.strText = _T("序号");
  m_attStruGrid.SetItem(&item);

  item.col++;
  item.strText = _T("字段名称");
  m_attStruGrid.SetItem(&item);

  item.col++;
  item.strText = _T("字段类型");
  m_attStruGrid.SetItem(&item);

  m_attStruGrid.AutoSizeColumns();
  m_attStruGrid.AutoSizeRows();
}

void CDlgAttStructSet::UpdateAttStructGridContent() {
  m_attStruGrid.SetRowCount(static_cast<int>(m_fields.size()) + 1);
  m_attStruGrid.SetFixedRowCount(1);

  GV_ITEM item;
  item.mask = GVIF_TEXT | GVIF_FORMAT;
  item.nFormat = DT_CENTER;

  for (size_t i = 0; i < m_fields.size(); ++i) {
    item.row = static_cast<int>(i) + 1;
    item.col = 0;
    item.strText.Format("%d", static_cast<int>(i) + 1);
    m_attStruGrid.SetItem(&item);

    item.col++;
    item.strText = m_fields[i].name;
    m_attStruGrid.SetItem(&item);
    if (static_cast<int>(i) < m_nFixField) {
      m_attStruGrid.SetItemState(
          item.row, item.col,
          m_attStruGrid.GetItemState(item.row, item.col) | GVIS_READONLY);
    }

    item.col++;
    m_attStruGrid.SetCellType(item.row, item.col,
                              RUNTIME_CLASS(CGridCellCombo));
    CGridCellCombo* pCmb =
        static_cast<CGridCellCombo*>(m_attStruGrid.GetCell(item.row, item.col));
    if (pCmb) {
      pCmb->SetOptions(m_arAllFldNames);
      pCmb->SetText(TypeLabel(m_fields[i].type));
    }
  }
}

void CDlgAttStructSet::SyncFieldsFromGrid() {
  const int rows = m_attStruGrid.GetRowCount() - 1;
  m_fields.clear();
  m_fields.reserve(rows > 0 ? rows : 0);
  for (int i = 0; i < rows; ++i) {
    FieldRow row;
    row.name = m_attStruGrid.GetItemText(i + 1, 1);
    row.name.Trim();
    row.type = TypeFromLabel(m_attStruGrid.GetItemText(i + 1, 2));
    if (!row.name.IsEmpty()) {
      m_fields.push_back(row);
    }
  }
}

void CDlgAttStructSet::OnGridClickEndEdit(NMHDR* pNotifyStruct,
                                          LRESULT* pResult) {
  *pResult = 0;
  NM_GRIDVIEW* pItem = reinterpret_cast<NM_GRIDVIEW*>(pNotifyStruct);

  if (pItem->iRow <= 0 || pItem->iColumn <= 0) {
    return;
  }

  const int index = pItem->iRow - 1;
  if (index < 0 || index >= static_cast<int>(m_fields.size())) {
    return;
  }

  CString strValue = m_attStruGrid.GetItemText(pItem->iRow, pItem->iColumn);
  if (pItem->iColumn == 1) {
    m_fields[index].name = strValue;
  } else if (pItem->iColumn == 2) {
    m_fields[index].type = TypeFromLabel(strValue);
  }

  m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnGridRClick(NMHDR* pNotifyStruct, LRESULT* pResult) {
  NM_GRIDVIEW* pItem = reinterpret_cast<NM_GRIDVIEW*>(pNotifyStruct);
  *pResult = 0;

  if (pItem->iRow > 0) {
    m_selRow = pItem->iRow;
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
}

void CDlgAttStructSet::OnAttstructAppend() {
  FieldRow row;
  row.name = _T("new_field");
  row.type = OFTString;
  m_fields.push_back(row);
  UpdateAttStructGridContent();
  m_attStruGrid.AutoSizeColumns();
  m_attStruGrid.Invalidate();
}

void CDlgAttStructSet::OnAttstructRemove() {
  if (m_selRow <= 0) {
    return;
  }
  const int index = m_selRow - 1;
  if (index < m_nFixField || index >= static_cast<int>(m_fields.size())) {
    return;
  }
  m_fields.erase(m_fields.begin() + index);
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
