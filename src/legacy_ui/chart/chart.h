/*
File:    sta_chart.h 

Desc:    SmtChart,图锟斤拷

Version: Version 1.0

Writter:  锟铰达拷锟斤拷

Date:    2012.8.15

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_CHART_H
#define _STA_CHART_H
#if defined(STAT_CHART_EXPORTS)
#define STAT_CHART_EXPORT __declspec(dllexport)
#else
#define STAT_CHART_EXPORT __declspec(dllimport)
#endif


#include "base/core/core.h"
#include "base/style/style.h"
#include "base/core/bas_struct.h"
#include "legacy_ui/chart/diagramdata.h"

using namespace base;
using namespace base;

namespace ui
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

	class STAT_CHART_EXPORT SmtChart:public SmtDiagramData
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

#if !defined(STAT_CHART_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif  
#endif

#endif // _STA_CHART_H