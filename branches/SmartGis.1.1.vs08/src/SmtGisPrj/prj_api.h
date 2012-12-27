/*
File:    prj_api.h

Desc:   

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.5.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _PRJ_API_H
#define _PRJ_API_H

#define  USE_PROJ4

#include "smt_core.h"
#include "smt_bas_struct.h"

#ifdef USE_PROJ4
#  include "proj_api.h"
#endif

namespace Smt_Prj
{
	struct SmtGeoTransform
	{
		int			need_geotransform;
		double		rotation_angle;  
		double		geotransform[6];    /* Pixel/line to georef. */
		double		invgeotransform[6]; /* georef to pixel/line */
	} ;

	struct SmtProjection  
	{
		int			numargs; /* actual number of projection args */
		int			automatic; /* projection object was to fetched from the layer */ 
		char		**args; /* variable number of projection args */

#ifdef USE_PROJ4
		projPJ		proj; /* a projection structure for the PROJ package */
#else
		void		*proj;
#endif
		SmtGeoTransform gt; /* extra transformation to apply */
	};

	int SMT_EXPORT_API		SmtProjectPoint(SmtProjection *in, SmtProjection *out, Smt_Core::dbfPoint *point);
	int SMT_EXPORT_API		SmtProjectRect(SmtProjection *in, SmtProjection *out, Smt_Core::dbfRect *rect);
	int SMT_EXPORT_API		SmtProjectionsDiffer(SmtProjection *, SmtProjection *);
	
	void SMT_EXPORT_API		SmtFreeProjection(SmtProjection *p);
	int SMT_EXPORT_API		SmtInitProjection(SmtProjection *p);
	int SMT_EXPORT_API		SmtProcessProjection(SmtProjection *p);
	int SMT_EXPORT_API		SmtLoadProjectionString(SmtProjection *p, const char *value);
	int SMT_EXPORT_API		SmtLoadProjectionStringEPSG(SmtProjection *p, const char *value);
	char SMT_EXPORT_API		*SmtGetProjectionString(SmtProjection *proj);
	void SMT_EXPORT_API		SmtAxisNormalizePoints( SmtProjection *proj, int count,double *x, double *y );
	void SMT_EXPORT_API		SmtAxisDenormalizePoints( SmtProjection *proj, int count,double *x, double *y );

	void SMT_EXPORT_API		SmtSetPROJ_LIB( const char *, const char * );

	/* Provides compatiblity with PROJ.4 4.4.2 */
#ifndef PJ_VERSION
#  define pj_is_latlong(x)	((x)->is_latlong)
#endif

}

#if !defined(Export_SmtGisPrj)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisPrjD.lib")
#       else
#          pragma comment(lib,"SmtGisPrj.lib")
#	    endif  
#endif

#endif //_PRJ_API_H
