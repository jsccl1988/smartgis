// am_map_project.cpp : ���� DLL �ĳ�ʼ�����̡�
//

#include "stdafx.h"
#include "plugin/legacy/proj/map_project.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

//
//TODO: ����� DLL ����� MFC DLL �Ƕ�̬���ӵģ�
//		��Ӵ� DLL �������κε���
//		MFC �ĺ������뽫 AFX_MANAGE_STATE �����ӵ�
//		�ú�������ǰ�档
//
//		����:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// �˴�Ϊ��ͨ������
//		}
//
//		�˺������κ� MFC ����
//		������ÿ��������ʮ����Ҫ������ζ��
//		��������Ϊ�����еĵ�һ�����
//		���֣������������ж������������
//		������Ϊ���ǵĹ��캯���������� MFC
//		DLL ���á�
//
//		�й�������ϸ��Ϣ��
//		����� MFC ����˵�� 33 �� 58��
//

// CSmtAMMapProjectApp

BEGIN_MESSAGE_MAP(CSmtAMMapProjectApp, CWinApp)
END_MESSAGE_MAP()


// CSmtAMMapProjectApp ����

CSmtAMMapProjectApp::CSmtAMMapProjectApp()
{
	// TODO: �ڴ˴����ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
}


// Ψһ��һ�� CSmtAMMapProjectApp ����

CSmtAMMapProjectApp theApp;


// CSmtAMMapProjectApp ��ʼ��

BOOL CSmtAMMapProjectApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}
