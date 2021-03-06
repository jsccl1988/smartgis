#include "prj_gaussprj.h"

#include<math.h>

namespace Smt_Prj
{

	double _A=1.0050517739 ,_B=0.00506237764,_C=0.0000106245,_D=0.00000002081;

	GaussPrj::GaussPrj(EarthEllipsoidPra eep):PrjX(eep)
	{
		m_nZoneWide = 6;
		SetPrjPra(37,36,118,117);
	}


	GaussPrj::~GaussPrj()
	{

	}

	void GaussPrj::SetPrjPra(
		double thetan,double thetas,
		double namtaE, double namtaW
		)
	{

	}

/*
	void  GaussPrj::PrjLB2XY(double B ,double L , double &x , double &y )
	{//theta，namta为角度

		int ProjNo = (int)(L / m_nZoneWide) ; 
		L0 = ProjNo * m_nZoneWide + m_nZoneWide / 2; 
		//double FE = 500000+ProjNo* 1000000;

		double FE = 500000;


		double sinB = sin(PRJ_1_rou0*B);   
		double cosB = cos(PRJ_1_rou0*B);   
		double X = 111134.0047*B-(32009.8575*sinB+133.9602*pow(sinB,3)+0.6976*pow(sinB,5)+0.0039*pow(sinB,7))*cosB;   
		double t = tan(PRJ_1_rou0*B);   
		double l = L - L0;   
		double m = PRJ_1_rou0*l*cosB;   
		double nn = e*e*cosB*cosB;   
		double N = a/sqrt(1+nn);   
		x = X + N*t*(0.5*pow(m,2))+(double)1/24*(5-t*t+4*nn*nn)*pow(m,4) +(double)1/720*(61-58*t*t+pow(t,4)*pow(m,6));   
		y = FE + N*(m+(double)1/6*(1-t*t+nn)*pow(m,3)+(double)1/120*(5-18*t*t+pow(t,4)+14*nn-58*nn*t*t)*pow(m,5));   

		//double S = 0.,N = 0.,n2 = 0.;
		//L -= L0;

		//B *= PRJ_1_rou0;
		//L *= PRJ_1_rou0;

		//double cosB = cos(B) ,sinB = sin(B),tanB = tan(B);
		//double cosB2 = cosB*cosB;
		//double tanB2 = tanB*tanB;
		//double L2 = L*L;

		//S = a*(1-e*e)*(_A/rou0*B-0.5*_B*sin(2*B) + 0.25*_C*sin(4*B)- _D*sin(6*B)/6.);
		//N = a/sqrt((1-e*e*sinB*sinB));
		//n2 = ee*ee*cosB2;

		//x = S+(L2*N)*sinB*cosB/2. + (L2*L2*N)*sinB*cosB2*cosB*(5-tanB2+9*n2+4*n2*n2)/24.;
		//y = L*N*cosB + (L2*L*N)*cosB2*cosB*(1-tanB2+n2)/6. + (L2*L2*L*N)*cosB2*cosB2*cosB*(5-18*tanB2 + tanB2*tanB2)/120.;

		x /= lScaleruler;
		y /= lScaleruler;
	}
*/
	void  GaussPrj::PrjLB2XY(double B ,double L , double &x , double &y )
	{//theta，namta为角度

		int ProjNo = (int)(L / m_nZoneWide) ; 
		L0 = ProjNo * m_nZoneWide + m_nZoneWide / 2; 
	//	double FE = 500000+ProjNo*1000000;
		double FE = 500000;

		L -= L0;
		B *= PRJ_1_rou0;
		L *= PRJ_1_rou0;

		double sinB = sin(B);   
		double cosB = cos(B);   

		double S = 0.,N = 0.,t = 0.,ee = 0.,eta2 = 0.;

		N    = a/sqrt((1-e*e*sinB*sinB));
		t    = tan(B);
		ee   = sqrt((a*a-b*b)/(b*b));
		eta2 = ee*ee*cosB*cosB;
		S = 6367558.496875*B - 16036.4802694*sin(2*B)+ 16.828066885*sin(4*B) - 0.02197531*sin(6*B) + 0.0000311311*sin(8*B);

		x = S + N*t*cosB*cosB*L*L*( 1/2. + (5-t*t+9*eta2+4*eta2*eta2)*pow(cosB,2)*pow(L,2)/24. + (61+58*t-pow(t,4)+270*eta2-330*t*t*eta2)*pow(cosB,4)*pow(L,4)/720.);
		y = FE + N*cosB*L + (1-t*t+eta2)*pow(cosB,3)*pow(L,3)/6. +  (5-18*t*t+pow(t,4)+14*eta2-58*t*t*eta2)*pow(cosB,5)*pow(L,5);

		x /= lScaleruler;
		y /= lScaleruler;
	}

	void  GaussPrj::PrjXY2LB( double x , double y ,double &theta ,double &namta )
	{
		double X = x/1000000;   
		double X3 = X-3;   
		double Bf = 27.11162289465+9.02483657729*X3-0.00579850656*X3*X3-0.00043540029*X3*X3*X3 +0.00004858357*X3*X3*X3*X3+0.00000215769*X3*X3*X3*X3*X3-0.00000019404*X3*X3*X3*X3*X3*X3;   
		double tf = tan(PRJ_1_rou0*Bf);   
		double cosBf = cos(PRJ_1_rou0*Bf);   
		double nf = e*e*cosBf*cosBf;   
		double n = y*sqrt(1+nf)/a;   
		double l = (double)1/(PRJ_PI*cosBf)*(180*n-30*(1+2*tf*tf+nf)*n*n*n+1.5*(5+28*tf*tf+24*tf*tf*tf*tf)*n*n*n*n*n);   
		double L = L0 +l;   
		double B = Bf -(1+nf)/PRJ_PI*tf*(90*n*n-7.5*(5+3*tf*tf+Bf-9*nf*tf*tf)*n*n*n*n+0.25*(61+90*tf*tf+45*tf*tf*tf*tf)*n*n*n*n*n*n);   
		theta = B*PRJ_rou0;   
		namta = L*PRJ_rou0;   

	}

}