/*
说明：采用魏文礼法
*/
#include "baog_baorthgrid.h"

namespace Smt_BAOrthGrid
{
	void  SmtBAOrthGrid::CalP2(Matrix2D<double> *pP)
	{
		int i ,j;
		dbfPoint Pw,Pe,P;
		dbfPoint P1,P2;

		double xks,xksks,xat,xatat,yks,yksks,yat,yatat,arfa,gama,f;  
		//j = 0
		for (i = 1;i < m_nX - 1 ; i++)
		{
			P =  m_pNodes->GetElement(0,i);
			Pe = m_pNodes->GetElement(0,i+1);
			Pw = m_pNodes->GetElement(0,i-1);
			P1 = m_pNodes->GetElement(1,i);
			P2 = m_pNodes->GetElement(2,i);

			xks = (Pe.x - Pw.x)/2.;
			xksks = Pe.x -2*P.x+ Pw.x;
			yks = (Pe.y - Pw.y)/2.;
			yksks = Pe.y -2*P.y+ Pw.y;

			xat   = (-3*P.x + 4*P1.x - P2.x)/2;	
			xatat = 2*(P1.x-P.x)-2*xat;
			yat   = (-3*P.y + 4*P1.y - P2.y)/2;
			yatat = 2*(P1.y-P.y)-2*yat;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = -(xks*xksks+yks*yksks)/gama+(xks*xatat+yks*yatat)/arfa;

			pP->SetElement(f,0,i);
		}

		//j = m_nY-1
		for (i = 1;i < m_nX - 1 ; i++)
		{
			P =  m_pNodes->GetElement(m_nY-1,i);
			Pe = m_pNodes->GetElement(m_nY-1,i+1);
			Pw = m_pNodes->GetElement(m_nY-1,i-1);
			P1 = m_pNodes->GetElement(m_nY-2,i);
			P2 = m_pNodes->GetElement(m_nY-3,i);

			xks = (Pe.x - Pw.x)/2.;
			xksks = Pe.x -2*P.x+ Pw.x;
			yks = (Pe.y - Pw.y)/2.;
			yksks = Pe.y -2*P.y+ Pw.y;

			xat   = (-3*P.x + 4*P1.x - P2.x)/2;	
			xatat = 2*(P.x-P1.x)+2*xat;
			yat   = (-3*P.y + 4*P1.y - P2.y)/2;
			yatat = 2*(P.y-P1.y)+2*yat;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = -(xks*xksks+yks*yksks)/gama+(xks*xatat+yks*yatat)/arfa;

			pP->SetElement(f,m_nY-1,i);
		}


		//内部插值
		double f0,f1;
		for (i = 1; i < m_nX -1 ; i++)
		{
			for (j = 1; j < m_nY -1 ; j++)
			{
				f0 = pP->GetElement(0,i);
				f1 = pP->GetElement(m_nY-1,i);

				f = f0 + (f1-f0)*j/(m_nY-1);

				pP->SetElement(f,j,i);
			}
		}
	}

	void  SmtBAOrthGrid::CalQ2(Matrix2D<double> *pQ)
	{
		int i ,j;
		dbfPoint Ps,Pn,P;
		dbfPoint P1,P2;

		double xks,xksks,xat,xatat,yks,yksks,yat,yatat,arfa,gama,f; 
		//i= 0
		for (j = 1; j < m_nY - 1; j++)
		{
			P =  m_pNodes->GetElement(j,0);
			Pn = m_pNodes->GetElement(j+1,0);
			Ps = m_pNodes->GetElement(j-1,0);
			P1 = m_pNodes->GetElement(j,1);
			P2 = m_pNodes->GetElement(j,2);

			xks   = (-3*P.x + 4*P1.x - P2.x)/2;	
			xksks = 2*(P1.x-P.x)-2*xat;
			yks   = (-3*P.y + 4*P1.y - P2.y)/2;
			yksks = 2*(P1.y-P.y)-2*yat;

			xat = (Pn.x - Ps.x)/2.;
			xatat = Pn.x -2*P.x+ Ps.x;
			yat = (Pn.y - Ps.y)/2.;
			yatat = Pn.y -2*P.y+ Ps.y;


			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = -(xat*xatat+yat*yatat)/arfa+(xat*xksks+yat*yksks)/gama;

			pQ->SetElement(f,j,0);
		}

		//i= m_nX -1
		for (j = 1; j < m_nY - 1; j++)
		{
			P =  m_pNodes->GetElement(j,m_nX -1);
			Pn = m_pNodes->GetElement(j+1,m_nX -1);
			Ps = m_pNodes->GetElement(j-1,m_nX -1);
			P1 = m_pNodes->GetElement(j,m_nX -2);
			P2 = m_pNodes->GetElement(j,m_nX -3);

			xks   = (-3*P.x + 4*P1.x - P2.x)/2;	
			xksks = 2*(P.x-P1.x)+2*xat;
			yks   = (-3*P.y + 4*P1.y - P2.y)/2;
			yksks = 2*(P.y-P1.y)+2*yat;

			xat = (Pn.x - Ps.x)/2.;
			xatat = Pn.x -2*P.x+ Ps.x;
			yat = (Pn.y - Ps.y)/2.;
			yatat = Pn.y -2*P.y+ Ps.y;


			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = -(xat*xatat+yat*yatat)/arfa+(xat*xksks+yat*yksks)/gama;

			pQ->SetElement(f,j,m_nX -1);
		}

		//内部插值
		double f0,f1;
		for (j = 1; j < m_nY -1 ; j++)
		{
			for (i = 1; i < m_nX -1 ; i++)
			{
				f0 = pQ->GetElement(j,0);
				f1 = pQ->GetElement(j,m_nX -1);

				f = f0 + (f1-f0)*i/(m_nX -1);

				pQ->SetElement(f,j,i);
			}
		}
	}
}