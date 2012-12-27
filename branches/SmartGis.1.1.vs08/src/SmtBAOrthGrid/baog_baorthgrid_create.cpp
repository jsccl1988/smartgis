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
	//��������
	long SmtBAOrthGrid::CreateOrthGrid()
	{
		if (m_nX == 0 || m_nY == 0)
			return SMT_ERR_INVALID_PARAM;
		 
		InitialGrid();                   //��ʼ������
		Orhogonal_SOR();                 //������	
		SetDiggedRegionInvalid();        //������ȥ���ڽڵ�����Ϊ��Чֵ
		CalGridOrthogonality();			 //���������������̶�
		SetGridCell();                   //��������Ԫ

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
	//��ʼ������
	void SmtBAOrthGrid::InitialGrid(void)   
	{
		float orth = 0.;
		for (int i = 0; i < m_nX; i ++)
			for (int j = 0; j < m_nY ; j++)
				m_pOrthogonality->SetElement(orth,j,i);

		//��ʼ���߽��
		InitialBoudary();
		//��ʼ���ڲ�����ڵ�
		InitialInternal();
		//��ʼ���ھ����߽�����
		InitialRegionBoudary();
	}     

	//��ʼ���߽�
	void SmtBAOrthGrid::InitialBoudary()
	{
		int nBnds = m_rMainRegion.m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = m_rMainRegion.m_vBnds.at(i);
			SampleBoudary(pBnd);                              //�ȼ������߽��
		} 
	}

	//��ʼ�����߽�
	void SmtBAOrthGrid::InitialRegionBoudary()
	{
		for (int i = 0; i < m_rRegions.size(); i ++)
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			InitialRegionBoudary(pRegion);
		}
	}

	//��ʼ�����߽�
	void SmtBAOrthGrid::InitialRegionBoudary(SmtBAOGRegion * pRegion)
	{
		int nBnds = pRegion->m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(i);
			SampleBoudary(pBnd);                              //�ȼ������߽��
		} 
	}

	//�ȼ������߽�����
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

	//�߽�����
	void SmtBAOrthGrid::SampleBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J)
	{	
		double dDis = 0/*�ɵ���*/,AllDis = 0/*�����ܳ�*/,CountDis = 0/*�ۼӾ���*/,CurrentDis = 0/*�����ľ���*/;
		int CountIndex = 0/*���߶κ��ۼ�*/;
		int i = 0;
		int nSize = bnd.size();
		//ʼĩ�㸳ֵ
		m_pNodes->SetElement(bnd[0],J,iStart);	
		m_pNodes->SetElement(bnd[nSize-1],J,iEnd);

		double *disN = new double[nSize -1];//nb -1�����ߵľ���

		for (i = 0; i < nSize - 1 ; i ++ )
		{//ÿ�����ߵľ���,disN[i]��ʾ���Ϊi-i+1�εľ���
			disN[i]=sqrt( (bnd[i].x-bnd[i+1].x)*(bnd[i].x-bnd[i+1].x) + (bnd[i].y-bnd[i+1].y)*(bnd[i].y-bnd[i+1].y)) ;
			AllDis += disN[i];
		}

		int nSection = abs(iEnd - iStart);
		dDis = AllDis/nSection;//�ɵ���

		dbfPoint P;
		//�ȼ���ɵ�
		if (iEnd > iStart)
		{
			for (i = iStart+1; i < iEnd;i ++)
			{
				//������i�����ľ���
				CurrentDis = (i-iStart)*dDis;

				/*
				if((i-iStart)%2) CurrentDis = (i-iStart-0.5)*dDis;
				else CurrentDis = (i-iStart)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//�ҵ�������λ�ڵ����߶�CountIndex
					CountDis += disN[CountIndex++];
				}

				//��ֵ�õ�������
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,J,i);
			}
		}else
		{
			for (i = iStart-1; i > iEnd;i --)
			{
				//������i�����ľ���
				CurrentDis = (iStart - i)*dDis;

				/*
				if((iStart - i)%2) CurrentDis = (iStart - i-0.5)*dDis;
				else CurrentDis = (iStart - i)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//�ҵ�������λ�ڵ����߶�CountIndex
					CountDis += disN[CountIndex++];
				}

				//��ֵ�õ�������
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,J,i);
			}
		}

		delete []disN;
	}

	//�߽�����
	void SmtBAOrthGrid::SampleBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I)
	{	
		double dDis = 0/*�ɵ���*/,AllDis = 0/*�����ܳ�*/,CountDis = 0/*�ۼӾ���*/,CurrentDis = 0/*�����ľ���*/;
		int CountIndex = 0/*���߶κ��ۼ�*/;
		int i = 0;
		int nSize = bnd.size();
		//ʼĩ�㸳ֵ
		m_pNodes->SetElement(bnd[0],jStart,I);
		m_pNodes->SetElement(bnd[nSize-1],jEnd,I);

		double *disN = new double[nSize -1];//nb -1�����ߵľ���

		for (i = 0; i < nSize - 1 ; i ++ )
		{//ÿ�����ߵľ���,disN[i]��ʾ���Ϊi-i+1�εľ���
			disN[i]=sqrt( (bnd[i].x-bnd[i+1].x)*(bnd[i].x-bnd[i+1].x) + (bnd[i].y-bnd[i+1].y)*(bnd[i].y-bnd[i+1].y)) ;
			AllDis += disN[i];
		}

		int nSection = abs(jEnd - jStart);
		dDis = AllDis/nSection;//�ɵ���

		dbfPoint P;
		//�ȼ���ɵ�
		if (jEnd > jStart)
		{
			for (int j = jStart+1; j < jEnd;j ++)
			{
				//������i�����ľ���
				CurrentDis = j*dDis;
				/*
				if((j-jStart)%2) CurrentDis = (j-jStart-0.5)*dDis;
				else CurrentDis = (j-jStart)*dDis;
				*/

				while ( CurrentDis > CountDis )
				{//�ҵ�������λ�ڵ����߶�CountIndex
					CountDis += disN[CountIndex++];
				}

				//��ֵ�õ�������
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
				//������i�����ľ���
				CurrentDis = (jStart - j)*dDis;
				/*
				if((jStart - j)%2) CurrentDis = (jStart - j-0.5)*dDis;
				else CurrentDis = (jStart - j)*dDis;
				*/			
				while ( CurrentDis > CountDis )
				{//�ҵ�������λ�ڵ����߶�CountIndex
					CountDis += disN[CountIndex++];
				}

				//��ֵ�õ�������
				double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

				P.x = (1-t)*bnd[CountIndex].x + t*bnd[CountIndex-1].x;
				P.y = (1-t)*bnd[CountIndex].y + t*bnd[CountIndex-1].y;

				m_pNodes->SetElement(P,j,I);
			}
		}


		delete []disN;
	}

	//��ʼ������X�߽�����
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

	//��ʼ������Y�߽�����
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
	//��ʼ���ڲ�����ڵ㣬˫���Բ�ֵ
	void SmtBAOrthGrid::InitialInternal()
	{
		int i,j;
		dbfPoint P,P0,P1;
		//�Ӵ��ڲ��ڵ�߽�
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
		//�Ӵ��ڲ��ڵ�߽�
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
	//�ڲ�������
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

		//�ɳ�����
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

					//�ɳ���
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

	//�������Ƿ����ھ���
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

	//�������Ƿ����ھ����ڲ�
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
	//�����߽磬ʹ�ñ߽籣������
	void SmtBAOrthGrid::SlideBoudary()
	{
		int nBnds = m_rMainRegion.m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = m_rMainRegion.m_vBnds.at(i);
			if(pBnd->can_slide)
				SlideBoudary(pBnd);                              //�����߽磬ʹ�߽籣������
		} 	

		SlideRegionBoudary();
	}

	//�������߽�
	void SmtBAOrthGrid::SlideRegionBoudary(void)
	{
		for (int i = 0; i < m_rRegions.size(); i ++)
		{
			SmtBAOGRegion *pRegion =  m_rRegions.at(i);
			SlideRegionBoudary(pRegion);
			AjustRegionCornerNode(pRegion);
		}
	}

	//�������߽�
	void SmtBAOrthGrid::SlideRegionBoudary(SmtBAOGRegion *pRegion)
	{
		int nBnds = pRegion->m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(i);
			if(pBnd->can_slide)SlideBoudary(pBnd);                              //�����߽磬ʹ�߽籣������
		} 	
	}

	//�������ǵ�
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

	//�������ǵ�
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

	//�����߽�
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

	//�����߽� X����
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

				//�ҵ�Pλ�ڵı߽��߶�
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(J+1,i);

				//����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
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

				//�ҵ�Pλ�ڵı߽��߶�
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(J-1,i);

				//����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
						continue;
					}
				}

				m_pNodes->SetElement(H,J,i);
			}
		}
	}

	//�����߽� Y����
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

				//�ҵ�Pλ�ڵı߽��߶�
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;

				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(j,I-1);

				//����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
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

				//�ҵ�Pλ�ڵı߽��߶�
				SmtLocate(P,bnd,pre,next);
				if (next >= bnd.size())
					continue;
				 
				A = bnd[pre];
				B = bnd[next];
				P = m_pNodes->GetElement(j,I+1);

				//����P1����ֱ���ҵ��ı߽��߶εĴ��ߣ�����ΪH��flag���H�Ƿ�λ���߶���
				flag = SmtGetH(A,B,P,H);

				if (flag == 1)
				{//AB�ӳ����ϣ�������ֱ����һ�����߶εĴ��ߣ���ô���H��Ϊ�߽�������ֵ
					pre ++;
					next ++;

					if (next >= bnd.size())
						continue;

					A = bnd[pre];
					B = bnd[next];

					flag = SmtGetH(A,B,P,H);

					if(flag == -1)
					{//��thta�Ƿ�Χ��,������һ�����߶���Ҳ�Ҳ�������
						continue;
					}
				}

				m_pNodes->SetElement(H,j,I);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//�����������̶�
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
	//��������Ԫ
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