#pragma once

#include "legacy/ui/xcatalog/resource.h"
// CDlgCreateLayer �Ի���
#include "base/core/bas_struct.h"

class CDlgCreateLayer : public CDialog {
  DECLARE_DYNAMIC(CDlgCreateLayer)

 public:
  CDlgCreateLayer(CWnd* pParent = NULL);  // ��׼���캯��
  virtual ~CDlgCreateLayer();

  // �Ի�������
  enum { IDD = IDD_DLG_CREATE_LAYER };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);  // DDX/DDV ֧��

  DECLARE_MESSAGE_MAP()
 public:
  virtual BOOL OnInitDialog();
  afx_msg void OnBnClickedOk();
  afx_msg void OnNMClickFclsTree(NMHDR* pNMHDR, LRESULT* pResult);

  afx_msg void OnEnChangeEditLayerName();
  afx_msg void OnEnChangeEditXmin();
  afx_msg void OnEnChangeEditYmin();
  afx_msg void OnEnChangeEditXmax();
  afx_msg void OnEnChangeEditYmax();

  CString GetLayerName(void) { return m_strLayerName; }
  CString GetSelFclsName(void) { return m_strSelFclsName; }
  UINT GetSelFcls(void);
  base::fRect GetLayerRect(void) { return m_lyrRect; }

 private:
  void UpdateFclsTree(void);

 private:
  CString m_strSelFclsName;
  CString m_strLayerName;

 protected:
  CTreeCtrl m_FclsTree;
  HTREEITEM m_hFclsRoot;

 protected:
  base::fRect m_lyrRect;
};
