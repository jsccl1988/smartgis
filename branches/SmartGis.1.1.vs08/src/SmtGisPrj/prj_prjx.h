/*
File:    prj_prjx.h

Desc:    PrjX,投影基类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _PRJ_PRJX_H
#define _PRJ_PRJX_H

#include "smt_core.h"
#include <math.h>

namespace Smt_Prj
{

	const   double   PRJ_PI    =   3.14159265359; 
	const   double   PRJ_rou0  =   180./PRJ_PI;
	const   double   PRJ_1_rou0 =   PRJ_PI/180.;

	enum EarthEllipsoidPra
	{
		IUGG1975,
		Krasovsky
	};

	class PrjX
	{
	public:
		PrjX(EarthEllipsoidPra eep)
		{
			switch(eep)
			{
			case IUGG1975:
				{
					a  = 6378140; 
					b  = 6356755.2882;

					e  = sqrt((a*a-b*b)/(a*a));
					f  = (a-b)/a;
					L0 = 0;
				}
				break;
			case Krasovsky:
				{
					a  = 6378245.0;
					b  = 6356863.0187730473;

					e  = sqrt((a*a-b*b)/(a*a));
					f  = (a-b)/a;
					L0 = 0;
				}
				break;
			default:
				break;
			}

			lScaleruler = 1;
		}

		virtual ~PrjX() 
		{

		}

	public:
		virtual void			SetPrjPra(double thetan  ,double thetas ,double namtaE  , double namtaW ) = 0;
        virtual void			SetScaleRuler(long lscale) {lScaleruler = lscale;}
		virtual long			GetScaleRuler(void) {return lScaleruler;}

		virtual void			PrjLB2XY(double theta ,double namta , double &x , double &y ) = 0;
		virtual void			PrjXY2LB( double x , double y ,double &theta ,double &namta ) = 0;

	protected:
		double					a,b,e,f;       //   基本椭球参数 
		double					A1,A2,A3,A4;   //   用于计算X的椭球参数 
		double					L0;            //   中央子午线经度
		long					lScaleruler;
	};
}

#if !defined(Export_SmtGisPrj)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisPrjD.lib")
#       else
#          pragma comment(lib,"SmtGisPrj.lib")
#	    endif  
#endif

#endif //_PRJ_PRJX_H