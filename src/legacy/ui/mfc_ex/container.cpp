// container.cpp : ʵ���ļ�
//

#include "stdafx.h"

#include "legacy/ui/mfc_ex/container.h"

// CContainer

IMPLEMENT_DYNAMIC(CContainer, CStatic)

CContainer::CContainer() {}

CContainer::~CContainer() {}

BEGIN_MESSAGE_MAP(CContainer, CStatic)
END_MESSAGE_MAP()

// CContainer ��Ϣ��������
BOOL CContainer::PreTranslateMessage(MSG *pMsg) {
  // TODO: �ڴ�����ר�ô����/����û���
  CWnd *pParentWnd = GetParent();

  if (NULL != pParentWnd)
    return pParentWnd->PreTranslateMessage(pMsg);
  else
    return CStatic::PreTranslateMessage(pMsg);
}

BOOL CContainer::OnCommand(WPARAM wParam, LPARAM lParam) {
  // TODO: �ڴ�����ר�ô����/����û���
  CWnd *pParentWnd = GetParent();

  if (NULL != pParentWnd)
    return pParentWnd->PostMessage(WM_COMMAND, wParam, lParam);
  else
    return CStatic::OnCommand(wParam, lParam);
}
