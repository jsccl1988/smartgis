#ifndef _BAOG_REGION_H
#define _BAOG_REGION_H

#include "baog_bas_struct.h"

/*
˵�����������ĸ���
1.�����еĿ��������ṩ���ı߽����ݹ���
2.�޸���������߽磺ɾ���߽���Ƶ�(ÿ���ߵ�ʼĩ���㲻��ɾ��)���϶��߽�����
*/

namespace Smt_BAOrthGrid
{
	enum Region_type
	{
		typeMain,      //����  ��ʱ��洢���߽�����
		typeDigged,    //��ȥ  ˳ʱ��洢���߽�����
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
		//�⻬�߽�����
		void			SmoothBoundary(int N = 30);

	public:
		/************************************************************************/
		/*�޸����߽�                                                            */
		/************************************************************************/
		//��ӱ߽���Ƶ�
		bool			AddCtrlPt(dbfPoint pt,int a = 10); 

		//ɾ���߽���Ƶ�
		bool			DelCtrlPt(dbfPoint pt,int a = 10);

		//�޸ı߽���Ƶ�
		void			ModCtrlPt(dbfPoint pt);                          

	private:
		/************************************************************************/
		/*�޸����߽�                                                            */
		/************************************************************************/
		//��ӱ߽���Ƶ�
		bool			AddCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a = 10);

		//ɾ���߽���Ƶ�
		bool			DelCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a);

		//�������Ƶ㣬�Ƚϵ�pt�Ƿ�ѡ�п��Ƶ�
		bool			SearchCtrlPt(dbfPoint pt,int a = 10);
		bool			SearchCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a = 10);

		//�ڱ߽������Ƶ�    
		void			InsertCtrlPt(vCtrlPoints & vbnd,CtrlPoint pt);   

	private:
		Region_type		m_rType;                     //1 ���� ��2��ȥ�� ��3������
		vBoudaryPtrs	m_vBnds;                     //�߽�

		int				m_selBnd;                    //-1��Ч
		int				m_modCtrlPtsIndex;           //�޸Ŀ��Ƶ����
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