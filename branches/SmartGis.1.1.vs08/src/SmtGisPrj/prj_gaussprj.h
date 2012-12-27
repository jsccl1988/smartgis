/*
File:    prj_guassprj.h

Desc:    GaussPrj,高斯克里格投影类

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _PRJ_GAUSSPRJ_H
#define _PRJ_GAUSSPRJ_H

#include "prj_prjx.h"

namespace Smt_Prj
{

	class SMT_EXPORT_CLASS GaussPrj:public PrjX
	{
	public:
		GaussPrj(EarthEllipsoidPra eep);
		virtual ~GaussPrj();

	public:
		virtual void			SetPrjPra(double thetan  ,double thetas ,double namtaE  , double namtaW );

		virtual void			PrjLB2XY(double B ,double L , double &x , double &y );
		virtual void			PrjXY2LB( double x , double y ,double &theta ,double &namta );

	private:
		int						m_nZoneWide;
	};

}

#if !defined(Export_SmtGisPrj)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisPrjD.lib")
#       else
#          pragma comment(lib,"SmtGisPrj.lib")
#	    endif  
#endif


#endif //_PRJ_GAUSSPRJ_H