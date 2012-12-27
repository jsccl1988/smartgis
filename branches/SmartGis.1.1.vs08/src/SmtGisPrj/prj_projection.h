/*
File:    prj_prjx.h

Desc:    Projection,ͶӰ��wrap,ͶӰ����������ģʽ-�㷨����ģʽ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _PRJ_PROJECTION_H
#define _PRJ_PROJECTION_H

#include "prj_prjx.h"
#include "geo_geometry.h"

namespace Smt_Prj
{
	class SMT_EXPORT_CLASS Projection
	{
	public:
		Projection(PrjX * pPrj);
		virtual ~Projection();

	public:
		void					SetPrjPra(
								float dtheta ,float dnamta ,
								double thetaN  ,double thetaS ,
								double namtaE  , double namtaW ,
								long scalerule );

		PrjX *					GetPrjX(void) { return m_pPrj;}

		void					DoGridPrj(Smt_Geo::SmtGrid &smtGrid) ;

		void					PrjLB2XY(double theta ,double namta , double &x , double &y ) ;
		void					PrjXY2LB( double x , double y ,double &theta ,double &namta );

	protected:
		PrjX					*m_pPrj;

	protected:
		double					m_thetaN,m_thetaS; 
		double					m_dTheta,m_dNamta;
		double					m_namtaE,m_namtaW ;
		long					m_lScalerule;                          //��������
	};

}

#if !defined(Export_SmtGisPrj)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisPrjD.lib")
#       else
#          pragma comment(lib,"SmtGisPrj.lib")
#	    endif  
#endif

#endif //_PRJ_PROJECTION_H
