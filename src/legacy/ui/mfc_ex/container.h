#pragma once


// CContainer

class AFX_EXT_CLASS CContainer : public CStatic
{
	DECLARE_DYNAMIC(CContainer)

public:
	CContainer();
	virtual ~CContainer();

protected:
	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

protected:
	virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam);
};


#if !defined(MFC_EX_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif  
#endif


