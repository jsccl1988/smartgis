#pragma once


// CDlgMapPrjDoGrid 对话框
#include "prj_projection.h"
#include "geo_geometry.h"
using namespace Smt_Prj;
using namespace Smt_Geo;

class CDlgMapPrjDoGrid : public CDialog
{
	DECLARE_DYNAMIC(CDlgMapPrjDoGrid)

public:
	CDlgMapPrjDoGrid(PrjX *pPrj,CWnd* pParent = NULL);   // 标准构造函数
	virtual ~CDlgMapPrjDoGrid();

	// 对话框数据
	enum { IDD = IDD_DLG_PRJ_DOGRID };

protected:
	virtual void		DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void		OnBnClickedBtnDogrid();

	void				OutputRes(SmtGrid &grid);

private:
	double				m_fDL;
	double				m_fDB;
	double				m_fLmin;
	double				m_fBmin;
	double				m_fLmax;
	double				m_fBmax;
	long				m_lScaleRuler;

	Projection			*m_pPrj;
};
