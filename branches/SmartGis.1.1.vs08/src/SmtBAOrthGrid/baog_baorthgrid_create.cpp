#include "baog_baorthgrid.h"
#include "baog_api.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include "geo_api.h"

const double				PI						=  3.14159265;       

using namespace Smt_Core;

namespace Smt_BAOrthGrid
{
	//////////////////////////////////////////////////////////////////////////
	//创建网格
	long SmtBAOrthGrid::CreateOrthGrid()
	{
		if (m_nX == 0 || m_nY == 0)
			return SMT_ERR_INVALID_PARAM;
		 
		InitialGrid();                   //初始化网格
		Orhogonal_SOR();                 //正交化	
		SetDiggedRegionInvalid();        //设置挖去区内节点坐标为无效值
		CalGridOrthogonality();			 //计算网格正交化程度
		SetGridCell();                   //设置网格单元

		return SMT_ERR_NONE;
	}

	long SmtBAOrthGrid::CvtToGrid(SmtGrid &oSmtGrid)
	{
		oSmtGrid.SetSize(m_nY,m_nX);

		dbfPoint *pDataBuf = NULL;
		int		 nSize = 0;
		Matrix2D<RawPoint>  *pNodes = oSmtGrid.GetGridNodeBuf();

		m_pNodes->GetElementBuf(pDataBuf,nSize);
		pNodes->SetElements(pDataBuf,nSize);
		
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	//初始化网格
	void SmtBAOrthGrid::InitialGrid(void)   
	{
		float orth = 0.;
		for (int i = 0; i < m_nX; i ++)
			for (int j = 0; j < m_nY ; j++)
				m_pOrthogonality->SetElement(orth,j,i);

		//初始化边界点
		InitialBoudary();
		//初始化内部网格节点
		InitialInternal();
		//初始化挖掘区边界坐标
		InitialRegionBoudary();
	}     

	//初始化边界
	void SmtBAOrthGrid::InitialBoudary()
	{
		int nBnds = m_rMainRegion.m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = m_rMainRegion.m_vBnds.at(i);
			SampleBoudary(pBnd);                              //等间距采样边界点
		} 
	}

	//初始化区边界
	void SmtBAOrthGrid::InitialRegionBoudary()
	{
		for (int i = 0; i < m_rRegions.size(); i ++)
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			InitialRegionBoudary(pRegion);
		}
	}

	//初始化区边界
	void SmtBAOrthGrid::InitialRegionBoudary(SmtBAOGRegion * pRegion)
	{
		int nBnds = pRegion->m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(i);
			SampleBoudary(pBnd);                              //等间距采样边界点
		} 
	}

	//等间距采样边界坐标
	void SmtBAOrthGrid::SampleBoudary(SmtBAOGBoudary * pBnd)
	{
		int is = pBnd->start,ie = pBnd->end,ii = pBnd->index,flag = pBnd->flag;
		if (pBnd->can_sample)
		{
			switch(flag)
			{
			case 0:
			case 2:
				SampleBoudaryX(pBnd->curvePts,is,ie,ii);
				break;
			case 1:
			case 3:
				SampleBoudaryY(pBnd->curvePts,is,ie,ii);
				break;
			default:
				break;
			}
		}
		else 
		{
			switch(flag)
			{
			case 0:
			case 2:
				SetBoudaryX(pBnd->curvePts,is,ie,ii);
				break;
			case 1:
			case 3:
				SetBoudaryY(pBnd->curvePts,is,ie,ii);
				break;
			default:
				break;
			}
		}

	}

	//边界坐标
	void SmtBAOrthGrid::SampleBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J)
	{	
		double dDis = 0/*采点间距*/,AllDis = 0/*曲线总长*/,CountDis = 0/*累加距离*/,CurrentDis = 0/*距起点的距离*/;
		int CountIndex = 0/*曲线段号累计*/;
		int i = 0;
		int nSize = bnd.size();
		//始末点赋值
		m_pNodes->SetElement(bnd[0],J,iStart);	
		m_pNodes->SetElement(bnd[nSize-1],J,iEnd);

		double *disN = new double[nSize -1];//nb -1段曲线的距离

		for (i = 0; i < nSize - 1 ; i ++ )
		{//每段曲线的距离,disN[i]表示点号为i-i+1段的距离
			disN[i]=sqrt( (bnd[i].x-bnd[i+1].x)*(bnd[i].x-bnd[i+1].x) + (bnd[i].y-bnd[i+1].y)*(bnd[i].y-bnd[i+1].y)) ;
			AllDis += disN[i];
		}

		int nSection = abs(iEnd - iStart);
		dDis = AllDis/nSection;//采点间距

		dbfPoint P;
		//等间隔采点
		if (iEnd > iStart)
		{
			for (i = iStart+1; i < iEnd;i ++)
			{
				//采样点i距起点的距离
				CurrentDis = (i-iStart)*dDis;

				/*
				if((i-iStart)%2) CurrentDis = (i-iStart-0.5)*dDis;
				else CurrentDis = (i-iStart)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//找到采样点位于的曲线段CountIndex
					CountDis += disN[CountIndex++];
				}

				//插值得到采样点
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,J,i);
			}
		}else
		{
			for (i = iStart-1; i > iEnd;i --)
			{
				//采样点i距起点的距离
				CurrentDis = (iStart - i)*dDis;

				/*
				if((iStart - i)%2) CurrentDis = (iStart - i-0.5)*dDis;
				else CurrentDis = (iStart - i)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//找到采样点位于的曲线段CountIndex
					CountDis += disN[CountIndex++];
				}

				//插值得到采样点
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,J,i);
			}
		}

		delete []disN;
	}

	//边界坐标
	void SmtBAOrthGrid::SampleBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I)
	{	
		double dDis = 0/*采点间距*/,AllDis = 0/*曲线总长*/,CountDis = 0/*累加距离*/,CurrentDis = 0/*距起点的距离*/;
		int CountIndex = 0/*曲线段号累计*/;
		int i = 0;
		int nSize = bnd.size();
		//始末点赋值
		m_pNodes->SetElement(bnd[0],jStart,I);
		m_pNodes->SetElement(bnd[nSize-1],jEnd,I);

		double *disN = new double[nSize -1];//nb -1段曲线的距离

		for (i = 0; i < nSize - 1 ; i ++ )
		{//每段曲线的距离,disN[i]表示点号为i-i+1段的距离
			disN[i]=sqrt( (bnd[i].x-bnd[i+1].x)*(bnd[i].x-bnd[i+1].x) + (bnd[i].y-bnd[i+1].y)*(bnd[i].y-bnd[i+1].y)) ;
			AllDis += disN[i];
		}

		int nSection = abs(jEnd - jStart);
		dDis = AllDis/nSection;//采点间距

		dbfPoint P;
		//等间隔采点
		if (jEnd > jStart)
		{
			for (int j = jStart+1; j < jEnd;j ++)
			{
				//采样点i距起点的距离
				CurrentDis = j*dDis;
				/*
				if((j-jStart)%2) CurrentDis = (j-jStart-0.5)*dDis;
				else CurrentDis = (j-jStart)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//找到采样点位于的曲线段CountIndex
					CountDis += disN[CountIndex++];
				}

				//插值得到采样点
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,j,I);
			}
		}
		else
		{
			for (int j = jStart-1; j > jEnd;j --)
			{
				//采样点i距起点的距离
				CurrentDis = (jStart - j)*dDis;
				/*
				if((jStart - j)%2) CurrentDis = (jStart - j-0.5)*dDis;
				else CurrentDis = (jStart - j)*dDis;
				*/			
				while ( CurrentDis > CountDis )
				{//找到采样点位于的曲线段CountIndex
					CountDis += disN[CountIndex++];
				}

				//插值得到采样点
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,j,I);
			}
		}


		delete []disN;
	}

	//初始化网格X边界坐标
	void SmtBAOrthGrid::SetBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J)
	{
		int i = 0;
		if (iEnd > iStart)
		{
			for (i = iStart; i <= iEnd;i ++)
			{			
				m_pNodes->SetElement(bnd[i-iStart],J,i);
			}
		}
		else
		{
			for (i = iStart; i >= iEnd;i --)
			{
				m_pNodes->SetElement(bnd[iStart-i],J,i);
			}
		}

	}

	//初始化网格Y边界坐标
	void SmtBAOrthGrid::SetBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I)
	{
		if (jEnd > jStart)
		{
			for (int j = jStart; j <= jEnd;j ++)
			{
				m_pNodes->SetElement(bnd[j-jStart],j,I);
			}
		}
		else
		{
			for (int j = jStart; j >= jEnd;j --)
			{
				m_pNodes->SetElement(bnd[jStart-j],j,I);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//初始化内部网格节点，双线性插值
	void SmtBAOrthGrid::InitialInternal()
	{
		int i,j;
		dbfPoint P,P0,P1;
		//河床内部节点边界
		for (j = 1; j < m_nY -1; j ++)
		{
			for (i = 1; i < m_nX -1 ; i ++)
			{
				P0 = m_pNodes->GetElement(0,i);
				P1 = m_pNodes->GetElement(j,m_nX -1);
				P.x = P0.x + (P1.x-P0.x)*i/(m_nX-1);
				P.y = P0.y + (P1.y-P0.y)*i/(m_nX-1);
				m_pNodes->SetElement(P,j,i);
			}
		}
		//河床内部节点边界
		for (i = 1; i < m_nX -1; i ++)
		{
			for (j = 1; j < m_nY -1 ; j ++)
			{
				P0 = m_pNodes->GetElement(0,i);
				P1 = m_pNodes->GetElement(m_nY - 1,i);
				P.x = P0.x + (P1.x-P0.x)*j/(m_nY-1);
				P.y = P0.y + (P1.y-P0.y)*j/(m_nY-1);
				m_pNodes->SetElement(P,j,i);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//内部正交化
	void SmtBAOrthGrid::Orhogonal_SOR(void)
	{
		LONGLONG  time1 = 0,time2 = 0,per_cnt;
		QueryPerformanceCounter((LARGE_INTEGER *) &time1);
		QueryPerformanceFrequency((LARGE_INTEGER *) &per_cnt);
		float time = 0.,fTime_scale = 0.;
		fTime_scale = 1./per_cnt;
		int i,j,times;
		dbfPoint Pw,Ps,Pe,Pn,P1,P2,P3,P4,P,PP;
		//
		double eps = 1.,tmp_eps;
		double xks,xat,yks,yat,xksat,yksat,arfa,beta,gama,J,p = -.01,q = .01;

		double dx,dy,rx,ry,coef;

		Matrix2D<double> *pP = new Matrix2D<double>(m_nY,m_nX);
		Matrix2D<double> *pQ = new Matrix2D<double>(m_nY,m_nX);

		//松弛因子
		double nn = cos(PI/(m_nX - 1)) ,mm = cos(PI/(m_nY - 1));
		double w =4.0/(2.0+sqrt(4.0 - (nn+mm)*(nn+mm) ) );
		times = 0;

		while ( eps > ddEps && times < nMaxTimes )
		{
			eps = 0.;
			//	CalP3(pP);
			//	CalQ3(pQ);
			for (j = 1; j < m_nY - 1 ; j ++)
			{
				for (i = 1; i < m_nX - 1 ; i ++ )
				{
					if(IsOnDiggedRegion(i,j)) continue;

					Pe = m_pNodes->GetElement(j,i+1);
					Ps = m_pNodes->GetElement(j-1,i);
					Pw = m_pNodes->GetElement(j,i-1);
					Pn = m_pNodes->GetElement(j+1,i);
					P1 = m_pNodes->GetElement(j+1,i+1);
					P2 = m_pNodes->GetElement(j-1,i+1);
					P3 = m_pNodes->GetElement(j-1,i-1);	
					P4 = m_pNodes->GetElement(j+1,i-1);
					P  = m_pNodes->GetElement(j,i);


					xks = (Pe.x - Pw.x)/2.;
					xat = (Pn.x - Ps.x)/2.;

					yks = (Pe.y - Pw.y)/2.;
					yat = (Pn.y - Ps.y)/2.;

					xksat = (P1.x + P3.x - P2.x - P4.x)/4.;
					yksat = (P1.y + P3.y - P2.y - P4.y)/4.;

					arfa = xat*xat + yat*yat;
					gama = xks*xks + yks*yks;
					beta = xks*xat + yks*yat;
					J = xks*yat-xat*yks;

					//p = pP->GetElement(j,i);
					//q = pQ->GetElement(j,i);

					//dx = (arfa*(Pw.x + Pe.x)+gama*(Pn.x + Ps.x) +(arfa*p*xks+gama*q*xat) - 2*beta*xksat)/(2*(arfa +gama));
					//dy = (arfa*(Pw.y + Pe.y)+gama*(Pn.y + Ps.y) +(arfa*p*yks+gama*q*yat) - 2*beta*yksat)/(2*(arfa +gama));

					dx = (arfa*(Pw.x + Pe.x)+gama*(Pn.x + Ps.x) +J*(p*xks+q*xat) - 2*beta*xksat)/(2*(arfa +gama));
					dy = (arfa*(Pw.y + Pe.y)+gama*(Pn.y + Ps.y) +J*(p*yks+q*yat) - 2*beta*yksat)/(2*(arfa +gama));


					rx = dx - P.x;
					ry = dy - P.y;

					//松弛项
					PP.x = P.x + w*rx;
					PP.y = P.y + w*ry;

					m_pNodes->SetElement(PP,j,i);	

					tmp_eps = fabs(PP.x - P.x)/P.x;
					if (tmp_eps>eps) eps = tmp_eps;

					tmp_eps = fabs(PP.y - P.y)/P.y;
					if (tmp_eps>eps) eps = tmp_eps;					
				}
			}	

			SlideBoudary();	
			
			times ++;
		}

		QueryPerformanceCounter((LARGE_INTEGER *) &time2);
		time = (time2 - time1)*fTime_scale;

		std::ofstream fout("Orhgonal_SOR.txt",std::ios::app);
		if (fout.good())
		{
			fout << "SOR OMEGA:"<<w << std::endl;
			fout <<"eps "<<ddEps<<" " <<m_nX <<" " <<m_nY <<" time:" <<time << " times " << times<< std::endl;
		}	
		fout.close();

		SMT_SAFE_DELETE(pP);
		SMT_SAFE_DELETE(pQ);
	}

	//索引号是否在挖掘区
	bool SmtBAOrthGrid::IsOnDiggedRegion(int ii,int jj)
	{
		int i = 0;
		bool flag = false;
		while (!flag && i < m_rRegions.size())
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			flag = pRegion->HitTestOn(ii,jj);

			if(!flag)
				flag = pRegion->HitTestIn(ii,jj);

			i++;
		}
		return flag;
	}

	//索引号是否在挖掘区内部
	bool SmtBAOrthGrid::IsInDiggedRegion(int ii,int jj)
	{
		int i = 0;
		bool flag = false;
		while (!flag && i < m_rRegions.size())
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			flag = pRegion->HitTestIn(ii,jj);
			if(!flag) 
			{
				if(pRegion->HitTestOn(ii,jj))
					if(ii == m_nX - 1 || ii == 0 || jj == m_nY - 1 || jj == 0)
						if(!pRegion->HitTestOnCorner(ii,jj))
							flag = true;
			}
			i++;
		}
		return flag;
	}
	
	//////////////////////////////////////////////////////////////////////////
	//调整边界，使得边界保持正交
	void SmtBAOrthGrid::SlideBoudary()
	{
		int nBnds = m_rMainRegion.m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = m_rMainRegion.m_vBnds.at(i);
			if(pBnd->can_slide)
				SlideBoudary(pBnd);                              //滑动边界，使边界保持正交
		} 	

		SlideRegionBoudary();
	}

	//调整区边界
	void SmtBAOrthGrid::SlideRegionBoudary(void)
	{
		for (int i = 0; i < m_rRegions.size(); i ++)
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			SlideRegionBoudary(pRegion);
			AjustRegionCornerNode(pRegion);
		}
	}

	//调整区边界
	void SmtBAOrthGrid::SlideRegionBoudary(SmtBAOGRegion *pRegion)
	{
		int nBnds = pRegion->m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(i);
			if(pBnd->can_slide)SlideBoudary(pBnd);                              //滑动边界，使边界保持正交
		} 	
	}

	//调整区角点
	void SmtBAOrthGrid::AjustRegionCornerNode(SmtBAOGRegion *pRegion)
	{
		int nBnds = pRegion->m_vBnds.size();
		int ii,jj;
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(i);
			switch(pBnd->flag)
			{
			case 0:
			case 2:
				ii = pBnd->start;
				jj = pBnd->index;
				break;
			case 1:
			case 3:
				ii = pBnd->index;
				jj = pBnd->start;
				break;
			}

			AjustRegionCornerNode(ii,jj);
		} 	
	}

	//调整区角点
	void SmtBAOrthGrid::AjustRegionCornerNode(int i,int j)
	{
		dbfPoint Pw,Ps,Pe,Pn,P;
		Pe = m_pNodes->GetElement(j,i+1);
		Ps = m_pNodes->GetElement(j-1,i);
		Pw = m_pNodes->GetElement(j,i-1);
		Pn = m_pNodes->GetElement(j+1,i);

		P.x = (Pe.x+Ps.x+Pw.x+Pn.x)/4.; 
		P.y = (Pe.y+Ps.y+Pw.y+Pn.y)/4.; 

		m_pNodes->SetElement(P,j,i);

	}

	//调整边界
	void SmtBAOrthGrid::SlideBoudary(SmtBAOGBoudary * pBnd)
	{
		int is = pBnd->start,ie = pBnd->end,ii = pBnd->index,flag = pBnd->flag;
		switch(flag)
		{
		case 0:
		case 2:
			SlideBoudaryX(pBnd->curvePts,is,ie,ii);
			break;
		case 1:
		case 3:
			SlideBoudaryY(pBnd->curvePts,is,ie,ii);
			break;
		default:
			break;
		}
	}

	//调整边界 X方向
	void SmtBAOrthGrid::SlideBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J)
	{
		int i;
		int pre = 0,next = pre + 1;
		
		if (iEnd > iStart)
		{
			if(J > m_nY -2 ) 
				return;

			for (i = iStart + 1;i < iEnd; i ++ )
			{
				pre = 0,next = pre + 1;
				dbfPoint A,B,P,H;				
				int flag = 1;

				P = m_pNodes->GetElement(J,i);

				//找到P位于的边界线段
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(J+1,i);

				//过点P1作垂直于找到的边界线段的垂线，垂足为H，flag标记H是否位于线段上
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB延长线上，则作垂直于下一条曲线段的垂线，获得垂足H作为边界点的修正值
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//在thta角范围内,即在下一条曲线段上也找不到垂足
						continue;
					}
				}

				m_pNodes->SetElement(H,J,i);
			}
		}
		else
		{
			if(J < 1 ) 
				return;

			for (i = iStart - 1;i > iEnd; i -- )
			{
				pre = 0,next = pre + 1;
				dbfPoint A,B,P,H;				
				int flag = 1;

				P = m_pNodes->GetElement(J,i);

				//找到P位于的边界线段
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(J-1,i);

				//过点P1作垂直于找到的边界线段的垂线，垂足为H，flag标记H是否位于线段上
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB延长线上，则作垂直于下一条曲线段的垂线，获得垂足H作为边界点的修正值
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//在thta角范围内,即在下一条曲线段上也找不到垂足
						continue;
					}
				}

				m_pNodes->SetElement(H,J,i);
			}
		}
	}

	//调整边界 Y方向
	void SmtBAOrthGrid::SlideBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I)
	{
		int j;
		int pre = 0,next = pre + 1;
		
		if (jEnd > jStart)
		{
			if(I < 1) 
				return;

			for (j = jStart + 1;j < jEnd; j ++ )
			{
				pre = 0,next = pre + 1;
				dbfPoint A,B,P,H;				
				int flag = 1;

				P = m_pNodes->GetElement(j,I);

				//找到P位于的边界线段
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(j,I-1);

				//过点P1作垂直于找到的边界线段的垂线，垂足为H，flag标记H是否位于线段上
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB延长线上，则作垂直于下一条曲线段的垂线，获得垂足H作为边界点的修正值
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//在thta角范围内,即在下一条曲线段上也找不到垂足
						continue;
					}
				}

				m_pNodes->SetElement(H,j,I);
			}
		}
		else
		{
			if(I > m_nX - 2) 
				return;

			for (j = jStart - 1;j > jEnd; j -- )
			{
				pre = 0,next = pre + 1;
				dbfPoint A,B,P,H;				
				int flag = 1;

				P = m_pNodes->GetElement(j,I);

				//找到P位于的边界线段
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;
				 
				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(j,I+1);

				//过点P1作垂直于找到的边界线段的垂线，垂足为H，flag标记H是否位于线段上
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB延长线上，则作垂直于下一条曲线段的垂线，获得垂足H作为边界点的修正值
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//在thta角范围内,即在下一条曲线段上也找不到垂足
						continue;
					}
				}

				m_pNodes->SetElement(H,j,I);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//计算正交化程度
	void SmtBAOrthGrid::CalGridOrthogonality()
	{
		dbfPoint Pw,Ps,Pe,Pn;
		float xks,xat,yks,yat,arfa,beta,gama;
		float ftheta = 0.,fdelta = 0.;

		int i,j;
		for (i = 1; i < m_nX - 1 ; i ++ )
		{
			for (j = 1; j < m_nY - 1 ; j ++)
			{
				if(IsOnDiggedRegion(i,j)) continue;
				Pe = m_pNodes->GetElement(j,i+1);
				Ps = m_pNodes->GetElement(j-1,i);
				Pw = m_pNodes->GetElement(j,i-1);
				Pn = m_pNodes->GetElement(j+1,i);		

				xks = (Pe.x - Pw.x)/2.;
				xat = (Pn.x - Ps.x)/2.;

				yks = (Pe.y - Pw.y)/2.;
				yat = (Pn.y - Ps.y)/2.;

				arfa = xat*xat + yat*yat;
				gama = xks*xks + yks*yks;
				beta = xks*xat + yks*yat;

				ftheta = acos(beta/sqrt(arfa*gama))*180./PI;
				fdelta = fabs(90 - ftheta);

				m_pOrthogonality->SetElement(fdelta,j,i);

			}
		}
	}

	void SmtBAOrthGrid::AddOrth(int i, int j,float add)
	{   
		float orth = m_pOrthogonality->GetElement(j,i);
		orth +=add;
		m_pOrthogonality->SetElement(orth,j,i);
	}

	//////////////////////////////////////////////////////////////////////////
	//设置网格单元
	void SmtBAOrthGrid::SetGridCell()
	{
		int i,j;
		//////////////////////////////////////////////////////////////////////////
		float orth;
		dbfPoint A,B,C,D;
		GridCell cell;
		for (i = 0; i < m_nX - 1; i++)
		{
			for (j = 0; j < m_nY - 1; j++)
			{
				A = m_pNodes->GetElement(j,i);
				B = m_pNodes->GetElement(j,i+1);
				C = m_pNodes->GetElement(j+1,i+1);
				D = m_pNodes->GetElement(j+1,i);

				cell = m_pCells->GetElement(j,i);

				if (InValid(A)||InValid(B)||InValid(C)||InValid(D)) 
				{
					cell.IsSelected = false;
					cell.P.x = fInvalidNum;
					cell.P.y = fInvalidNum;
					m_pCells->SetElement(cell,j,i);
					continue;
				}

				cell.IsSelected = true;
				cell.P.x = (A.x+B.x+C.x+D.x)/4;
				cell.P.y = (A.y+B.y+C.y+D.y)/4;

				orth = m_pOrthogonality->GetElement(j,i);
				orth += m_pOrthogonality->GetElement(j,i+1);
				orth += m_pOrthogonality->GetElement(j+1,i+1);
				orth += m_pOrthogonality->GetElement(j+1,i);
				
				m_pCells->SetElement(cell,j,i);
			}
		}
	}

	void SmtBAOrthGrid::SetDiggedRegionInvalid(void)
	{
		dbfPoint P;
		P.x = fInvalidNum;
		P.y = fInvalidNum;
		for (int i = 0; i < m_nX ; i ++)
		{
			for (int j = 0; j < m_nY ;j ++)
			{
				if(IsInDiggedRegion(i,j)) 
					m_pNodes->SetElement(P,j,i);
			}
		}

		dbfPoint A,B,C,D;
		GridCell cell;
		for (int i = 0; i < m_nX - 1; i++)
		{
			for (int j = 0; j < m_nY - 1; j++)
			{
				A = m_pNodes->GetElement(j,i);
				B = m_pNodes->GetElement(j,i+1);
				C = m_pNodes->GetElement(j+1,i+1);
				D = m_pNodes->GetElement(j+1,i);

				cell = m_pCells->GetElement(j,i);

				if (InValid(A)||InValid(B)||InValid(C)||InValid(D)) 
				{
					cell.IsSelected = false;
					cell.P.x = fInvalidNum;
					cell.P.y = fInvalidNum;
					m_pCells->SetElement(cell,j,i);
					continue;
				}
			}
		}
	}
}