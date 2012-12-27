#include "baog_region.h"
#include "baog_api.h"
#include "geo_api.h"

using namespace Smt_Core;

namespace Smt_BAOrthGrid
{
	SmtBAOGRegion::SmtBAOGRegion(Region_type rType)
	{
		m_selBnd = -1;
		m_modCtrlPtsIndex = -1;
		m_rType = rType;
	}

	SmtBAOGRegion::~SmtBAOGRegion()
	{
		Release();
	}

	void SmtBAOGRegion::Release(void)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			if(pBnd) 
			{
				pBnd->Clear();
				delete pBnd;
			}
		}

		m_vBnds.clear();
		m_selBnd = -1;
		m_modCtrlPtsIndex = -1;
	}

	//设置类型
	void SmtBAOGRegion::SetType(Region_type rType)
	{
		m_rType = rType;
	}

	//添加边界
	void SmtBAOGRegion::AppendBoudary(vdbfPoints &pts,int start,int end,int index,int flag,bool slided,bool sampled)
	{
		SmtBAOGBoudary *pBnd = new SmtBAOGBoudary;
		pBnd->flag    = flag;
		pBnd->start   = start;
		pBnd->end     = end;
		pBnd->index   = index;
		pBnd->can_slide = slided;
		pBnd->can_sample = sampled;
		pBnd->ctrlPts.resize(pts.size());
		SetCtrlPoints(pBnd->ctrlPts,pts);
		m_vBnds.push_back(pBnd);
	}

	//设置是否固定边界点
	void SmtBAOGRegion::SetSlided(int SelBnd,bool slided)
	{
		if(SelBnd > -1 && SelBnd < m_vBnds.size())
			m_vBnds[SelBnd]->can_slide = slided;

	}

	void SmtBAOGRegion::SetSlided(bool slided)
	{
		for (int i = 0; i < m_vBnds.size();i ++ ) 
			m_vBnds[i]->can_slide = slided;

	}

	//重置网格大小
	void SmtBAOGRegion::ReSizeBoudary(int ib,int start,int end,int index,int flag)
	{
		SmtBAOGBoudary *pBnd = m_vBnds[ib];

		pBnd->flag    = flag;
		pBnd->start   = start;
		pBnd->end     = end;
		pBnd->index   = index;
	}

	//
	bool SmtBAOGRegion::HitTestOn(int ii,int jj)
	{
		bool flag = false;
		int i = 0;
		//判断是不是在边上
		while (!flag && i < m_vBnds.size())
		{
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);

			flag = HitTestBoudaryOn(pBnd,ii,jj);
			i++;
		}

		return flag;
	}

	bool SmtBAOGRegion::HitTestOnCorner(int ii,int jj)
	{
		bool flag = false;
		int i = 0;
		//判断是不是在角点上
		while (!flag && i < m_vBnds.size())
		{
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			flag = HitTestBoudaryOnCorner(pBnd,ii,jj);
			i++;
		}

		return flag;
	}

	bool SmtBAOGRegion::HitTestIn(int ii,int jj)
	{
		bool flag = true;
		int i = 0;
		//判断是不是在内部
		while (flag && i < m_vBnds.size() )
		{
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			flag = HitTestBoudaryRightHand(pBnd,ii,jj);
			i++;
		}
		return flag;
	}

	bool SmtBAOGRegion::HitTestBoudaryOn(SmtBAOGBoudary * pBnd,int i,int j)
	{
		bool flag = false;
		if (pBnd->flag == 0 || pBnd->flag == 2)
		{
			if(j == pBnd->index && 
			   i <= max(pBnd->start,pBnd->end) && 
			   i >= min(pBnd->start,pBnd->end)) 
				flag = true;
		}
		else if (pBnd->flag == 1 || pBnd->flag == 3)
		{
			if(i == pBnd->index && 
			   j <= max(pBnd->start,pBnd->end) && 
			   j >= min(pBnd->start,pBnd->end)) 
			   flag = true;
		}

		return flag;
	}

	bool SmtBAOGRegion::HitTestBoudaryOnCorner(SmtBAOGBoudary * pBnd,int i,int j)
	{
		bool flag = false;
		if (pBnd->flag == 0 || pBnd->flag == 2)
		{
			if(j == pBnd->index && 
			   (i == pBnd->start || i == pBnd->end) ) 
			   flag = true;
		}
		else if (pBnd->flag == 1 || pBnd->flag == 3)
		{
			if(i == pBnd->index && 
			   (j == pBnd->start || 
			    j == pBnd->end) ) 
				flag = true;
		}

		return flag;
	}

	bool SmtBAOGRegion::HitTestBoudaryRightHand(SmtBAOGBoudary * pBnd,int i,int j)
	{
		bool flag = false;
		if (pBnd->flag == 0)
		{
			if(j < pBnd->index) 
				flag = true;
		}
		else if(pBnd->flag == 1)
		{
			if(i > pBnd->index) 
				flag = true;
		}
		else if(pBnd->flag == 2)
		{
			if(j > pBnd->index) 
				flag = true;
		}
		else if(pBnd->flag == 3)
		{
			if(i < pBnd->index) 
				flag = true;
		}
		return flag;
	}

	//////////////////////////////////////////////////////////////////////////
	//光滑边界
	void  SmtBAOGRegion::SmoothBoundary(int N)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			pBnd->curvePts.clear();
			if(pBnd->can_sample)
			{
				pBnd->curvePts.resize(N);
				Spline3(pBnd->ctrlPts,pBnd->curvePts,N);
			}
			else 
			{
				pBnd->curvePts.resize(pBnd->ctrlPts.size());
				SetCurvePoints(pBnd->ctrlPts,pBnd->curvePts);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//添加边界控制点
	bool SmtBAOGRegion::AddCtrlPt(dbfPoint pt,int a)
	{
		if(SearchCtrlPt(pt,a)) 
			return true;

		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			if (AddCtrlPt(pBnd,pt,a))
			{
				m_selBnd = i;
				return true;
			}
		}
		return false;
	}

	//添加边界控制点
	bool SmtBAOGRegion::AddCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a)
	{
		int prei = 0,nexti = 1;
		
		double d = SmtDistance(pt,pBnd->curvePts,prei,nexti);
		if(d < a) 
		{
			dbfPoint A,B,H;
			CtrlPoint P;
			int flag;
			A = pBnd->curvePts[prei];	
			B = pBnd->curvePts[nexti];

			//过点P1作垂直于找到的边界线段的垂线，垂足为H，flag标记H是否位于线段上
			flag = SmtGetH(A,B,pt,H);
			if( flag == -1)
			{
				H = A;
			}
			else if (flag == 1)
			{
				H = B;
			}

			P.P = H;
			P.PreIndex = prei;
			P.NexIndex = nexti;

			InsertCtrlPt(pBnd->ctrlPts,P);

			return true;
		}

		return false;
	}
	//////////////////////////////////////////////////////////////////////////
	//遍历控制点，比较点pt是否选中控制点
	bool SmtBAOGRegion::SearchCtrlPt(dbfPoint pt,int a)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	 
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			if (SearchCtrlPt(pBnd,pt,a))
			{
				m_selBnd = i;
				return true;
			}
		}
		return false;		
	}

	bool SmtBAOGRegion::SearchCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a)
	{
		int N = pBnd->ctrlPts.size(); 
		for (int i = 0; i < N ; i ++)
		{
			dbfPoint A = pBnd->ctrlPts[i].P;
			double d = SmtDistance(A.x,A.y,pt.x,pt.y);
			if(d < a)
			{
				m_modCtrlPtsIndex = i;
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	//修改边界控制点
	void SmtBAOGRegion::ModCtrlPt(dbfPoint pt)
	{
		if(m_selBnd < 0 || m_selBnd > m_vBnds.size() - 1 || m_modCtrlPtsIndex == -1) 
			return;

		SmtBAOGBoudary * pBnd = m_vBnds.at(m_selBnd);
		pBnd->ctrlPts[m_modCtrlPtsIndex].P = pt;

		if (m_rType == typeMain)
		{
			if (m_modCtrlPtsIndex == pBnd->ctrlPts.size()-1)
			{
				SmtBAOGBoudary * p = m_vBnds.at(m_selBnd+1);
				p->ctrlPts[0].P = pt;
			}

			if (m_modCtrlPtsIndex == 0 && m_selBnd == 0)
			{
				SmtBAOGBoudary * p = m_vBnds.at(m_vBnds.size()-1);
				p->ctrlPts[p->ctrlPts.size()-1].P = pt;
			}
		}
		else if(m_rType == typeDigged)
		{
			if (m_modCtrlPtsIndex == 0)
			{
				SmtBAOGBoudary * p = m_vBnds.at(m_selBnd+1);
				p->ctrlPts[p->ctrlPts.size()-1].P = pt;
			}

			if (m_modCtrlPtsIndex == pBnd->ctrlPts.size()-1 && m_selBnd == 0)
			{
				SmtBAOGBoudary * p = m_vBnds.at(m_vBnds.size()-1);
				p->ctrlPts[0].P = pt;
			}
		}

		m_modCtrlPtsIndex = -1;
	}

	//在边界插入控制点
	void SmtBAOGRegion::InsertCtrlPt(vCtrlPoints & vbnd,CtrlPoint pt)
	{
		int i = 0;
		bool flag = false;
		vCtrlPoints::iterator iter = vbnd.begin();
		while (i < vbnd.size())
		{	
			if ((*iter).PreIndex >= pt.NexIndex) 
			{
				m_modCtrlPtsIndex = i;
				flag = true;
				break;
			}
			i ++;
			iter ++;
		}	

		if(flag)
			vbnd.insert(iter,1,pt);
	}

	//////////////////////////////////////////////////////////////////////////
	//删除边界控制点
	bool SmtBAOGRegion::DelCtrlPt(dbfPoint pt,int a)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGBoudary * pBnd = m_vBnds.at(i);
			if(DelCtrlPt(pBnd,pt,a)) 
				return true;
		}

		return false;
	}

	//删除边界控制点
	bool SmtBAOGRegion::DelCtrlPt(SmtBAOGBoudary * pBnd,dbfPoint pt,int a)
	{
		bool flag = false;
		vCtrlPoints::iterator iterb = pBnd->ctrlPts.begin(),itere = pBnd->ctrlPts.end();
		vCtrlPoints::iterator iter = iterb;
		itere --;
		
		while (iter != pBnd->ctrlPts.end())
		{
			dbfPoint A = (*iter).P;
			double d = SmtDistance(A.x,A.y,pt.x,pt.y);
			if(d < a && iter != iterb  && iter != itere )
			{
				pBnd->ctrlPts.erase(iter);
				flag =  true;
				break;
			}
			iter++;
		}
		return flag;
	}
}