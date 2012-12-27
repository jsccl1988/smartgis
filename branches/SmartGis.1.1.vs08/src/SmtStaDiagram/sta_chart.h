/*
File:    sta_chart.h 

Desc:    SmtChart,Í¼±í

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_CHART_H
#define _STA_CHART_H

#include "smt_core.h"
#include "bl_style.h"
#include "smt_bas_struct.h"
#include "sta_diagramdata.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_StaDiagram
{
	struct SmtChartAxis 
	{
		string			strTitle;
		string			strUnit;
	};

	struct SmtChartPanel 
	{
		string			strTitle;
		fRect			rtContent;
	};

	typedef vector<fPoint>	vPoints;
	struct SmtChartData
	{
		vPoints			m_vPoints;
	};

	class SMT_EXPORT_CLASS SmtChart:public SmtDiagramData
	{
	public:
		SmtChart();
		virtual ~SmtChart();

	public:
		virtual	long	Init();

		virtual long	Create();

		virtual	long	Clear();

	public:
		inline void		SetShowAxis(bool bShow = true) {m_bShowAxis = bShow;}
		inline void		SetShowTitle(bool bShow = true) {m_bShowTitle = bShow;}
		inline void		SetShowGridLines(bool bShow = true) {m_bShowGridLines = bShow;}

		inline void		SetDivWidth(float fWidth) { m_fDivWidth = fWidth;}
		inline void		SetDivHeight(float fHeight) { m_fDivHeight = fHeight;}

		inline void		SetXOffset(float fXOffset) { m_fXOffSet = fXOffset;}
		inline void		SetYOffset(float fYOffset) { m_fYOffSet = fYOffset;}

		inline bool		GetShowAxis(void) {return m_bShowAxis;}
		inline bool		GetShowTitle(void) {return m_bShowTitle;}
		inline bool		GetShowGridLines(void) {return m_bShowGridLines;}

		inline float	GetDivWidth(void) { return m_fDivWidth ;}
		inline float	GetDivHeight(void) { return m_fDivHeight;}

		inline float	GetXOffset(void) { return m_fXOffSet;}
		inline float	GetYOffset(void) { return m_fYOffSet;}

		inline fRect	GetChartRect(void) { return m_rtChart;}

	public:
		void			SetTitle(const char * szTitle) { m_strTitle = szTitle;}
		void			SetPanelTitle(const char * szTitle) { m_cPanel.strTitle = szTitle;}

		void			SetXAxis(const char * szTitle,const char * szUnit) { m_cXAxis.strTitle = szTitle;m_cXAxis.strUnit = szUnit;}
		void			SetYAxis(const char * szTitle,const char * szUnit) { m_cYAxis.strTitle = szTitle;m_cYAxis.strUnit = szUnit;}
		void			SetPoints(const vPoints &points);

	protected:
		void			Draw(void);
		void			DrawChartContent(void);
		void			DrawTitle(void);
		void			DrawGridLines(void);
		void			DrawAixs(void);
		void			DrawPanel(void);
		void			DrawData(void);

	protected:
		SmtChartPanel	m_cPanel;
		SmtChartAxis	m_cXAxis;
		SmtChartAxis	m_cYAxis;
		SmtChartData	m_cData;
		fRect			m_rtData;
		fRect			m_rtChart;

		SmtStyle		m_styChart;
		SmtStyle		m_styTitle;
		SmtStyle		m_styAxis;
		SmtStyle		m_styRule;
		SmtStyle		m_styPanel;
		SmtStyle		m_styGridPoint;
		SmtStyle		m_styGridLine;
		SmtStyle		m_styDataPoint;
		SmtStyle		m_styDataLine;

		float			m_fDivWidth;
		float			m_fDivHeight;
		float			m_fXOffSet;
		float			m_fYOffSet;

		string			m_strTitle;

	protected:
		bool			m_bShowAxis;
		bool			m_bShowTitle;
		bool			m_bShowGridLines;
	};
}

#if !defined(Export_SmtStaDiagram)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaDiagramD.lib")
#       else
#          pragma comment(lib,"SmtStaDiagram.lib")
#	    endif  
#endif

#endif // _STA_CHART_H