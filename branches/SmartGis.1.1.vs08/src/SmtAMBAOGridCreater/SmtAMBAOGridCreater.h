// SmtAMBAOGridCreater.h : SmtAMBAOGridCreater DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtAMBAOGridCreaterApp
// �йش���ʵ�ֵ���Ϣ������� SmtAMBAOGridCreater.cpp
//

class CSmtAMBAOGridCreaterApp : public CWinApp
{
public:
	CSmtAMBAOGridCreaterApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
