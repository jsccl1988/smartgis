
#include "prj_lambertprj.h"



namespace Smt_Prj
{

	LambertPrj::LambertPrj(EarthEllipsoidPra eep):PrjX(eep)
	{
		SetPrjPra(41.5,37.5,120,112);
	}


	LambertPrj::~LambertPrj()
	{

	}

	void LambertPrj::SetPrjPra(
		double thetan,double thetas,
		double namtaE, double namtaW
		)
	{
		double t1,t2;

		theta1 = thetas + 30/60.0;
		theta2 = thetan - 30/60.0;

		theta1=theta1*PRJ_1_rou0;
		theta2=theta2*PRJ_1_rou0;

		pofa1 = asin(e*sin(theta1));
		pofa2= asin(e*sin(theta2));

		t1=(1-e*e*sin(theta1)*sin(theta1));
		t2=(1-e*e*sin(theta2)*sin(theta2));

		M1 = a*(1-e*e)/(pow(t1,3/2.0));
		M2 = a*(1-e*e)/(pow(t2,3/2.0));

		N1 = a/pow(t1,1/2.0);
		N2 = a/pow(t2,1/2.0);

		r1 = N1*cos(theta1);
		r2 = N2*cos(theta2);

		U1 = tan(45*PRJ_1_rou0+ theta1/2)/pow(tan(45*PRJ_1_rou0 + pofa1/2 ),e);
		U2 = tan(45*PRJ_1_rou0+ theta2/2)/pow(tan(45*PRJ_1_rou0 + pofa2/2 ),e);

		arfa = (log10(r2)-log10(r1))/(log10(U1) - log10(U2));

		theta0 = asin(arfa);

		K = r1*pow(U1,arfa)/arfa;

		rous =  CalRou((thetas-1)*PRJ_1_rou0);

		L0 = (namtaE+namtaW)/2.;
	}

	double LambertPrj::CalU(double theta ,double pofa )
	{//theta，pofa为弧度
		return tan(45*PRJ_1_rou0+ theta/2)/pow(tan(45*PRJ_1_rou0+ pofa/2 ),e);
	}

	double LambertPrj::CalPofa(double theta)
	{//theta为弧度
		return asin(e*sin(theta));
	}

	double LambertPrj::CalRou(double theta )
	{//theta为弧度
		double U,pofa;
		pofa = CalPofa(theta);
		U = CalU(theta,pofa);
		return K/pow(U,arfa);
	}

	void  LambertPrj::PrjLB2XY(double theta ,double namta , double &x , double &y )
	{//theta，namta为角度
		double deta, rou ;
		namta -=L0;
		deta = arfa * namta*PRJ_1_rou0;
		rou = CalRou(theta *PRJ_1_rou0);
		x = (rous - rou * cos (deta));
		y = (rou * sin(deta));

		x /= lScaleruler;
		y /= lScaleruler;

	}

	void   LambertPrj::PrjXY2LB( double x , double y ,double &theta ,double &namta )
	{//theta，namta为角度

	}

}