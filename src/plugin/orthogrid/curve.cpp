#include "plugin/orthogrid/curve.h"
#include "plugin/orthogrid/types.h"
#include "plugin/orthogrid/detail/polyline.h"

#include <cmath>

namespace orthogrid
{
	void  Spline3(vCtrlPoints &ctrlPts,vCurvePoints &curvePts,int m)
	{
		if(ctrlPts.size() < 2) 
			return;

		vCurvePoints interPts;	

		interPts.push_back(ctrlPts[0].P);
		int nCtrlPts = ctrlPts.size();

		if (nCtrlPts > 2)
		{
			int n = nCtrlPts -1;
			int i , j;
			double *Q1_x , *Q1_y , * alfa_x, *alfa_y , * beta_x , * beta_y , * gama_x, * gama_y;
			double ax,ay,bx,by,cx,cy,dx,dy,t;	
			Q1_x = new double [n+1];
			Q1_y = new double [n+1];
			alfa_x = new double [n+1];
			alfa_y = new double [n+1];
			beta_x = new double [n+1];
			beta_y = new double [n+1];
			gama_x = new double [n+1];
			gama_y = new double [n+1];
			float x , y;
			Q1_x[0] = 1; Q1_y[0] = 1;Q1_x[n] = 1; Q1_y[n] = 1;
			beta_x[0] = 0;beta_y[0] = 0; alfa_x[0] = 0;alfa_y[0] = 0;
			gama_x[1]=3*(ctrlPts[2].P.x - ctrlPts[0].P.x ) -Q1_x[0];
			gama_y[1]=3*(ctrlPts[2].P.y - ctrlPts[0].P.y ) -Q1_y[0];
			gama_x[n-1]=3*(ctrlPts[n].P.x - ctrlPts[n-2].P.x ) -Q1_x[n];
			gama_y[n-1]=3*(ctrlPts[n].P.y - ctrlPts[n-2].P.y ) -Q1_y[n];
			for ( i =1 ;i < n ;i++)
			{
				if ( i != 1 && i != n-1)
				{
					gama_x[i]=3*(ctrlPts[i+1].P.x - ctrlPts[i-1].P.x ) ;
					gama_y[i]=3*(ctrlPts[i+1].P.y - ctrlPts[i-1].P.y ) ;
				}
				beta_x[i] = -1/(4+beta_x[i-1]);
				beta_y[i] = -1/(4+beta_y[i-1]);
				alfa_x[i] = (alfa_x[i-1]-gama_x[i])*beta_x[i];
				alfa_y[i] = (alfa_y[i-1]-gama_y[i])*beta_y[i];
			}
			for ( i = n-1 ;i > 0 ;i --)
			{
				Q1_x[i] = alfa_x[i]+beta_x[i]*Q1_x[i+1];
				Q1_y[i] = alfa_y[i]+beta_y[i]*Q1_y[i+1];
			}


			for ( i =1 ;i <= n ;i++)
			{
				ax = Q1_x[i] + Q1_x[i-1] - 2*(ctrlPts[i].P.x - ctrlPts[i-1].P.x);
				ay = Q1_y[i] + Q1_y[i-1] - 2*(ctrlPts[i].P.y - ctrlPts[i-1].P.y);
				bx = -Q1_x[i] - 2*Q1_x[i-1] + 3*(ctrlPts[i].P.x - ctrlPts[i-1].P.x);
				by = -Q1_y[i] - 2*Q1_y[i-1] + 3*(ctrlPts[i].P.y - ctrlPts[i-1].P.y);
				cx = Q1_x[i-1];
				cy = Q1_y[i-1];
				dx = ctrlPts[i-1].P.x;
				dy = ctrlPts[i-1].P.y;
				for ( j = 1 ;j <= m ; j++ )
				{
					dbfPoint pt;
					t = (float)j/m;
					x = ax*t*t*t + bx*t*t + cx * t +dx;
					y = ay*t*t*t + by*t*t + cy * t +dy;

					pt.x = x;
					pt.y = y;
					interPts.push_back(pt);
				}
			}

			delete []Q1_x;
			delete []Q1_y;
			delete []alfa_x;
			delete []alfa_y;
			delete []beta_x;
			delete []beta_y;
			delete []gama_x;
			delete []gama_y;
		}
		interPts.push_back(ctrlPts[ctrlPts.size()-1].P);

		CurveSampling(interPts,curvePts,m);
		GetCtrlPointsPosOnCurve(ctrlPts,curvePts);
	}

	//�߽�����
	void  CurveSampling(vCurvePoints &orgCurve,vCurvePoints &newCurve,int N)
	{//�����ڱ߽�(0,J)--(nb-1,J)֮���NX-2����ʹ����NX-2�����ڱ߽��ϣ����ҵ�֮���ǵȾ��

		if(N < 2) 
			return;

		int nOrgPts = orgCurve.size();

		if (nOrgPts < 2) 
			return;

		double dDis = 0/*�ɵ���*/,AllDis = 0/*�����ܳ�*/,CountDis = 0/*�ۼӾ���*/,CurrentDis = 0/*�����ľ���*/;
		int CountIndex = 0/*���߶κ��ۼ�*/;
		int i = 0;
		//	newCurve.resize(N);
		//ʼĩ�㸳ֵ
		newCurve[0] = orgCurve[0];
		newCurve[N-1] = orgCurve[nOrgPts - 1];

		double *disN = new double[nOrgPts -1];//nb -1�����ߵľ���

		for (i = 0; i < nOrgPts - 1 ; i ++ )
		{//ÿ�����ߵľ���,disN[i]��ʾ���Ϊi-i+1�εľ���
			disN[i]= std::hypot(orgCurve[i+1].x - orgCurve[i].x,
			                    orgCurve[i+1].y - orgCurve[i].y);
			AllDis += disN[i];
		}

		dDis = AllDis/(N-1);//�ɵ���

		//�ȼ���ɵ�
		for (i = 1; i < N - 1;i ++)
		{
			//������i�����ľ���
			CurrentDis = i*dDis;

			while ( CurrentDis > CountDis )
			{//�ҵ�������λ�ڵ����߶�CountIndex
				CountDis += disN[CountIndex++];
			}

			//��ֵ�õ�������
			double t = ( CountDis - CurrentDis)/disN[CountIndex-1];

			newCurve[i].x = (1-t)*orgCurve[CountIndex].x + t*orgCurve[CountIndex-1].x;
			newCurve[i].y = (1-t)*orgCurve[CountIndex].y + t*orgCurve[CountIndex-1].y;
		}

		delete []disN;
	}

	void  GetCtrlPointsPosOnCurve(vCtrlPoints &ctrlPts,vCurvePoints &curvePts)
	{
		int preindex = 0,nextindex = 1;

		for (int i = 0; i < ctrlPts.size(); i ++)
		{
			orthogrid::detail::locate_on_polyline(ctrlPts[i].P,curvePts,preindex,nextindex);
			ctrlPts[i].PreIndex = preindex;
			ctrlPts[i].NexIndex = nextindex;
		}
	}

	void  SetCtrlPoints(vCtrlPoints &ctrlPts,vdbfPoints & Pts)
	{
		for (int i = 0; i < Pts.size(); i ++)
		{
			ctrlPts[i].P = Pts[i];
			ctrlPts[i].PreIndex = i - 1;
			ctrlPts[i].NexIndex = i + 1;
		}
	}

	void  SetCurvePoints(vCtrlPoints &ctrlPts,vdbfPoints &Pts)
	{
		for (int i = 0; i < ctrlPts.size(); i ++)
		{
			Pts[i] = ctrlPts[i].P;
		}
	}
}
