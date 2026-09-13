// am_3d_model_creater.cpp : ���� DLL �ĳ�ʼ�����̡�
//

#include "stdafx.h"
#include "plugin/model3d/model_3d_creater.h"

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

// CSmtAM3DModelCreaterApp

BEGIN_MESSAGE_MAP(CSmtAM3DModelCreaterApp, CWinApp)
END_MESSAGE_MAP()


// CSmtAM3DModelCreaterApp ����

CSmtAM3DModelCreaterApp::CSmtAM3DModelCreaterApp()
{
	// TODO: �ڴ˴����ӹ�����룬
	// ��������Ҫ�ĳ�ʼ�������� InitInstance ��
}


// Ψһ��һ�� CSmtAM3DModelCreaterApp ����

CSmtAM3DModelCreaterApp theApp;


// CSmtAM3DModelCreaterApp ��ʼ��

BOOL CSmtAM3DModelCreaterApp::InitInstance()
{
	CWinApp::InitInstance();

	return TRUE;
}
