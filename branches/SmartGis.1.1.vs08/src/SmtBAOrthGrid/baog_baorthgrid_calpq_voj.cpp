/*
说明：采用文章Generation of curvilinear coordinates on multiply connected regions with boundary singularities 中的构造方法
*/
#include "baog_baorthgrid.h"

namespace Smt_BAOrthGrid
{
	void  SmtBAOrthGrid::CalP1(Matrix2D<double> *pP)
	{
		int i ,j;
		dbfPoint Pw,Pe,P;

		double xks,xksks,yks,yksks,arfa,gama,f;  
		//j = 1
		for (i = 1;i < m_nX - 1 ; i++)
		{
			P = m_pNodes->GetElement(0,i);
			Pe = m_pNodes->GetElement(0,i+1);
			Pw = m_pNodes->GetElement(0,i-1);

			xks = (Pe.x - Pw.x)/2.;
			xksks = Pe.x -2*P.x+ Pw.x;
			yks = (Pe.y - Pw.y)/2.;
			yksks = Pe.y -2*P.y+ Pw.y;

			if(xksks < 0.001) 
				xksks = 0;

			if(yksks < 0.001) 
				yksks = 0;

			gama = xks*xks + yks*yks;
			f = -(xks*xksks+yks*yksks)/gama;

			pP->SetElement(f,0,i);
		}

		//j = m_nY-2
		for (i = 1;i < m_nX - 1 ; i++)
		{
			P = m_pNodes->GetElement(m_nY-1,i);
			Pe = m_pNodes->GetElement(m_nY-1,i+1);
			Pw = m_pNodes->GetElement(m_nY-1,i-1);
			xks = (Pe.x - Pw.x)/2.;
			xksks = Pe.x -2*P.x+ Pw.x;
			yks = (Pe.y - Pw.y)/2.;
			yksks = Pe.y -2*P.y+ Pw.y;

			if(xksks < 0.001) 
				xksks = 0;

			if(yksks < 0.001) 
				yksks = 0;

			gama = xks*xks + yks*yks;
			f = -(xks*xksks+yks*yksks)/gama;

			pP->SetElement(f,m_nY-1,i);
		}

		//内部插值
		double f0,f1;
		for (i = 1; i < m_nX -1 ; i++)
		{
			f0 = pP->GetElement(0,i);
			f1 = pP->GetElement(m_nY-1,i);
			for (j = 1; j < m_nY -1 ; j++)
			{			
				f = f0 + (f1-f0)*j/(m_nY-1);
				pP->SetElement(f,i,j);
			}
		}
	}

	void  SmtBAOrthGrid::CalQ1(Matrix2D<double> *pQ)
	{
		int i ,j;
		dbfPoint Ps,Pn,P;

		double xat,yat,xatat,yatat,arfa,gama,f;  
		//i= 1
		for (j = 1; j < m_nY - 1; j++)
		{
			P = m_pNodes->GetElement(j,0);
			Pn = m_pNodes->GetElement(j+1,0);
			Ps = m_pNodes->GetElement(j-1,0);
			xat = (Pn.x - Ps.x)/2.;
			xatat = Pn.x -2*P.x+ Ps.x;
			yat = (Pn.y - Ps.y)/2.;
			yatat = Pn.y -2*P.y+ Ps.y;

			if(xatat < 0.001) 
				xatat = 0;

			if(yatat < 0.001) 
				yatat = 0;

			arfa = xat*xat + yat*yat;
			f = -(xat*xatat+yat*yatat)/arfa;

			pQ->SetElement(f,j,0);
		}

		//i= m_nX -2
		for (j = 1; j < m_nY - 1; j++)
		{
			P = m_pNodes->GetElement(j,m_nX -1);
			Pn = m_pNodes->GetElement(j+1,m_nX -1);
			Ps = m_pNodes->GetElement(j-1,m_nX -1);

			xat = (Pn.x - Ps.x)/2.;
			xatat = Pn.x -2*P.x+ Ps.x;
			yat = (Pn.y - Ps.y)/2.;
			yatat = Pn.y -2*P.y+ Ps.y;

			if(xatat < 0.001) 
				xatat = 0;

			if(yatat < 0.001)
				yatat = 0;

			arfa = xat*xat + yat*yat;
			f = -(xat*xatat+yat*yatat)/arfa;

			pQ->SetElement(f,j,m_nX -1);
		}

		//内部插值
		double f0,f1;
		for (j = 1; j < m_nY -1 ; j++)
		{
			f0 = pQ->GetElement(j,0);
			f1 = pQ->GetElement(j,m_nX -1);

			for (i = 1; i < m_nX -1 ; i++)
			{
				f = f0 + (f1-f0)*i/(m_nX -1);

				pQ->SetElement(f,j,i);
			}
		}	
	}
}