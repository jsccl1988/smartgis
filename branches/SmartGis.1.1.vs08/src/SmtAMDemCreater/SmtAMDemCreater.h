// SmtAMDemCreater.h : SmtAMDemCreater DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtAMDemCreaterApp
// �йش���ʵ�ֵ���Ϣ������� SmtAMDemCreater.cpp
//

class CSmtAMDemCreaterApp : public CWinApp
{
public:
	CSmtAMDemCreaterApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
