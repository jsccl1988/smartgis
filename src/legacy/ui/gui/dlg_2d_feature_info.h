#pragma once

#include "legacy/ui/mfc_ex/grid_ctrl_support.h"
// CDlg2DFeatureInfo �Ի���
#include "legacy/ui/gui/resource.h"
#include "gis/feature/feature.h"
using namespace gis;

class CDlg2DFeatureInfo : public CDialog {
  DECLARE_DYNAMIC(CDlg2DFeatureInfo)

 public:
  CDlg2DFeatureInfo(CWnd* pParent = NULL);  // ��׼���캯��
  virtual ~CDlg2DFeatureInfo();

  // �Ի�������
  enum { IDD = IDD_DLG_2DFEATURE_INFO };

 protected:
  virtual void DoDataExchange(CDataExchange* pDX);  // DDX/DDV ֧��
  virtual BOOL OnInitDialog();

  DECLARE_MESSAGE_MAP()
 public:
  afx_msg void OnBnClickedOk();

 public:
  // Leftover MFC shell: holds SmtFeature* for read-only display only.
  // Product attribute writeback goes through content::feature_attrs
  // (AttributeTable / MapScene), not this dialog.
  void SetFeature(SmtFeature* pSmtFea) { m_pSmtFea = pSmtFea; }

  void InitAttGridHead();
  void UpdateAttGridContent();

  void UpdateGeomInfo();

 protected:
  CGridCtrl m_attGrid;
  CStatic m_geomInfo;

  SmtFeature* m_pSmtFea;
};
