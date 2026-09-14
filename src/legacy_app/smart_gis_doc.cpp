// SmartGisDoc.cpp : CSmartGisDoc 类뿯皽뿯宽뿯玽
//

#include "legacy_app/stdafx.h"
#include "legacy_app/smart_gis.h"

#include "legacy_app/smart_gis_doc.h"
#include "legacy_app/smart_gis_view.h"
#include "legacy_app/smart_map_edit_view.h"

#include "base/core/logmanager.h"

using namespace base;


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSmartGisDoc

IMPLEMENT_DYNCREATE(CSmartGisDoc, CDocument)

BEGIN_MESSAGE_MAP(CSmartGisDoc, CDocument)
	

END_MESSAGE_MAP()


// CSmartGisDoc 뿯枽붿/뿯枽뿯枽

CSmartGisDoc::CSmartGisDoc()
{
	// TODO: 在此뿯涽뿯劽一次性뿯枽붿뿯亽码
	m_hCurMainMenu = NULL;
}

CSmartGisDoc::~CSmartGisDoc()
{

}

BOOL CSmartGisDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: 在此뿯涽뿯劽重뿯施初뿯妽化뿯亽码
	// (SDI 뿯施档将重用该文档)

	return TRUE;
}

// CSmartGisDoc 뿯庽列化

void CSmartGisDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: 在此뿯涽뿯劽存뿯傽뿯亽码
	}
	else
	{
		// TODO: 在此뿯涽뿯劽뿯劽붿뿯亽码
	}
}

HMENU CSmartGisDoc::GetDefaultMenu()
{
	return m_hCurMainMenu;
}

// CSmartGisDoc 诊뿯施

#ifdef _DEBUG
void CSmartGisDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CSmartGisDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CSmartGisDoc 命뿯亽
//////////////////////////////////////////////////////////////////////////
