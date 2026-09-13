// am_map_print.h : SmtAMMapPrint DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "plugin/print/resource.h"		// ������


// CSmtAMMapPrintApp
// �йش���ʵ�ֵ���Ϣ������� am_map_print.cpp
//

class CSmtAMMapPrintApp : public CWinApp
{
public:
	CSmtAMMapPrintApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
