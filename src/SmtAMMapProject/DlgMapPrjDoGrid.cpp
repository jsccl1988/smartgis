// DlgMapPrjDoGrid.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtAMMapProject.h"
#include "DlgMapPrjDoGrid.h"

#include <math.h>
#include <fstream>
#include <iomanip> 

#include "bl_stylemanager.h"
#include "cata_mapmgr.h"
#include "gis_feature.h"
#include "gis_map.h"
#include "gis_sde.h"
#include "geo_geometry.h"

#include "prj_projection.h"
#include "sys_sysmanager.h"
#include "smt_msg.h"
#include "gt_defs.h"
#include "smt_api.h"
#include "am_msg.h"
#include "t_msg.h"

using namespace Smt_Core;
using namespace Smt_GIS;
using namespace Smt_Geo;
using namespace Smt_Prj;
using namespace Smt_Sys;
using namespace Smt_XCatalog;

// CDlgMapPrjDoGrid 对话框

IMPLEMENT_DYNAMIC(CDlgMapPrjDoGrid, CDialog)

CDlgMapPrjDoGrid::CDlgMapPrjDoGrid(PrjX *pPrjX,CWnd* pParent /*=NULL*/)
: CDialog(CDlgMapPrjDoGrid::IDD, pParent)
, m_fDL(1/8.)
, m_fDB(1/12.)
, m_fLmin(117.)
, m_fBmin(36.)
, m_fLmax(118.)
, m_fBmax(37.)
, m_lScaleRuler(10000)
{
	m_pPrj = new Projection(pPrjX);
}

CDlgMapPrjDoGrid::~CDlgMapPrjDoGrid()
{
	SMT_SAFE_DELETE(m_pPrj);
}

void CDlgMapPrjDoGrid::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DL, m_fDL);
	DDX_Text(pDX, IDC_EDIT_DB, m_fDB);

	DDX_Text(pDX, IDC_EDIT_LMIN, m_fLmin);
	DDX_Text(pDX, IDC_EDIT_BMIN, m_fBmin);
	DDX_Text(pDX, IDC_EDIT_LMAX, m_fLmax);
	DDX_Text(pDX, IDC_EDIT_BMAX, m_fBmax);

	DDX_Text(pDX, IDC_EDIT_SCALE, m_lScaleRuler);
}


BEGIN_MESSAGE_MAP(CDlgMapPrjDoGrid, CDialog)
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDC_BTN_DOGRID, &CDlgMapPrjDoGrid::OnBnClickedBtnDogrid)
END_MESSAGE_MAP()


// CDlgMapPrjDoGrid 消息处理程序

void CDlgMapPrjDoGrid::OutputRes(SmtGrid &grid)
{
	string strAppTempPath = GetAppTempPath();
	strAppTempPath += "GridRes.txt";
	fstream  fOut;
	fOut.open(strAppTempPath.c_str(),ios::out);
	if (fOut.is_open())
	{
		Matrix2D<RawPoint>  *pNodes = grid.GetGridNodeBuf();

		int nN,nM;
		grid.GetSize(nM,nN);

		fOut<< "minL:" << m_fLmin << "   maxL:" << m_fLmax << endl;
		fOut<< "minB:" << m_fBmin << "   maxB:" << m_fBmax << endl;
		fOut<< "Scale Ruler:  1:" << m_lScaleRuler << endl;
		fOut<< "Grid Size:" << nN << "   " << nM<<endl;

		int r = 4;
		double l,b;

		for (int j = 0; j < nN; j++)
		{
			l = m_fLmin + j*m_fDL;
			fOut<< setprecision(4)<< "\t" << l << "\t" ;	
		}
		fOut << endl;

		for (int i = nM-1; i > -1; i --)
		{
			b = m_fBmin + i*m_fDB;
			fOut << setprecision(4) << b;
			for (int j = 0; j < nN; j++)
			{
				RawPoint rawPt = pNodes->GetElement(i,j);
				fOut<< setprecision(4) <<"\t(" << rawPt.x << "," << rawPt.y<< ")";	
			}
			fOut << endl;
		}
		fOut.close();
	}
}

void CDlgMapPrjDoGrid::OnBnClickedBtnDogrid()
{
	// TODO: 在此添加控件通知处理程序代码
	UpdateData(TRUE);
	
	SmtMapMgr *pSmtMapMgr = SmtMapMgr::GetSingletonPtr();
	SmtLayer *pLayer = pSmtMapMgr->GetActiveLayer();

	if (NULL == pLayer || LYR_VECTOR != pLayer->GetLayerType())
		return;

	SmtVectorLayer *pVLayer = (SmtVectorLayer *)pLayer;

	if(pVLayer && pVLayer->GetLayerFeatureType() == SmtFtGrid)
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();
		
		SmtFeature *pSmtFeature = new SmtFeature;
		SmtGrid     oSmtGrid;

		m_pPrj->SetPrjPra(m_fDB,m_fDL,m_fBmax,m_fBmin,m_fLmax,m_fLmin,m_lScaleRuler); // gauss
		m_pPrj->DoGridPrj(oSmtGrid);
		OutputRes(oSmtGrid);

		pSmtFeature->SetFeatureType(SmtFeatureType::SmtFtGrid);
		pSmtFeature->SetStyle(styleSonfig.szPointStyle);
		pSmtFeature->SetGeometry(&oSmtGrid);

		if (pSmtMapMgr->AppendFeature(pSmtFeature,false))
		{
			SmtListenerMsg param;
			param.hSrcWnd = m_hWnd;
			::MessageBox(::GetActiveWindow(), "生成成功!", "提示", MB_OK);
			SmtPostIAToolMsg(SMT_IATOOL_MSG_BROADCAST,GT_MSG_VIEW_ZOOMREFRESH,param);
		}
		else
			SMT_SAFE_DELETE(pSmtFeature);
	}
	else
		::MessageBox(::GetActiveWindow(), "请激活GRID图层!", "提示", MB_OK);
}
