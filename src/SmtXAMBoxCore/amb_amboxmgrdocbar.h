#ifndef _AMB_AMBOXMGR_H
#define _AMB_AMBOXMGR_H

#include "am_amodulemanager.h"
#include "StackedWndDockBar.h"

using namespace Smt_AM;

namespace Smt_XAMBox
{
	class SMT_EXPORT_CLASS SmtAMBoxMgrDocBar : public CBCGPOutlookBar
	{
	public:
		SmtAMBoxMgrDocBar();
		virtual ~SmtAMBoxMgrDocBar();

	public:
		bool					AddWnd(CWnd* pWnd,CString strTitle);
		CBCGPOutlookWnd*		GetOnerWnd(void) {return  DYNAMIC_DOWNCAST (CBCGPOutlookWnd,GetUnderlinedWindow ());}

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

#if !defined(Export_SmtXAMBoxCore)
#if     defined( _DEBUG)
#          pragma comment(lib,"SmtXAMBoxCoreD.lib")
#       else
#          pragma comment(lib,"SmtXAMBoxCore.lib")
#	    endif
#endif

#endif //_AMB_AMBOXMGR_H