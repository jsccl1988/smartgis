#include "legacy/app/stdafx.h"

#include "legacy/app/doc/smart_gis_doc.h"

#include "base/core/log.h"
#include "legacy/app/shell/frame/app.h"
#include "legacy/app/view/map/map.h"
#include "legacy/app/view/edit/edit.h"

using namespace base;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


IMPLEMENT_DYNCREATE(CSmartGisDoc, CDocument)

BEGIN_MESSAGE_MAP(CSmartGisDoc, CDocument)

END_MESSAGE_MAP()


CSmartGisDoc::CSmartGisDoc() {
  m_hCurMainMenu = NULL;
}

CSmartGisDoc::~CSmartGisDoc() {}

BOOL CSmartGisDoc::OnNewDocument() {
  if (!CDocument::OnNewDocument()) return FALSE;


  return TRUE;
}


void CSmartGisDoc::Serialize(CArchive& ar) {
  if (ar.IsStoring()) {
  } else {
  }
}

HMENU CSmartGisDoc::GetDefaultMenu() { return m_hCurMainMenu; }

#ifdef _DEBUG
void CSmartGisDoc::AssertValid() const { CDocument::AssertValid(); }

void CSmartGisDoc::Dump(CDumpContext& dc) const { CDocument::Dump(dc); }
#endif  //_DEBUG

