// SmtAMMapServiceMgr.h : SmtAMMapServiceMgr DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtAMMapServiceMgrApp
// �йش���ʵ�ֵ���Ϣ������� SmtAMMapServiceMgr.cpp
//

class CSmtAMMapServiceMgrApp : public CWinApp
{
public:
	CSmtAMMapServiceMgrApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
