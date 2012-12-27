#include "stdafx.h"

#include "sta_chart.h"
#include "bl_stylemanager.h"
#include "sys_sysmanager.h"
#include "smt_api.h"

using namespace Smt_Sys;

namespace Smt_StaDiagram
{
	const double				PI						=  3.14159265;   
	const string				c_str_anno_lyr			="SMT_CHART_ANNO";
	const string				c_str_pnt_lyr			="SMT_CHART_PNT";
	const string				c_str_line_lyr			="SMT_CHART_LINE";
	const string				c_str_reg_lyr			="SMT_CHART_REG";

	//////////////////////////////////////////////////////////////////////////
	//SmtChart
	SmtChart::SmtChart():m_fDivWidth(40)
		,m_fDivHeight(40)
		,m_fXOffSet(40)
		,m_fYOffSet(40)
		,m_bShowAxis(true)
		,m_bShowTitle(true)
		,m_bShowGridLines(true)
	{
		;
	}

	SmtChart::~SmtChart()
	{

	}

	//////////////////////////////////////////////////////////////////////////
	long SmtChart::Init()
	{
		SmtSysManager * pSysMgr = SmtSysManager::GetSingletonPtr();
		SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		SmtStyleConfig &styleSonfig = pSysMgr->GetSysStyleConfig();
		SmtStyle *pStyle = NULL;

		pStyle = pStyleMgr->GetStyle(styleSonfig.szPointStyle);
		if (pStyle)
		{	
			SmtPenDesc        stPenDesc;
			SmtBrushDesc      stBrushDesc;
			SmtAnnotationDesc trAnnoDesc;

			memcpy(&m_styChart,pStyle,sizeof(SmtStyle));
			memcpy(&m_styTitle,pStyle,sizeof(SmtStyle));
			memcpy(&m_styAxis,pStyle,sizeof(SmtStyle));
			memcpy(&m_styPanel,pStyle,sizeof(SmtStyle));
			memcpy(&m_styGridPoint,pStyle,sizeof(SmtStyle));
			memcpy(&m_styDataPoint,pStyle,sizeof(SmtStyle));
			
			m_styChart.SetStyleType(ST_PenDesc);
			m_styTitle.SetStyleType(ST_AnnoDesc);
			m_styRule.SetStyleType(ST_PenDesc|ST_AnnoDesc);
			m_styAxis.SetStyleType(ST_PenDesc|ST_AnnoDesc);
			m_styPanel.SetStyleType(ST_PenDesc|ST_AnnoDesc);
			m_styGridPoint.SetStyleType(ST_PenDesc|ST_BrushDesc);
			m_styDataPoint.SetStyleType(ST_PenDesc|ST_BrushDesc);

			stPenDesc.lPenColor = RGB(0,0,0);
			stPenDesc.fPenWidth = 0.2;

			trAnnoDesc.fHeight = 16;
			trAnnoDesc.fWidth  = 16;
			m_styTitle.SetAnnoDesc(trAnnoDesc);
			m_styPanel.SetAnnoDesc(trAnnoDesc);

			trAnnoDesc.fHeight = 8;
			trAnnoDesc.fWidth  = 8;
			m_styAxis.SetAnnoDesc(trAnnoDesc);

			trAnnoDesc.fHeight = 4;
			trAnnoDesc.fWidth  = 4;
			m_styRule.SetAnnoDesc(trAnnoDesc);
			
			m_styAxis.SetPenDesc(stPenDesc);
			m_styChart.SetPenDesc(stPenDesc);
			m_styPanel.SetPenDesc(stPenDesc);
		}
		
		pStyle = pStyleMgr->GetStyle(styleSonfig.szLineStyle);
		if (pStyle)
		{	
			SmtPenDesc        stPenDesc;
			SmtBrushDesc      stBrushDesc;

			memcpy(&m_styGridLine,pStyle,sizeof(SmtStyle));
			memcpy(&m_styDataLine,pStyle,sizeof(SmtStyle));

			m_styGridLine.SetStyleType(ST_PenDesc);
			m_styDataLine.SetStyleType(ST_PenDesc);

			stPenDesc.lPenColor = RGB(255,0,0);
			stPenDesc.fPenWidth = 0.1;
			m_styGridLine.SetPenDesc(stPenDesc);
			m_styRule.SetPenDesc(stPenDesc);

			stPenDesc.lPenColor = RGB(0,0,255);
			stPenDesc.fPenWidth = 0.2;
			m_styDataLine.SetPenDesc(stPenDesc);
		}

		///////////////////////////////////////////////////////////////////////////////////////////////
		fRect lyrRect;
		lyrRect.lb.x = 0;
		lyrRect.lb.y = 0;
		lyrRect.rt.x = 500;
		lyrRect.rt.y = 500;

		SmtVectorLayer* pRegLyr = (SmtVectorLayer*)CreateLayer(c_str_reg_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtSurface);
		SmtVectorLayer* pLineLyr = (SmtVectorLayer*)CreateLayer(c_str_line_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtCurve);
		SmtVectorLayer* pPntLyr = (SmtVectorLayer*)CreateLayer(c_str_pnt_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtDot);
		SmtVectorLayer* pAnnoLyr = (SmtVectorLayer*)CreateLayer(c_str_anno_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtAnno);
		
		if (NULL != pAnnoLyr && NULL != pPntLyr && NULL != pLineLyr &&NULL != pRegLyr)
		{
			pAnnoLyr->Open(c_str_anno_lyr.c_str());
			pPntLyr->Open(c_str_pnt_lyr.c_str());
			pLineLyr->Open(c_str_line_lyr.c_str());
			pRegLyr->Open(c_str_reg_lyr.c_str());

			m_smtMap.SetActiveLayer(c_str_line_lyr.c_str());

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtChart::Create()
	{
		vPoints &vPts =  m_cData.m_vPoints;
		if (vPts.size() < 1)
			return SMT_ERR_FAILURE;

		m_rtData.lb = vPts[0];
		for (int i = 0 ; i < vPts.size(); i++)
		{
			m_rtData.lb.x = min(vPts[i].x,m_rtData.lb.x);
			m_rtData.lb.y = min(vPts[i].y,m_rtData.lb.y);

			m_rtData.rt.x = max(vPts[i].x,m_rtData.rt.x);
			m_rtData.rt.y = max(vPts[i].y,m_rtData.rt.y);
		}

		m_fXOffSet = m_rtData.Width()/8;
		m_fYOffSet = m_rtData.Height()/8;

		m_fDivWidth = m_rtData.Width()/8;
		m_fDivHeight = m_rtData.Height()/8;

		///////////////////////////////////////////////////////////////////
		SmtPenDesc        stPenDesc;
		SmtBrushDesc      stBrushDesc;
		SmtAnnotationDesc trAnnoDesc;
		float			  fWHMin = min(m_rtData.Height(),m_rtData.Width());

		stPenDesc.lPenColor = RGB(0,0,255);
		stPenDesc.fPenWidth = fWHMin/80;
		m_styDataLine.SetPenDesc(stPenDesc);

		trAnnoDesc.fHeight = fWHMin/20;
		trAnnoDesc.fWidth  = fWHMin/20;
		m_styTitle.SetAnnoDesc(trAnnoDesc);
		m_styPanel.SetAnnoDesc(trAnnoDesc);

		trAnnoDesc.fHeight = fWHMin/40;
		trAnnoDesc.fWidth  = fWHMin/40;
		m_styAxis.SetAnnoDesc(trAnnoDesc);

		trAnnoDesc.fHeight = fWHMin/60;
		trAnnoDesc.fWidth  = fWHMin/60;
		m_styRule.SetAnnoDesc(trAnnoDesc);
		///////////////////////////////////////////////////////////////////

		m_cPanel.rtContent  = m_rtData;

		m_cPanel.rtContent.lb.x -= m_fXOffSet;
		m_cPanel.rtContent.lb.y -= m_fYOffSet;

		m_cPanel.rtContent.rt.x += m_fXOffSet;
		m_cPanel.rtContent.rt.y += m_fYOffSet;

		m_rtChart.lb.x = m_cPanel.rtContent.lb.x - m_fDivWidth;
		m_rtChart.rt.x = m_cPanel.rtContent.rt.x + m_fDivWidth;

		m_rtChart.lb.y = m_cPanel.rtContent.lb.y - m_fDivHeight;
		m_rtChart.rt.y = m_cPanel.rtContent.rt.y + m_fDivHeight;

		////////////////////////////////////////////////////////
		if (SMT_ERR_NONE == Clear())
			Draw();

		return SMT_ERR_NONE;
	}

	long SmtChart::Clear()
	{
		SmtVectorLayer* pAnnoLyr = (SmtVectorLayer*)GetLayer(c_str_anno_lyr.c_str());
		SmtVectorLayer* pPntLyr = (SmtVectorLayer*)GetLayer(c_str_pnt_lyr.c_str());
		SmtVectorLayer* pLineLyr = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());
		SmtVectorLayer* pRegLyr = (SmtVectorLayer*)GetLayer(c_str_reg_lyr.c_str());

		if (NULL != pAnnoLyr && NULL != pPntLyr && NULL != pLineLyr &&NULL != pRegLyr)
		{
			pAnnoLyr->DeleteAll();
			pPntLyr->DeleteAll();
			pLineLyr->DeleteAll();
			pRegLyr->DeleteAll();

			return SMT_ERR_NONE;
		}
	
		return SMT_ERR_FAILURE;
	}

	void  SmtChart::Draw()
	{
		//
		DrawChartContent();

		//
		DrawPanel();

		//
		if (m_bShowGridLines)
			DrawGridLines();

		//
		if (m_bShowAxis)
			DrawAixs();

		//
		DrawData();
	}

	void SmtChart::SetPoints(const vPoints &points)
	{
		m_cData.m_vPoints = points;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtChart::DrawChartContent(void)
	{
		SmtVectorLayer *pLineLayer = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());
		SmtLinearRing *pLinearRing = new SmtLinearRing();
		SmtFeature * pSmtFeature = new SmtFeature;	
	
		pLinearRing->AddPoint(m_rtChart.lb.x,m_rtChart.lb.y);
		pLinearRing->AddPoint(m_rtChart.rt.x,m_rtChart.lb.y);
		pLinearRing->AddPoint(m_rtChart.rt.x,m_rtChart.rt.y);
		pLinearRing->AddPoint(m_rtChart.lb.x,m_rtChart.rt.y);
		pLinearRing->CloseRings();

		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styChart);
		pSmtFeature->SetGeometryDirectly(pLinearRing);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pLinearRing)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);

		if (m_bShowTitle)
			DrawTitle();
	}

	void SmtChart::DrawTitle(void)
	{
		fPoint point;
		float fCharWidth = abs(m_styTitle.GetAnnoDesc().fWidth);
		float fCharHeight = abs(m_styTitle.GetAnnoDesc().fHeight);
		float fStrWidth = m_strTitle.length()*fCharWidth;

		point.x = m_rtChart.lb.x+(m_rtChart.Width()-fStrWidth)/2;
		point.y = m_rtChart.rt.y-2*fCharHeight;

		SmtVectorLayer *pAnnoLayer = (SmtVectorLayer*)GetLayer(c_str_anno_lyr.c_str());
		SmtPoint *pSmtPoint = new SmtPoint(point.x,point.y);
		SmtFeature *pSmtFeature = new SmtFeature;
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetStyle(&m_styTitle);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),m_strTitle.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		pAnnoLayer->AppendFeature(pSmtFeature);
	}

	void SmtChart::DrawGridLines(void)
	{
		int nWidth = m_cPanel.rtContent.Width();
		int nHeight = m_cPanel.rtContent.Height();
		float fWHMin = min(m_cPanel.rtContent.Height(),m_cPanel.rtContent.Width());

		float x,y;
	
		SmtVectorLayer *pLineLayer = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());
		SmtMultiLineString *pMLineString = new SmtMultiLineString;
		SmtFeature * pSmtFeature = new SmtFeature;	
		
		x = int(m_cPanel.rtContent.lb.x);
		y = m_cPanel.rtContent.lb.y;

		for (int i = 0 ; i < nWidth;i ++)
		{
			if (IsEqual(int(x/20.)*20,x,dEPSILON))
			{
				SmtLineString *pLineString = new SmtLineString();
				pMLineString->AddGeometryDirectly(pLineString);

				pLineString->AddPoint(x,m_cPanel.rtContent.lb.y);
				pLineString->AddPoint(x,m_cPanel.rtContent.rt.y);
			}

			x += 1.;
		}

		x = m_cPanel.rtContent.lb.x;
		y = int(m_cPanel.rtContent.lb.y);

		for (int i = 0 ; i < nHeight;i ++)
		{
			if (IsEqual(int(y/20.)*20,y,dEPSILON))
			{
				SmtLineString *pLineString = new SmtLineString();
				pMLineString->AddGeometryDirectly(pLineString);

				pLineString->AddPoint(m_cPanel.rtContent.lb.x,y);
				pLineString->AddPoint(m_cPanel.rtContent.rt.x,y);
			}

			y += 1.;
		}

		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styGridLine);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pMLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);
	}

	void SmtChart::DrawAixs(void)
	{
		SmtAnnotationDesc &anno = m_styAxis.GetAnnoDesc();
		fPoint point;
		float fCharWidth = abs(anno.fWidth);
		float fCharHeight = abs(anno.fHeight);
		float fStrWidth = 0;

		SmtVectorLayer *pAnnoLayer = (SmtVectorLayer*)GetLayer(c_str_anno_lyr.c_str());
		SmtVectorLayer *pLineLayer = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());

		SmtPoint	*pSmtPoint = NULL;
		SmtMultiLineString *pMLineString = NULL;
		SmtLineString *pLineString = NULL;
		SmtFeature	*pSmtFeature = NULL;

		//xaxis 
		string strText;

		strText = m_cXAxis.strTitle +"("+m_cXAxis.strUnit+")";
		fStrWidth = strText.length()*fCharWidth;

		point.x = m_cPanel.rtContent.lb.x+(m_cPanel.rtContent.Width()-fStrWidth)/2;
		point.y = m_rtChart.lb.y-fCharHeight*3/4.;

		pSmtPoint = new SmtPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strText.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		pAnnoLayer->AppendFeature(pSmtFeature);

		//yaxis
		strText = m_cYAxis.strTitle +"("+m_cYAxis.strUnit+")";
		fStrWidth = strText.length()*fCharWidth;

		point.x = m_rtChart.lb.x+2*fCharHeight;
		point.y = m_cPanel.rtContent.lb.y+(m_cPanel.rtContent.Height()+fStrWidth)/2;

		pSmtPoint = new SmtPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strText.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),PI/2);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		pAnnoLayer->AppendFeature(pSmtFeature);

		//
		pLineString = new SmtLineString();
		pLineString->AddPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLineString->AddPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.lb.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);

		pLineString = new SmtLineString();
		pLineString->AddPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLineString->AddPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.rt.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);

		//rule
		int nWidth = m_cPanel.rtContent.Width();
		int nHeight = m_cPanel.rtContent.Height();

		//axis 
		float fWHMin = min(m_cPanel.rtContent.Height(),m_cPanel.rtContent.Width());
		float l1 = fWHMin/40,l2 = fWHMin/20;
		float l = 0.;

		//xaxis 
		point.x = int(m_cPanel.rtContent.lb.x);
		point.y = m_cPanel.rtContent.lb.y;
		pMLineString = new SmtMultiLineString;
		for (int i = 0 ; i < nWidth;i++ )
		{
			l = (IsEqual(int(point.x/100.)*100,point.x,dEPSILON))?l2:l1;

			if (IsEqual(int(point.x/50.)*50,point.x,dEPSILON))
			{
				SmtLineString *pLineString = new SmtLineString();
				pMLineString->AddGeometryDirectly(pLineString);

				pLineString->AddPoint(point.x,point.y);
				pLineString->AddPoint(point.x,point.y+l);
			}

			point.x += 1.;
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pMLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);

		//yaxis
		point.x = m_cPanel.rtContent.lb.x;
		point.y = int(m_cPanel.rtContent.lb.y);
		pMLineString = new SmtMultiLineString;
		for (int i = 0 ; i < nHeight;i++ )
		{	
			l = (IsEqual(int(point.y/100.)*100,point.y,dEPSILON))?l2:l1;

			if (IsEqual(int(point.y/50.)*50,point.y,dEPSILON))
			{
				SmtLineString *pLineString = new SmtLineString();
				pMLineString->AddGeometryDirectly(pLineString);

				pLineString->AddPoint(point.x,point.y);
				pLineString->AddPoint(point.x+l,point.y);
			}

			point.y += 1.;
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pMLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);

		char szBuf[TEMP_BUFFER_SIZE];

		fCharWidth = abs(m_styRule.GetAnnoDesc().fWidth);
		fCharHeight = abs(m_styRule.GetAnnoDesc().fHeight);

		//xaxis 
		point.x = int(m_cPanel.rtContent.lb.x);
		point.y = m_cPanel.rtContent.lb.y;

		for (int i = 0 ; i < nWidth;i++ )
		{
			if (IsEqual(int(point.x/50.)*50,point.x,dEPSILON))
			{
				sprintf_s(szBuf,TEMP_BUFFER_SIZE,"%.1f",point.x);
				pSmtPoint = new SmtPoint(point.x+1.5*fCharWidth,point.y);
				pSmtFeature = new SmtFeature;
				pSmtFeature->SetGeometryDirectly(pSmtPoint);
				pSmtFeature->SetFeatureType(SmtFtAnno);
				pSmtFeature->SetStyle(&m_styRule);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),szBuf);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),PI/2.);
				pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
				pAnnoLayer->AppendFeature(pSmtFeature);
			}

			point.x += 1.;
		}

		//yaxis
		point.x = m_cPanel.rtContent.lb.x;
		point.y = int(m_cPanel.rtContent.lb.y);

		for (int i = 0 ; i < nHeight;i++  )
		{	
			if (IsEqual(int(point.y/50.)*50,point.y,dEPSILON))
			{
				sprintf_s(szBuf,TEMP_BUFFER_SIZE,"%.1f",point.y);

				pSmtPoint = new SmtPoint(point.x-fCharWidth*strlen(szBuf),point.y-1.5*fCharHeight);
				pSmtFeature = new SmtFeature;
				pSmtFeature->SetStyle(&m_styRule);
				pSmtFeature->SetFeatureType(SmtFtAnno);
				pSmtFeature->SetGeometryDirectly(pSmtPoint);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),szBuf);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
				pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
				pAnnoLayer->AppendFeature(pSmtFeature);
			}

			point.y += 1.;
		}
	}

	void SmtChart::DrawPanel(void)
	{
		SmtAnnotationDesc &anno = m_styPanel.GetAnnoDesc();
		fPoint point;
		float fCharWidth = abs(anno.fWidth);
		float fCharHeight = abs(anno.fHeight);
		float fStrWidth = 0;

		string strTitle = m_cPanel.strTitle;

		fStrWidth = strTitle.length()*fCharWidth;

		point.x = m_cPanel.rtContent.lb.x+(m_cPanel.rtContent.Width()-fStrWidth)/2;
		point.y = m_cPanel.rtContent.rt.y-fCharHeight;

		SmtVectorLayer *pAnnoLayer = (SmtVectorLayer*)GetLayer(c_str_anno_lyr.c_str());
		SmtVectorLayer *pLineLayer = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());
		SmtFeature * pSmtFeature = NULL;	

		SmtPoint *pSmtPoint = new SmtPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetStyle(&m_styPanel);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strTitle.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		pAnnoLayer->AppendFeature(pSmtFeature);

		SmtLinearRing *pLinearRing = new SmtLinearRing();
		pLinearRing->AddPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLinearRing->AddPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.lb.y);
		pLinearRing->AddPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.rt.y);
		pLinearRing->AddPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.rt.y);
		pLinearRing->CloseRings();
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styPanel);
		pSmtFeature->SetGeometryDirectly(pLinearRing);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pLinearRing)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);
	}

	void SmtChart::DrawData(void)
	{
		vPoints &vPts =  m_cData.m_vPoints;

		SmtVectorLayer *pDotLayer = (SmtVectorLayer*)GetLayer(c_str_pnt_lyr.c_str());
		SmtVectorLayer *pLineLayer = (SmtVectorLayer*)GetLayer(c_str_line_lyr.c_str());
		SmtFeature * pSmtFeature = NULL;	

		SmtMultiPoint *pMPoint = new SmtMultiPoint;
		for (int i = 0 ; i < vPts.size(); i++)
		{
			pMPoint->AddGeometry(new SmtPoint(vPts[i].x,vPts[i].y));
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtDot);
		pSmtFeature->SetStyle(&m_styDataPoint);
		pSmtFeature->SetGeometryDirectly(pMPoint);
		pSmtFeature->SetID(pDotLayer->GetFeatureCount()+1);
		pDotLayer->AppendFeature(pSmtFeature);

		SmtLineString *pLineString = new SmtLineString();
		pLineString->SetNumPoints(vPts.size());
		for (int i = 0; i < vPts.size();i++)
		{
			pLineString->SetPoint(i,vPts[i].x,vPts[i].y);
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styDataLine);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((SmtCurve*)pLineString)->GetLength());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		pLineLayer->AppendFeature(pSmtFeature);
	}
}