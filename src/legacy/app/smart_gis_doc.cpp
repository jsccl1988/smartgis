// SmartGisDoc.cpp : CSmartGisDoc
// 缁祽鍒婇惃璇垔鐎硅鍒婇悳?//

#include "legacy/app/stdafx.h"
#include "legacy/app/smart_gis_doc.h"

#include "base/core/log.h"
#include "legacy/app/smart_gis.h"
#include "legacy/app/smart_gis_view.h"
#include "legacy/app/smart_map_edit_view.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSmartGisDoc

IMPLEMENT_DYNCREATE(CSmartGisDoc, CDocument)

BEGIN_MESSAGE_MAP(CSmartGisDoc, CDocument)

END_MESSAGE_MAP()

// CSmartGisDoc 鑲燁垱鐏茬酱鐘界妸/鑲燁垱鐏茶偀顖涚伈

CSmartGisDoc::CSmartGisDoc() {
  // TODO:
  // 閸︺劍顒濊偀顖涚潱鑲燁垰濞婃稉鈧▎鈩冣偓褖鍒婇弸绛嬬炊鏀典京鍒婃禍鐣岀垳
  m_hCurMainMenu = NULL;
}

CSmartGisDoc::~CSmartGisDoc() {}

BOOL CSmartGisDoc::OnNewDocument() {
  if (!CDocument::OnNewDocument()) return FALSE;

  // TODO:
  // 閸︺劍顒濊偀顖涚潱鑲燁垰濞婇柌宥嬪垔閺傝棄鍨佃偀顖氼瀴閸栨牤鍒婃禍鐣岀垳
  // (SDI 鑲燁垱鏌﹀锝呯殺闁插秶鏁ょ拠銉︽瀮锟?

  return TRUE;
}

// CSmartGisDoc 鑲燁垰鑸崚妤€锟?

void CSmartGisDoc::Serialize(CArchive& ar) {
  if (ar.IsStoring()) {
    // TODO:
    // 閸︺劍顒濊偀顖涚潱鑲燁垰濞婄€涙﹤鍒婇崒璇垔娴滅晫锟?
  } else {
    // TODO: load code
  }
}

HMENU CSmartGisDoc::GetDefaultMenu() { return m_hCurMainMenu; }

// CSmartGisDoc 鐠囧鍒婇弬?
#ifdef _DEBUG
void CSmartGisDoc::AssertValid() const { CDocument::AssertValid(); }

void CSmartGisDoc::Dump(CDumpContext& dc) const { CDocument::Dump(dc); }
#endif  //_DEBUG

// CSmartGisDoc
// 閸涜鍒婃禍?//////////////////////////////////////////////////////////////////////////
