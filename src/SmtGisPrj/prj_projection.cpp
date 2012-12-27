
#include "prj_projection.h"

using namespace Smt_Geo;

namespace Smt_Prj
{

	Projection::Projection(PrjX * pPrj):m_pPrj(pPrj) 
	{

	}

	Projection::~Projection() 
	{
		//SMT_SAFE_DELETE(m_pPrj);
	}

	void   Projection::SetPrjPra(
		float dtheta , float dnamta , 	
		double thetan  ,double thetas ,
		double namtaE  , double namtaW,
		long scalerule )
	{
		m_dNamta = dnamta;
		m_dTheta = dtheta;
		m_lScalerule = scalerule;
		m_thetaN = thetan;
		m_thetaS = thetas;
		m_namtaE = namtaE;
		m_namtaW = namtaW;

		if(m_pPrj) 
		{
			m_pPrj->SetPrjPra(thetan,thetas,namtaE,namtaW);
			m_pPrj->SetScaleRuler(scalerule);
		}
	}

	void   Projection::DoGridPrj(Smt_Geo::SmtGrid &smtGrid) 
	{
		int nN,nM;
		nM = (int)fabs((m_thetaN-m_thetaS)/m_dTheta) +1;
		nN = (int)fabs((m_namtaE-m_namtaW)/m_dNamta) +1;

		smtGrid.ReSize(nM,nN);

		int i,j;
		double x,y;

		Matrix2D<RawPoint>  *pNodes = smtGrid.GetGridNodeBuf();
		for(i = 0; i< nM ; i++ )
		{
			for (j = 0 ; j < nN ;j++ )
			{		
				m_pPrj->PrjLB2XY(i* m_dTheta + m_thetaS,j*m_dNamta + m_namtaW ,y,x);
				RawPoint newPoint(x,y);
				pNodes->SetElement(newPoint,i,j);
			}
		}
	}

	void   Projection::PrjLB2XY(double theta ,double namta , double &x , double &y ) 
	{
		if(m_pPrj) 
			m_pPrj->PrjLB2XY(theta ,namta ,x , y );
	}

	void   Projection::PrjXY2LB( double x , double y ,double &theta ,double &namta )
	{
		if(m_pPrj) 
			m_pPrj->PrjXY2LB(x*m_lScalerule ,y*m_lScalerule,theta ,namta);
	}

}