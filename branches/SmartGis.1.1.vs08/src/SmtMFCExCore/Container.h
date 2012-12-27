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


#if !defined(Export_SmtMFCExCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMFCExCoreD.lib")
#       else
#          pragma comment(lib,"SmtMFCExCore.lib")
#	    endif  
#endif


