// SmtXAMBoxCore.h : SmtXAMBoxCore DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtXAMBoxCoreApp
// �йش���ʵ�ֵ���Ϣ������� SmtXAMBoxCore.cpp
//

class CSmtXAMBoxCoreApp : public CWinApp
{
public:
	CSmtXAMBoxCoreApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
