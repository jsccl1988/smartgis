/*
File:    prj_lambertprj.h

Desc:    LambertPrj,À¼²®ÌØÍ¶Ó°Àà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _PRJ_LAMBERTPRJ_H
#define _PRJ_LAMBERTPRJ_H

#include "prj_prjx.h"

namespace Smt_Prj
{

	class SMT_EXPORT_CLASS LambertPrj:public PrjX
	{
	public:
		LambertPrj(EarthEllipsoidPra eep);
		virtual ~LambertPrj();

	public:
		virtual void			SetPrjPra(double thetan  ,double thetas,double namtaE, double namtaW );

		virtual void			PrjLB2XY(double theta ,double namta , double &x , double &y );
		virtual void			PrjXY2LB( double x , double y ,double &theta ,double &namta );

	private:
		void					CalPra(void);
		double					CalU(double theta ,double pofa );
		double					CalPofa(double theta);
		double					CalRou(double U);

	protected:
		double					U1,U2,arfa,K,theta0,namta,theta;
		double					M1,N1,M2,N2,pofa1,pofa2;              //
		double					r1,r2;                                //Î³È¦ÇúÂÊ°ë¾¶
		double					theta1,theta2;
		double					rous;
	};

}

#if !defined(Export_SmtGisPrj)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisPrjD.lib")
#       else
#          pragma comment(lib,"SmtGisPrj.lib")
#	    endif  
#endif


#endif //_PRJ_LAMBERTPRJ_H