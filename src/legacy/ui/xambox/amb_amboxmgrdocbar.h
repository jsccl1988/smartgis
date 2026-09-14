#ifndef _AMB_AMBOXMGR_H
#define _AMB_AMBOXMGR_H
#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif


#include "plugin/legacy/module_manager.h"
#include "legacy/ui/mfc_ex/stacked_wnd_dock_bar.h"

using namespace plugin;

namespace ui
{
	class XAMBOX_EXPORT SmtAMBoxMgrDocBar : public CBCGPOutlookBar
	{
	public:
		SmtAMBoxMgrDocBar();
		virtual ~SmtAMBoxMgrDocBar();

	public:
		bool					AddWnd(CWnd* pWnd,CString strTitle);
		CBCGPOutlookWnd*		GetOnerWnd(void) {return  DYNAMIC_DOWNCAST (CBCGPOutlookWnd,GetUnderlyingWindow ());}

	protected:
		afx_msg void			OnContextMenu(CWnd* /*pWnd*/, CPoint /*point*/);

		DECLARE_MESSAGE_MAP()

	public:
		bool					UpdateAMBoxs(void);

	protected:
		bool					CreateAMBox(SmtAuxModule* pAModule,int nID);

	protected:
		vector<CWnd*>			m_vWndPtrs;
		int						m_nToolBoxPage;
	};
}

#if !defined(XAMBOX_EXPORTS)
#if     defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif
#endif

#endif //_AMB_AMBOXMGR_H