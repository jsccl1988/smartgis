#include "stdafx.h"

#include "legacy_ui/chart/chart.h"
#include "base/style/stylemanager.h"
#include "sys/sysmanager.h"
#include "base/core/api.h"
#include "sdb/feature/feature.h"

using namespace sys;

namespace ui
{
	const double				PI						=  3.14159265;   
	const string				c_str_anno_lyr			="CHART_ANNO";
	const string				c_str_pnt_lyr			="CHART_PNT";
	const string				c_str_line_lyr			="CHART_LINE";
	const string				c_str_reg_lyr			="CHART_REG";

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
		SmtSysManager * pSysMgr = SmtSysManager::get_singleton_ptr();
		SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		SmtStyleConfig styleSonfig = pSysMgr->get_sys_style_config();
		SmtStyle *pStyle = NULL;

		pStyle = pStyleMgr->get_style(styleSonfig.szPointStyle);
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
			
			m_styChart.set_style_type(ST_PenDesc);
			m_styTitle.set_style_type(ST_AnnoDesc);
			m_styRule.set_style_type(ST_PenDesc|ST_AnnoDesc);
			m_styAxis.set_style_type(ST_PenDesc|ST_AnnoDesc);
			m_styPanel.set_style_type(ST_PenDesc|ST_AnnoDesc);
			m_styGridPoint.set_style_type(ST_PenDesc|ST_BrushDesc);
			m_styDataPoint.set_style_type(ST_PenDesc|ST_BrushDesc);

			stPenDesc.lPenColor = RGB(0,0,0);
			stPenDesc.fPenWidth = 0.2;

			trAnnoDesc.fHeight = 16;
			trAnnoDesc.fWidth  = 16;
			m_styTitle.set_anno_desc(trAnnoDesc);
			m_styPanel.set_anno_desc(trAnnoDesc);

			trAnnoDesc.fHeight = 8;
			trAnnoDesc.fWidth  = 8;
			m_styAxis.set_anno_desc(trAnnoDesc);

			trAnnoDesc.fHeight = 4;
			trAnnoDesc.fWidth  = 4;
			m_styRule.set_anno_desc(trAnnoDesc);
			
			m_styAxis.set_pen_desc(stPenDesc);
			m_styChart.set_pen_desc(stPenDesc);
			m_styPanel.set_pen_desc(stPenDesc);
		}
		
		pStyle = pStyleMgr->get_style(styleSonfig.szLineStyle);
		if (pStyle)
		{	
			SmtPenDesc        stPenDesc;
			SmtBrushDesc      stBrushDesc;

			memcpy(&m_styGridLine,pStyle,sizeof(SmtStyle));
			memcpy(&m_styDataLine,pStyle,sizeof(SmtStyle));

			m_styGridLine.set_style_type(ST_PenDesc);
			m_styDataLine.set_style_type(ST_PenDesc);

			stPenDesc.lPenColor = RGB(255,0,0);
			stPenDesc.fPenWidth = 0.1;
			m_styGridLine.set_pen_desc(stPenDesc);
			m_styRule.set_pen_desc(stPenDesc);

			stPenDesc.lPenColor = RGB(0,0,255);
			stPenDesc.fPenWidth = 0.2;
			m_styDataLine.set_pen_desc(stPenDesc);
		}

		///////////////////////////////////////////////////////////////////////////////////////////////
		fRect lyrRect;
		lyrRect.lb.x = 0;
		lyrRect.lb.y = 0;
		lyrRect.rt.x = 500;
		lyrRect.rt.y = 500;

		CreateLayer(c_str_reg_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtSurface);
		CreateLayer(c_str_line_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtCurve);
		CreateLayer(c_str_pnt_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtDot);
		CreateLayer(c_str_anno_lyr.c_str(),lyrRect,SmtFeatureType::SmtFtAnno);
		OGRLayer* pRegLyr = m_smtMap.GetOgrLayer(c_str_reg_lyr.c_str());
		OGRLayer* pLineLyr = m_smtMap.GetOgrLayer(c_str_line_lyr.c_str());
		OGRLayer* pPntLyr = m_smtMap.GetOgrLayer(c_str_pnt_lyr.c_str());
		OGRLayer* pAnnoLyr = m_smtMap.GetOgrLayer(c_str_anno_lyr.c_str());
		
		if (NULL != pAnnoLyr && NULL != pPntLyr && NULL != pLineLyr &&NULL != pRegLyr)
		{
			m_smtMap.SetActiveOgrLayer(pLineLyr);

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

		m_fXOffSet = m_rtData.width()/8;
		m_fYOffSet = m_rtData.height()/8;

		m_fDivWidth = m_rtData.width()/8;
		m_fDivHeight = m_rtData.height()/8;

		///////////////////////////////////////////////////////////////////
		SmtPenDesc        stPenDesc;
		SmtBrushDesc      stBrushDesc;
		SmtAnnotationDesc trAnnoDesc;
		float			  fWHMin = min(m_rtData.height(),m_rtData.width());

		stPenDesc.lPenColor = RGB(0,0,255);
		stPenDesc.fPenWidth = fWHMin/80;
		m_styDataLine.set_pen_desc(stPenDesc);

		trAnnoDesc.fHeight = fWHMin/20;
		trAnnoDesc.fWidth  = fWHMin/20;
		m_styTitle.set_anno_desc(trAnnoDesc);
		m_styPanel.set_anno_desc(trAnnoDesc);

		trAnnoDesc.fHeight = fWHMin/40;
		trAnnoDesc.fWidth  = fWHMin/40;
		m_styAxis.set_anno_desc(trAnnoDesc);

		trAnnoDesc.fHeight = fWHMin/60;
		trAnnoDesc.fWidth  = fWHMin/60;
		m_styRule.set_anno_desc(trAnnoDesc);
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
		OGRLayer* pAnnoLyr = GetSmtMap().GetOgrLayer(c_str_anno_lyr.c_str());
		OGRLayer* pPntLyr = GetSmtMap().GetOgrLayer(c_str_pnt_lyr.c_str());
		OGRLayer* pLineLyr = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());
		OGRLayer* pRegLyr = GetSmtMap().GetOgrLayer(c_str_reg_lyr.c_str());

		if (NULL != pAnnoLyr && NULL != pPntLyr && NULL != pLineLyr &&NULL != pRegLyr)
		{
			/* clear deferred */ (void)pAnnoLyr;
			(void)pPntLyr;
			(void)pLineLyr;
			(void)pRegLyr;

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
		OGRLayer* pLineLayer = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());
		OGRLinearRing *pLinearRing = new OGRLinearRing();
		SmtFeature * pSmtFeature = new SmtFeature;	
	
		pLinearRing->addPoint(m_rtChart.lb.x,m_rtChart.lb.y);
		pLinearRing->addPoint(m_rtChart.rt.x,m_rtChart.lb.y);
		pLinearRing->addPoint(m_rtChart.rt.x,m_rtChart.rt.y);
		pLinearRing->addPoint(m_rtChart.lb.x,m_rtChart.rt.y);
		pLinearRing->closeRings();

		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styChart);
		pSmtFeature->SetGeometryDirectly(pLinearRing);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pLinearRing)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);

		if (m_bShowTitle)
			DrawTitle();
	}

	void SmtChart::DrawTitle(void)
	{
		fPoint point;
		float fCharWidth = abs(m_styTitle.get_anno_desc().fWidth);
		float fCharHeight = abs(m_styTitle.get_anno_desc().fHeight);
		float fStrWidth = m_strTitle.length()*fCharWidth;

		point.x = m_rtChart.lb.x+(m_rtChart.width()-fStrWidth)/2;
		point.y = m_rtChart.rt.y-2*fCharHeight;

		OGRLayer* pAnnoLayer = GetSmtMap().GetOgrLayer(c_str_anno_lyr.c_str());
		OGRPoint *pSmtPoint = new OGRPoint(point.x,point.y);
		SmtFeature *pSmtFeature = new SmtFeature;
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetStyle(&m_styTitle);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),m_strTitle.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		leftover_append_feature(pAnnoLayer, pSmtFeature);
	}

	void SmtChart::DrawGridLines(void)
	{
		int nWidth = m_cPanel.rtContent.width();
		int nHeight = m_cPanel.rtContent.height();
		float fWHMin = min(m_cPanel.rtContent.height(),m_cPanel.rtContent.width());

		float x,y;
	
		OGRLayer* pLineLayer = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());
		OGRMultiLineString *pMLineString = new OGRMultiLineString;
		SmtFeature * pSmtFeature = new SmtFeature;	
		
		x = int(m_cPanel.rtContent.lb.x);
		y = m_cPanel.rtContent.lb.y;

		for (int i = 0 ; i < nWidth;i ++)
		{
			if (is_equal(int(x/20.)*20,x,dEPSILON))
			{
				OGRLineString *pLineString = new OGRLineString();
				pMLineString->addGeometryDirectly(pLineString);

				pLineString->addPoint(x,m_cPanel.rtContent.lb.y);
				pLineString->addPoint(x,m_cPanel.rtContent.rt.y);
			}

			x += 1.;
		}

		x = m_cPanel.rtContent.lb.x;
		y = int(m_cPanel.rtContent.lb.y);

		for (int i = 0 ; i < nHeight;i ++)
		{
			if (is_equal(int(y/20.)*20,y,dEPSILON))
			{
				OGRLineString *pLineString = new OGRLineString();
				pMLineString->addGeometryDirectly(pLineString);

				pLineString->addPoint(m_cPanel.rtContent.lb.x,y);
				pLineString->addPoint(m_cPanel.rtContent.rt.x,y);
			}

			y += 1.;
		}

		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styGridLine);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pMLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);
	}

	void SmtChart::DrawAixs(void)
	{
		SmtAnnotationDesc &anno = m_styAxis.get_anno_desc();
		fPoint point;
		float fCharWidth = abs(anno.fWidth);
		float fCharHeight = abs(anno.fHeight);
		float fStrWidth = 0;

		OGRLayer* pAnnoLayer = GetSmtMap().GetOgrLayer(c_str_anno_lyr.c_str());
		OGRLayer* pLineLayer = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());

		OGRPoint	*pSmtPoint = NULL;
		OGRMultiLineString *pMLineString = NULL;
		OGRLineString *pLineString = NULL;
		SmtFeature	*pSmtFeature = NULL;

		//xaxis 
		string strText;

		strText = m_cXAxis.strTitle +"("+m_cXAxis.strUnit+")";
		fStrWidth = strText.length()*fCharWidth;

		point.x = m_cPanel.rtContent.lb.x+(m_cPanel.rtContent.width()-fStrWidth)/2;
		point.y = m_rtChart.lb.y-fCharHeight*3/4.;

		pSmtPoint = new OGRPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strText.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		leftover_append_feature(pAnnoLayer, pSmtFeature);

		//yaxis
		strText = m_cYAxis.strTitle +"("+m_cYAxis.strUnit+")";
		fStrWidth = strText.length()*fCharWidth;

		point.x = m_rtChart.lb.x+2*fCharHeight;
		point.y = m_cPanel.rtContent.lb.y+(m_cPanel.rtContent.height()+fStrWidth)/2;

		pSmtPoint = new OGRPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strText.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),PI/2);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		leftover_append_feature(pAnnoLayer, pSmtFeature);

		//
		pLineString = new OGRLineString();
		pLineString->addPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLineString->addPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.lb.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);

		pLineString = new OGRLineString();
		pLineString->addPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLineString->addPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.rt.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);

		//rule
		int nWidth = m_cPanel.rtContent.width();
		int nHeight = m_cPanel.rtContent.height();

		//axis 
		float fWHMin = min(m_cPanel.rtContent.height(),m_cPanel.rtContent.width());
		float l1 = fWHMin/40,l2 = fWHMin/20;
		float l = 0.;

		//xaxis 
		point.x = int(m_cPanel.rtContent.lb.x);
		point.y = m_cPanel.rtContent.lb.y;
		pMLineString = new OGRMultiLineString;
		for (int i = 0 ; i < nWidth;i++ )
		{
			l = (is_equal(int(point.x/100.)*100,point.x,dEPSILON))?l2:l1;

			if (is_equal(int(point.x/50.)*50,point.x,dEPSILON))
			{
				OGRLineString *pLineString = new OGRLineString();
				pMLineString->addGeometryDirectly(pLineString);

				pLineString->addPoint(point.x,point.y);
				pLineString->addPoint(point.x,point.y+l);
			}

			point.x += 1.;
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pMLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);

		//yaxis
		point.x = m_cPanel.rtContent.lb.x;
		point.y = int(m_cPanel.rtContent.lb.y);
		pMLineString = new OGRMultiLineString;
		for (int i = 0 ; i < nHeight;i++ )
		{	
			l = (is_equal(int(point.y/100.)*100,point.y,dEPSILON))?l2:l1;

			if (is_equal(int(point.y/50.)*50,point.y,dEPSILON))
			{
				OGRLineString *pLineString = new OGRLineString();
				pMLineString->addGeometryDirectly(pLineString);

				pLineString->addPoint(point.x,point.y);
				pLineString->addPoint(point.x+l,point.y);
			}

			point.y += 1.;
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styAxis);
		pSmtFeature->SetGeometryDirectly(pMLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pMLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);

		char szBuf[TEMP_BUFFER_SIZE];

		fCharWidth = abs(m_styRule.get_anno_desc().fWidth);
		fCharHeight = abs(m_styRule.get_anno_desc().fHeight);

		//xaxis 
		point.x = int(m_cPanel.rtContent.lb.x);
		point.y = m_cPanel.rtContent.lb.y;

		for (int i = 0 ; i < nWidth;i++ )
		{
			if (is_equal(int(point.x/50.)*50,point.x,dEPSILON))
			{
				sprintf_s(szBuf,TEMP_BUFFER_SIZE,"%.1f",point.x);
				pSmtPoint = new OGRPoint(point.x+1.5*fCharWidth,point.y);
				pSmtFeature = new SmtFeature;
				pSmtFeature->SetGeometryDirectly(pSmtPoint);
				pSmtFeature->SetFeatureType(SmtFtAnno);
				pSmtFeature->SetStyle(&m_styRule);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),szBuf);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),PI/2.);
				pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
				leftover_append_feature(pAnnoLayer, pSmtFeature);
			}

			point.x += 1.;
		}

		//yaxis
		point.x = m_cPanel.rtContent.lb.x;
		point.y = int(m_cPanel.rtContent.lb.y);

		for (int i = 0 ; i < nHeight;i++  )
		{	
			if (is_equal(int(point.y/50.)*50,point.y,dEPSILON))
			{
				sprintf_s(szBuf,TEMP_BUFFER_SIZE,"%.1f",point.y);

				pSmtPoint = new OGRPoint(point.x-fCharWidth*strlen(szBuf),point.y-1.5*fCharHeight);
				pSmtFeature = new SmtFeature;
				pSmtFeature->SetStyle(&m_styRule);
				pSmtFeature->SetFeatureType(SmtFtAnno);
				pSmtFeature->SetGeometryDirectly(pSmtPoint);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),szBuf);
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
				pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
				pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
				leftover_append_feature(pAnnoLayer, pSmtFeature);
			}

			point.y += 1.;
		}
	}

	void SmtChart::DrawPanel(void)
	{
		SmtAnnotationDesc &anno = m_styPanel.get_anno_desc();
		fPoint point;
		float fCharWidth = abs(anno.fWidth);
		float fCharHeight = abs(anno.fHeight);
		float fStrWidth = 0;

		string strTitle = m_cPanel.strTitle;

		fStrWidth = strTitle.length()*fCharWidth;

		point.x = m_cPanel.rtContent.lb.x+(m_cPanel.rtContent.width()-fStrWidth)/2;
		point.y = m_cPanel.rtContent.rt.y-fCharHeight;

		OGRLayer* pAnnoLayer = GetSmtMap().GetOgrLayer(c_str_anno_lyr.c_str());
		OGRLayer* pLineLayer = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());
		SmtFeature * pSmtFeature = NULL;	

		OGRPoint *pSmtPoint = new OGRPoint(point.x,point.y);
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetGeometryDirectly(pSmtPoint);
		pSmtFeature->SetStyle(&m_styPanel);
		pSmtFeature->SetFeatureType(SmtFtAnno);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("anno"),strTitle.c_str());
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("color"),int(RGB(0,0,0)));
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("angle"),0.);
		pSmtFeature->SetID(pAnnoLayer->GetFeatureCount()+1);
		leftover_append_feature(pAnnoLayer, pSmtFeature);

		OGRLinearRing *pLinearRing = new OGRLinearRing();
		pLinearRing->addPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.lb.y);
		pLinearRing->addPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.lb.y);
		pLinearRing->addPoint(m_cPanel.rtContent.rt.x,m_cPanel.rtContent.rt.y);
		pLinearRing->addPoint(m_cPanel.rtContent.lb.x,m_cPanel.rtContent.rt.y);
		pLinearRing->closeRings();
		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styPanel);
		pSmtFeature->SetGeometryDirectly(pLinearRing);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pLinearRing)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);
	}

	void SmtChart::DrawData(void)
	{
		vPoints &vPts =  m_cData.m_vPoints;

		OGRLayer* pDotLayer = GetSmtMap().GetOgrLayer(c_str_pnt_lyr.c_str());
		OGRLayer* pLineLayer = GetSmtMap().GetOgrLayer(c_str_line_lyr.c_str());
		SmtFeature * pSmtFeature = NULL;	

		OGRMultiPoint *pMPoint = new OGRMultiPoint;
		for (int i = 0 ; i < vPts.size(); i++)
		{
			pMPoint->addGeometry(new OGRPoint(vPts[i].x,vPts[i].y));
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtDot);
		pSmtFeature->SetStyle(&m_styDataPoint);
		pSmtFeature->SetGeometryDirectly(pMPoint);
		pSmtFeature->SetID(pDotLayer->GetFeatureCount()+1);
		leftover_append_feature(pDotLayer, pSmtFeature);

		OGRLineString *pLineString = new OGRLineString();
		pLineString->setNumPoints(vPts.size());
		for (int i = 0; i < vPts.size();i++)
		{
			pLineString->setPoint(i,vPts[i].x,vPts[i].y);
		}

		pSmtFeature = new SmtFeature;
		pSmtFeature->SetFeatureType(SmtFtCurve);
		pSmtFeature->SetStyle(&m_styDataLine);
		pSmtFeature->SetGeometryDirectly(pLineString);
		pSmtFeature->SetFieldValue(pSmtFeature->GetFieldIndexByName("length"),((OGRCurve*)pLineString)->get_Length());
		pSmtFeature->SetID(pLineLayer->GetFeatureCount()+1);
		leftover_append_feature(pLineLayer, pSmtFeature);
	}
}