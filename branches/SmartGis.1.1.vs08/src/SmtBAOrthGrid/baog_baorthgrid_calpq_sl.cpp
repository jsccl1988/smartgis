/*
说明：采用势流理论构造的PQ
*/
#include "baog_baorthgrid.h"
#include <cmath>

namespace Smt_BAOrthGrid
{
	void  SmtBAOrthGrid::CalP0(Matrix2D<double> *pP)
	{
		int i ,j;
		dbfPoint Pw,Ps,Pe,Pn;
		dbfPoint P0,P1,P2;

		double xks,xat,yks,yat,xksat,yksat,arfa,gama;
		double K1,K2;
		double f;

		//j = 1;
		for (i = 1; i < m_nX -1 ; i++)
		{
			P0 = m_pNodes->GetElement(0,i);
			P1 = m_pNodes->GetElement(1,i);
			P2 = m_pNodes->GetElement(2,i);
			Pe = m_pNodes->GetElement(1,i+1);
			Pw = m_pNodes->GetElement(1,i-1);
			xat   = (-3*P0.x + 4*P1.x - P2.x)/2;		
			yat   = (-3*P0.y + 4*P1.y - P2.y)/2;
			xks = (Pe.x - Pw.x)/2.;
			yks = (Pe.y - Pw.y)/2.;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K1 =sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(2,i+1);
			Ps = m_pNodes->GetElement(1,i);
			Pw = m_pNodes->GetElement(2,i-1);
			Pn = m_pNodes->GetElement(3,i);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K2 = sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(1,i+1);
			Ps = m_pNodes->GetElement(0,i);
			Pw = m_pNodes->GetElement(1,i-1);
			Pn = m_pNodes->GetElement(2,i);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = - log(K1/K2)/gama;
			pP->SetElement(f,1,i);
		}

		//j = m_nY - 2;
		for (i = 1; i < m_nX -1 ; i++)
		{
			P0 = m_pNodes->GetElement(m_nY - 1,i);
			P1 = m_pNodes->GetElement(m_nY - 2,i);
			P2 = m_pNodes->GetElement(m_nY - 3,i);
			Pe = m_pNodes->GetElement(m_nY - 1,i+1);
			Pw = m_pNodes->GetElement(m_nY - 1,i-1);
			xat = (-3*P0.x + 4*P1.x - P2.x)/2;		
			yat = (-3*P0.y + 4*P1.y - P2.y)/2;
			xks = (Pe.x - Pw.x)/2.;
			yks = (Pe.y - Pw.y)/2.;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K1 = log(gama/arfa);

			Pe = m_pNodes->GetElement(m_nY-3,i+1);
			Ps = m_pNodes->GetElement(m_nY-4,i);
			Pw = m_pNodes->GetElement(m_nY-3,i-1);
			Pn = m_pNodes->GetElement(m_nY-2,i);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K2 = log(gama/arfa);

			Pe = m_pNodes->GetElement(m_nY-2,i+1);
			Ps = m_pNodes->GetElement(m_nY-3,i);
			Pw = m_pNodes->GetElement(m_nY-2,i-1);
			Pn = m_pNodes->GetElement(m_nY-1,i);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = - log(K1/K2)/gama;
			pP->SetElement(f,m_nY - 2,i);
		}

		for (j = 2; j < m_nY -2;j++)	
		{
			for (i = 2; i < m_nX -2 ; i++)
			{
				Pe = m_pNodes->GetElement(j,i);
				Ps = m_pNodes->GetElement(j-1,i-1);
				Pw = m_pNodes->GetElement(j,i-2);
				Pn = m_pNodes->GetElement(j+1,i-1);
				xks = (Pe.x - Pw.x)/2.;
				xat = (Pn.x - Ps.x)/2.;

				yks = (Pe.y - Pw.y)/2.;
				yat = (Pn.y - Ps.y)/2.;
				arfa = xat*xat + yat*yat;
				gama = xks*xks + yks*yks;
				K1 = sqrt(gama/arfa);

				Pe = m_pNodes->GetElement(j,i+2);
				Ps = m_pNodes->GetElement(j-1,i+1);
				Pw = m_pNodes->GetElement(j,i);
				Pn = m_pNodes->GetElement(j+1,i+1);
				xks = (Pe.x - Pw.x)/2.;
				xat = (Pn.x - Ps.x)/2.;

				yks = (Pe.y - Pw.y)/2.;
				yat = (Pn.y - Ps.y)/2.;
				arfa = xat*xat + yat*yat;
				gama = xks*xks + yks*yks;
				K2 = sqrt(gama/arfa);

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

				f = - log(K1/K2)/gama;
				pP->SetElement(f,j,i);
			}
		}
	}

	void  SmtBAOrthGrid::CalQ0(Matrix2D<double> *pQ)
	{
		int i ,j;
		dbfPoint Pw,Ps,Pe,Pn;
		dbfPoint P0,P1,P2;

		double xks,xat,yks,yat,xksat,yksat,arfa,gama;
		double K1,K2;
		double f;

		//i = 1
		for (j = 1; j < m_nY-1; j++)
		{
			P0 = m_pNodes->GetElement(j,0);
			P1 = m_pNodes->GetElement(j,1);
			P2 = m_pNodes->GetElement(j,2);
			Pn = m_pNodes->GetElement(j+1,1);
			Ps = m_pNodes->GetElement(j-1,1);
			xks = (-3*P0.x + 4*P1.x - P2.x)/2;		
			yks = (-3*P0.y + 4*P1.y - P2.y)/2;
			xat = (Pe.x - Pw.x)/2.;
			yat = (Pe.y - Pw.y)/2.;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K1 =sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(j,3);
			Ps = m_pNodes->GetElement(j-1,2);
			Pw = m_pNodes->GetElement(j,1);
			Pn = m_pNodes->GetElement(j+1,2);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K2 = sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(j,2);
			Ps = m_pNodes->GetElement(j-1,1);
			Pw = m_pNodes->GetElement(j,0);
			Pn = m_pNodes->GetElement(j+1,1);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;

			f = log(K1/K2)/arfa;
			pQ->SetElement(f,j,1);
		}
		//i = m_nX -2
		for (j = 1; j < m_nY-1; j++)
		{
			P0 = m_pNodes->GetElement(j,m_nX-1);
			P1 = m_pNodes->GetElement(j,m_nX-2);
			P2 = m_pNodes->GetElement(j,m_nX-3);
			Pn = m_pNodes->GetElement(j+1,m_nX-1);
			Ps = m_pNodes->GetElement(j-1,m_nX-1);
			xks = (-3*P0.x + 4*P1.x - P2.x)/2;		
			yks = (-3*P0.y + 4*P1.y - P2.y)/2;
			xat = (Pe.x - Pw.x)/2.;
			yat = (Pe.y - Pw.y)/2.;

			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K1 =sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(j,m_nX-2);
			Ps = m_pNodes->GetElement(j-1,m_nX-3);
			Pw = m_pNodes->GetElement(j,m_nX-4);
			Pn = m_pNodes->GetElement(j+1,m_nX-3);
			xks = (Pe.x - Pw.x)/2.;
			xat = (Pn.x - Ps.x)/2.;

			yks = (Pe.y - Pw.y)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			K2 = sqrt(gama/arfa);

			Pe = m_pNodes->GetElement(j,m_nX-1);
			Ps = m_pNodes->GetElement(j-1,m_nX-2);
			Pw = m_pNodes->GetElement(j,m_nX-3);
			Pn = m_pNodes->GetElement(j+1,m_nX-2);
			xks = (Pe.x - Pw.x)/2.;
			yks = (Pe.y - Pw.y)/2.;
			xat = (Pn.x - Ps.x)/2.;
			yat = (Pn.y - Ps.y)/2.;
			arfa = xat*xat + yat*yat;
			gama = xks*xks + yks*yks;
			f = log(K1/K2)/arfa;
			pQ->SetElement(f,j,m_nX-2);
		}
		for (i = 2; i < m_nX -2 ; i++)
		{
			for (j = 2; j < m_nY -2;j++)	
			{			
				Pe = m_pNodes->GetElement(j-1,i+1);
				Ps = m_pNodes->GetElement(j-2,i);
				Pw = m_pNodes->GetElement(j-1,i-1);
				Pn = m_pNodes->GetElement(j,i);
				xks = (Pe.x - Pw.x)/2.;
				xat = (Pn.x - Ps.x)/2.;

				yks = (Pe.y - Pw.y)/2.;
				yat = (Pn.y - Ps.y)/2.;
				arfa = xat*xat + yat*yat;
				gama = xks*xks + yks*yks;
				K1 = sqrt(gama/arfa);

				Pe = m_pNodes->GetElement(j+1,i+1);
				Ps = m_pNodes->GetElement(j,i);
				Pw = m_pNodes->GetElement(j+1,i-1);
				Pn = m_pNodes->GetElement(j+2,i);
				xks = (Pe.x - Pw.x)/2.;
				xat = (Pn.x - Ps.x)/2.;

				yks = (Pe.y - Pw.y)/2.;
				yat = (Pn.y - Ps.y)/2.;
				arfa = xat*xat + yat*yat;
				gama = xks*xks + yks*yks;
				K2 = sqrt(gama/arfa);

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

				f = log(K1/K2)/gama;
				pQ->SetElement(f,j,i);
			}
		}
	}
}
