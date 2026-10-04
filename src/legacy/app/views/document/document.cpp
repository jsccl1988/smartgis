#include "legacy/app/stdafx.h"

#include "legacy/app/views/document/document.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CSmartGisDoc, CDocument)

BEGIN_MESSAGE_MAP(CSmartGisDoc, CDocument)
END_MESSAGE_MAP()

CSmartGisDoc::CSmartGisDoc() { m_hCurMainMenu = NULL; }

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
