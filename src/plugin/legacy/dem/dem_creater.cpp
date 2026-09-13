// am_dem_creater.cpp : ���� DLL �ĳ�ʼ�����̡�
//

#include "stdafx.h"
#include "plugin/legacy/dem/dem_creater.h"

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

// CSmtAMDemCreaterApp

BEGIN_MESSAGE_MAP(CSmtAMDemCreaterApp, CWinApp)
END_MESSAGE_MAP()


// CSmtAMDemCreaterApp ����

CSmtAMDemCreaterApp::CSmtAMDemCreaterApp()
{
	// TODO: �ڴ˴����ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
}


// Ψһ��һ�� CSmtAMDemCreaterApp ����

CSmtAMDemCreaterApp theApp;


// CSmtAMDemCreaterApp ��ʼ��

BOOL CSmtAMDemCreaterApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}
