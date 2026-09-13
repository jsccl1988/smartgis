#include "plugin/orthogrid/region.h"
#include "plugin/orthogrid/curve.h"
#include "plugin/orthogrid/detail/polyline.h"

#include <cmath>

using namespace base;

namespace orthogrid
{
	Region::Region(Region_type rType)
	{
		m_selBnd = -1;
		m_modCtrlPtsIndex = -1;
		m_rType = rType;
	}

	Region::~Region()
	{
		Release();
	}

	void Region::Release(void)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			Boundary * pBnd = m_vBnds.at(i);
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

	//��������
	void Region::SetType(Region_type rType)
	{
		m_rType = rType;
	}

	//���ӱ߽�
	void Region::AppendBoudary(vdbfPoints &pts,int start,int end,int index,int flag,bool slided,bool sampled)
	{
		Boundary *pBnd = new Boundary;
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

	//�����Ƿ�̶��߽��
	void Region::SetSlided(int SelBnd,bool slided)
	{
		if(SelBnd > -1 && SelBnd < m_vBnds.size())
			m_vBnds[SelBnd]->can_slide = slided;

	}

	void Region::SetSlided(bool slided)
	{
		for (int i = 0; i < m_vBnds.size();i ++ ) 
			m_vBnds[i]->can_slide = slided;

	}

	//���������С
	void Region::ReSizeBoudary(int ib,int start,int end,int index,int flag)
	{
		Boundary *pBnd = m_vBnds[ib];

		pBnd->flag    = flag;
		pBnd->start   = start;
		pBnd->end     = end;
		pBnd->index   = index;
	}

	//
	bool Region::HitTestOn(int ii,int jj)
	{
		bool flag = false;
		int i = 0;
		//�ж��ǲ����ڱ���
		while (!flag && i < m_vBnds.size())
		{
			Boundary * pBnd = m_vBnds.at(i);

			flag = HitTestBoudaryOn(pBnd,ii,jj);
			i++;
		}

		return flag;
	}

	bool Region::HitTestOnCorner(int ii,int jj)
	{
		bool flag = false;
		int i = 0;
		//�ж��ǲ����ڽǵ���
		while (!flag && i < m_vBnds.size())
		{
			Boundary * pBnd = m_vBnds.at(i);
			flag = HitTestBoudaryOnCorner(pBnd,ii,jj);
			i++;
		}

		return flag;
	}

	bool Region::HitTestIn(int ii,int jj)
	{
		bool flag = true;
		int i = 0;
		//�ж��ǲ������ڲ�
		while (flag && i < m_vBnds.size() )
		{
			Boundary * pBnd = m_vBnds.at(i);
			flag = HitTestBoudaryRightHand(pBnd,ii,jj);
			i++;
		}
		return flag;
	}

	bool Region::HitTestBoudaryOn(Boundary * pBnd,int i,int j)
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

	bool Region::HitTestBoudaryOnCorner(Boundary * pBnd,int i,int j)
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

	bool Region::HitTestBoudaryRightHand(Boundary * pBnd,int i,int j)
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
	//�⻬�߽�
	void  Region::SmoothBoundary(int N)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			Boundary * pBnd = m_vBnds.at(i);
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
	//���ӱ߽���Ƶ�
	bool Region::AddCtrlPt(dbfPoint pt,int a)
	{
		if(SearchCtrlPt(pt,a)) 
			return true;

		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			Boundary * pBnd = m_vBnds.at(i);
			if (AddCtrlPt(pBnd,pt,a))
			{
				m_selBnd = i;
				return true;
			}
		}
		return false;
	}

	//���ӱ߽���Ƶ�
	bool Region::AddCtrlPt(Boundary * pBnd,dbfPoint pt,int a)
	{
		int prei = 0,nexti = 1;
		
		double d = orthogrid::detail::distance_to_polyline(pt,pBnd->curvePts,prei,nexti);
		if(d < a) 
		{
			dbfPoint A,B,H;
			CtrlPoint P;
			int flag;
			A = pBnd->curvePts[prei];	
			B = pBnd->curvePts[nexti];

			//����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
			flag = orthogrid::detail::foot_on_segment(A,B,pt,H);
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
	//�������Ƶ㣬�Ƚϵ�pt�Ƿ�ѡ�п��Ƶ�
	bool Region::SearchCtrlPt(dbfPoint pt,int a)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	 
			Boundary * pBnd = m_vBnds.at(i);
			if (SearchCtrlPt(pBnd,pt,a))
			{
				m_selBnd = i;
				return true;
			}
		}
		return false;		
	}

	bool Region::SearchCtrlPt(Boundary * pBnd,dbfPoint pt,int a)
	{
		int N = pBnd->ctrlPts.size(); 
		for (int i = 0; i < N ; i ++)
		{
			dbfPoint A = pBnd->ctrlPts[i].P;
			double d = std::hypot(pt.x - A.x, pt.y - A.y);
			if(d < a)
			{
				m_modCtrlPtsIndex = i;
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	//�޸ı߽���Ƶ�
	void Region::ModCtrlPt(dbfPoint pt)
	{
		if(m_selBnd < 0 || m_selBnd > m_vBnds.size() - 1 || m_modCtrlPtsIndex == -1) 
			return;

		Boundary * pBnd = m_vBnds.at(m_selBnd);
		pBnd->ctrlPts[m_modCtrlPtsIndex].P = pt;

		if (m_rType == typeMain)
		{
			if (m_modCtrlPtsIndex == pBnd->ctrlPts.size()-1)
			{
				Boundary * p = m_vBnds.at(m_selBnd+1);
				p->ctrlPts[0].P = pt;
			}

			if (m_modCtrlPtsIndex == 0 && m_selBnd == 0)
			{
				Boundary * p = m_vBnds.at(m_vBnds.size()-1);
				p->ctrlPts[p->ctrlPts.size()-1].P = pt;
			}
		}
		else if(m_rType == typeDigged)
		{
			if (m_modCtrlPtsIndex == 0)
			{
				Boundary * p = m_vBnds.at(m_selBnd+1);
				p->ctrlPts[p->ctrlPts.size()-1].P = pt;
			}

			if (m_modCtrlPtsIndex == pBnd->ctrlPts.size()-1 && m_selBnd == 0)
			{
				Boundary * p = m_vBnds.at(m_vBnds.size()-1);
				p->ctrlPts[0].P = pt;
			}
		}

		m_modCtrlPtsIndex = -1;
	}

	//�ڱ߽������Ƶ�
	void Region::InsertCtrlPt(vCtrlPoints & vbnd,CtrlPoint pt)
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
	//ɾ���߽���Ƶ�
	bool Region::DelCtrlPt(dbfPoint pt,int a)
	{
		int nNum = m_vBnds.size();
		for(int i=0;i<nNum;i++)
		{	
			Boundary * pBnd = m_vBnds.at(i);
			if(DelCtrlPt(pBnd,pt,a)) 
				return true;
		}

		return false;
	}

	//ɾ���߽���Ƶ�
	bool Region::DelCtrlPt(Boundary * pBnd,dbfPoint pt,int a)
	{
		bool flag = false;
		vCtrlPoints::iterator iterb = pBnd->ctrlPts.begin(),itere = pBnd->ctrlPts.end();
		vCtrlPoints::iterator iter = iterb;
		itere --;
		
		while (iter != pBnd->ctrlPts.end())
		{
			dbfPoint A = (*iter).P;
			double d = std::hypot(pt.x - A.x, pt.y - A.y);
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