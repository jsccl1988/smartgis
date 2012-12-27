#ifndef _BAOG_REGION_H
#define _BAOG_REGION_H

#include "baog_bas_struct.h"

/*
说明：定义区的概念
1.网格中的控制区，提供区的边界数据管理。
2.修改区的物理边界：删除边界控制点(每条边的始末两点不能删除)，拖动边界曲线
*/

namespace Smt_BAOrthGrid
{
	enum Region_type
	{
		typeMain,      //主区  逆时针存储各边界坐标
		typeDigged,    //挖去  顺时针存储各边界坐标
	};

	enum ShowType
	{
		typeSel,
		typeGeneral
	};

	class SmtBAOrthGrid;
	class SMT_EXPORT_CLASS SmtBAOGRegion
	{
	public:
		friend class SmtBAOrthGrid;

		SmtBAOGRegion(Region_type rType);
		~SmtBAOGRegion(void);
		void			Release(void);

	public:
		void			SetType(Region_type rType);
		void			SetSlided(int SelBnd,bool slided = true);
		void			SetSlided(bool slided = true);
		void			AppendBoudary(vdbfPoints &pts,int start,int end,int index,int flag,bool slided = true,bool sampled = true);
		void			ReSizeBoudary(int ib,int start,int end,int index,int flag);

		bool			HitTestOn(int i,int j);
		bool			HitTestOnCorner(int i,int j);
		bool			HitTestIn(int i,int j);
		bool			HitTestBoudaryOn(SmtBAOGBoudary * pBnd,int i,int j);
		bool			HitTestBoudaryOnCorner(SmtBAOGBoudary * pBnd,int i,int j);
		bool			HitTestBoudaryRightHand(SmtBAOGBoudary * pBnd,int i,int j);

	public:
		//光滑边界曲线
		void			SmoothBoundary(int N = 30);

	public:
		/************************************************************************/
		/*修改区边界                                                            */
		/************************************************************************/
		//添加边界控制点
		bool			AddCtrlPt(dbfPoint pt,int a = 10); 

		//删除边界控制点
		bool			DelCtrlPt(dbfPoint pt,int a = 10);

		//修改边界控制点
		void			ModCtrlPt(dbfPoint pt);                          

	private:
		/************************************************************************/
		/*修改区边界                                                            */
		/************************************************************************/
		//添加边界控制点
		bool			AddCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a = 10);

		//删除边界控制点
		bool			DelCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a);

		//遍历控制点，比较点pt是否选中控制点
		bool			SearchCtrlPt(dbfPoint pt,int a = 10);
		bool			SearchCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a = 10);

		//在边界插入控制点    
		void			InsertCtrlPt(vCtrlPoints & vbnd,CtrlPoint pt);   

	private:
		Region_type		m_rType;                     //1 主区 ，2挖去区 ，3附加区
		vBoudaryPtrs	m_vBnds;                     //边界

		int				m_selBnd;                    //-1无效
		int				m_modCtrlPtsIndex;           //修改控制点序号
	};

	typedef std::vector<SmtBAOGRegion*> vRegionPtrs;
}

#if !defined(Export_SmtBAOrthGrid)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtBAOrthGridD.lib")
#       else
#          pragma comment(lib,"SmtBAOrthGrid.lib")
#	    endif  
#endif

#endif// _BAOG_REGION_H