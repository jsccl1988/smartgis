#include "baog_baorthgrid.h"
#include "baog_api.h"

namespace Smt_BAOrthGrid
{
	//////////////////////////////////////////////////////////////////////////
	//添加边界控制点
	bool SmtBAOrthGrid::AddCtrlPt(dbfPoint pt,int a)
	{
		bool flag = m_rMainRegion.AddCtrlPt(pt,a);
		int i = 0;
		while(!flag && i < m_rRegions.size())
		{
			flag = m_rRegions.at(i)->AddCtrlPt(pt,a);
			i++;
		}
		return flag;
	}

	//修改边界控制点
	void SmtBAOrthGrid::ModCtrlPt(dbfPoint pt)
	{
		m_rMainRegion.ModCtrlPt(pt);
		m_rMainRegion.SmoothBoundary();
		for (int i = 0; i < m_rRegions.size(); i ++)
		{
			m_rRegions.at(i)->ModCtrlPt(pt);
			m_rRegions.at(i)->SmoothBoundary();
		}
		CreateOrthGrid();
	}

	//////////////////////////////////////////////////////////////////////////
	//删除边界控制点
	bool SmtBAOrthGrid::DelCtrlPt(dbfPoint pt,int a)
	{
		bool flag = m_rMainRegion.DelCtrlPt(pt,a);	
		if (flag)
		{
			m_rMainRegion.SmoothBoundary();
		}

		int i = 0;
		while(!flag && i < m_rRegions.size())
		{
			flag = m_rRegions.at(i)->DelCtrlPt(pt,a);	
			if(flag)
				m_rRegions.at(i)->SmoothBoundary();
			i++;
		}

		if (flag)
		{
			CreateOrthGrid();
			return true;
		}

		return flag;
	}

	//////////////////////////////////////////////////////////////////////////
	//挖去区
	bool SmtBAOrthGrid::DigRectRegion(int iCellStart,int jCellStart,int iCellEnd ,int jCellEnd)
	{
		if (iCellStart == iCellEnd || jCellStart == jCellEnd) 
			return false;

		int iStart,jStart,iEnd,jEnd;

		iStart = iCellStart;
		jStart = jCellStart;

		iEnd = iCellEnd;
		jEnd = jCellEnd;

		if (iStart < iEnd && jStart > jEnd)
		{
			jStart++;
			iEnd++;
		}
		else if (iStart > iEnd && jStart < jEnd)
		{
			iStart++;
			jEnd++;
		}
		else if (iStart < iEnd && jStart < jEnd)
		{
			iEnd++;
			jEnd++;
		}
		else if (iStart > iEnd && jStart > jEnd)
		{
			iStart++;
			jStart++;
		}

		vdbfPoints bnd0,bnd1,bnd2,bnd3;
		GetRegionBoudaryX(bnd0,iStart,iEnd,jStart);        
		GetRegionBoudaryY(bnd1,jEnd,jStart,iStart);         
		GetRegionBoudaryX(bnd2,iEnd,iStart,jEnd);           
		GetRegionBoudaryY(bnd3,jStart,jEnd,iEnd);         

		SmtBAOGRegion *pRegion = new SmtBAOGRegion(typeDigged);
		//添加挖去区边界
		pRegion->AppendBoudary(bnd0,iStart,iEnd,jStart,0);        //添加下边
		pRegion->AppendBoudary(bnd1,jEnd,jStart,iStart,1);        //添加右边
		pRegion->AppendBoudary(bnd2,iEnd,iStart,jEnd,2);          //添加上边
		pRegion->AppendBoudary(bnd3,jStart,jEnd,iEnd,3);          //添加左边

		//光滑边界曲线
		pRegion->SmoothBoundary(15);
		m_rRegions.push_back(pRegion);

		SetDiggedRegionInvalid();

		return true;
	}

	//获取挖去X边界坐标
	void SmtBAOrthGrid::GetRegionBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J)
	{
		int i;
		dbfPoint P;
		int nStep = abs(iStart - iEnd)/4;
		if(nStep < 1)nStep = 1;
		if (iEnd > iStart)
		{
			for (i = iStart; i <= iEnd;i += nStep)
			{
				P = m_pNodes->GetElement(J,i);
				bnd.push_back(P);
			}
		}
		else
		{
			for (i = iStart; i >= iEnd;i -=nStep)
			{
				P = m_pNodes->GetElement(J,i);
				bnd.push_back(P);
			}
		}
		bnd[bnd.size()-1] = m_pNodes->GetElement(J,iEnd);

	}

	//获取挖去Y边界坐标
	void SmtBAOrthGrid::GetRegionBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I)
	{
		dbfPoint P;
		int j;
		int nStep = abs(jStart - jEnd)/3;
		if(nStep < 1)nStep = 1;
		if (jEnd > jStart)
		{
			for (j = jStart; j <= jEnd;j += nStep)
			{ 
				P = m_pNodes->GetElement(j,I);
				bnd.push_back(P);
			}
		}
		else
		{
			for (j = jStart; j >= jEnd;j -= nStep)
			{
				P = m_pNodes->GetElement(j,I);
				bnd.push_back(P);
			}
		}
		bnd[bnd.size()-1] = m_pNodes->GetElement(jEnd,I);
	}


	//////////////////////////////////////////////////////////////////////////
	//悬挂区
	void SmtBAOrthGrid::GetBoudaryNode(int start,int end, int index, int flag,vdbfPoints &nodes)
	{
		switch(flag)
		{
		case 0:
			GetBoudaryNodeX(start,end,index,nodes);
			break;
		case 1:
			GetBoudaryNodeY(start,end,index,nodes);
			break;
		default:
			break;
		}
	}

	void SmtBAOrthGrid::GetBoudaryNodeX(int start,int end, int index,vdbfPoints &nodes)
	{
		int i;
		dbfPoint P;
		if (end > start)
		{
			for (i = start; i <= end;i ++)
			{
				P = m_pNodes->GetElement(index,i);
				nodes.push_back(P);
			}
		}
		else
		{
			for (i = start; i >= end;i --)
			{
				P = m_pNodes->GetElement(index,i);
				nodes.push_back(P);
			}
		}
	}

	void SmtBAOrthGrid::GetBoudaryNodeY(int start,int end, int index,vdbfPoints &nodes)
	{
		dbfPoint P;
		int j;
		if (end > start)
		{
			for (j = start; j <= end;j ++)
			{ 
				P = m_pNodes->GetElement(j,index);
				nodes.push_back(P);
			}

		}
		else
		{
			for (j = start; j >= end;j --)
			{
				P = m_pNodes->GetElement(j,index);
				nodes.push_back(P);
			}
		}
	}
}