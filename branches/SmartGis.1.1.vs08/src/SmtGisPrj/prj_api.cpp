#include "prj_api.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
using namespace Smt_Core;

#ifdef USE_PROJ4
//#pragma comment(lib,"proj.lib")
#pragma comment(lib,"proj_i.lib")
#endif

namespace Smt_Prj
{
	/************************************************************************/
	/*                           SmtProjectPoint()                           */
	/************************************************************************/
	int SmtProjectPoint(SmtProjection *in, SmtProjection *out, dbfPoint *point)
	{
#ifdef USE_PROJ4
		projUV p;
		int	 error;

		if( in && in->gt.need_geotransform )
		{
			double x_out, y_out;

			x_out = in->gt.geotransform[0]
			+ in->gt.geotransform[1] * point->x 
				+ in->gt.geotransform[2] * point->y;
			y_out = in->gt.geotransform[3]
			+ in->gt.geotransform[4] * point->x 
				+ in->gt.geotransform[5] * point->y;

			point->x = x_out;
			point->y = y_out;
		}

		/* -------------------------------------------------------------------- */
		/*      If the source and destination are simple and equal, then do     */
		/*      nothing.                                                        */
		/* -------------------------------------------------------------------- */
		if( in && in->numargs == 1 && out && out->numargs == 1
			&& strcmp(in->args[0],out->args[0]) == 0 )
		{
			/* do nothing, no transformation required */
		}

		/* -------------------------------------------------------------------- */
		/*      If we have a fully defined input coordinate system and          */
		/*      output coordinate system, then we will use pj_transform.        */
		/* -------------------------------------------------------------------- */
		else if( in && in->proj && out && out->proj )
		{
			double	z = 0.0;

			if( pj_is_latlong(in->proj) )
			{
				point->x *= DEG_TO_RAD;
				point->y *= DEG_TO_RAD;
			}

			error = pj_transform( in->proj, out->proj, 1, 0, 
				&(point->x), &(point->y), &z );

			if( error || point->x == HUGE_VAL || point->y == HUGE_VAL )
				return SMT_ERR_FAILURE;

			if( pj_is_latlong(out->proj) )
			{
				point->x *= RAD_TO_DEG;
				point->y *= RAD_TO_DEG;
			}
		}

		/* -------------------------------------------------------------------- */
		/*      Otherwise we fallback to using pj_fwd() or pj_inv() and         */
		/*      assuming that the NULL SmtProjection is supposed to be          */
		/*      lat/long in the same datum as the other SmtProjection.  This    */
		/*      is essentially a backwards compatibility mode.                  */
		/* -------------------------------------------------------------------- */
		else
		{
			/* nothing to do if the other coordinate system is also lat/long */
			if( in == NULL && out != NULL && pj_is_latlong(out->proj) )
				return SMT_ERR_NONE;
			if( out == NULL && in != NULL && pj_is_latlong(in->proj) )
				return SMT_ERR_NONE;

			p.u = point->x;
			p.v = point->y;

			if(in==NULL || in->proj==NULL) { /* input coordinates are lat/lon */
				p.u *= DEG_TO_RAD; /* convert to radians */
				p.v *= DEG_TO_RAD;  
				p = pj_fwd(p, out->proj);
			} else {
				if(out==NULL || out->proj==NULL) { /* output coordinates are lat/lon */
					p = pj_inv(p, in->proj);
					p.u *= RAD_TO_DEG; /* convert to decimal degrees */
					p.v *= RAD_TO_DEG;
				} else { /* need to go from one projection to another */
					p = pj_inv(p, in->proj);
					p = pj_fwd(p, out->proj);
				}
			}

			if( p.u == HUGE_VAL || p.v == HUGE_VAL )
				return SMT_ERR_FAILURE;

			point->x = p.u;
			point->y = p.v;
		}

		if( out && out->gt.need_geotransform )
		{
			double x_out, y_out;

			x_out = out->gt.invgeotransform[0]
			+ out->gt.invgeotransform[1] * point->x 
				+ out->gt.invgeotransform[2] * point->y;
			y_out = out->gt.invgeotransform[3]
			+ out->gt.invgeotransform[4] * point->x 
				+ out->gt.invgeotransform[5] * point->y;

			point->x = x_out;
			point->y = y_out;
		}

		return(SMT_ERR_NONE);
#else
		return(SMT_ERR_FAILURE);
#endif
	}

	/************************************************************************/
	/*                         SmtProjectGrowRect()                          */
	/************************************************************************/
#ifdef USE_PROJ4
	static void SmtProjectGrowRect(SmtProjection *in, SmtProjection *out, 
		dbfRect *prj_rect, int *rect_initialized, 
		dbfPoint *prj_point, int *failure )

	{
		if( SmtProjectPoint(in, out, prj_point) == SMT_ERR_NONE )
		{
			if( *rect_initialized )
			{
				prj_rect->lb.y = min(prj_rect->lb.y, prj_point->y);
				prj_rect->rt.y = max(prj_rect->rt.y, prj_point->y);
				prj_rect->lb.x = min(prj_rect->lb.x, prj_point->x);
				prj_rect->rt.x = max(prj_rect->rt.x, prj_point->x);
			}
			else
			{
				prj_rect->lb.x = prj_rect->rt.x = prj_point->x;
				prj_rect->lb.y = prj_rect->rt.y = prj_point->y;
				*rect_initialized = 1;
			}
		}
		else
			(*failure)++;
	}
#endif /* def USE_PROJ4 */

	/************************************************************************/
	/*                          SmtProjectSegment()                          */
	/*                                                                      */
	/*      Interpolate along a line segment for which one end              */
	/*      reprojects and the other end does not.  Finds the horizon.      */
	/************************************************************************/

	static int SmtProjectSegment( SmtProjection *in, SmtProjection *out, 
		dbfPoint *start, dbfPoint *end )

	{
		dbfPoint testPoint, subStart, subEnd;

		/* -------------------------------------------------------------------- */
		/*      Without loss of generality we assume the first point            */
		/*      reprojects, and the second doesn't.  If that is not the case    */
		/*      then re-call with the points reversed.                          */
		/* -------------------------------------------------------------------- */
		testPoint = *start;
		if( SmtProjectPoint( in, out, &testPoint ) == SMT_ERR_FAILURE )
		{
			testPoint = *end;
			if( SmtProjectPoint( in, out, &testPoint ) == SMT_ERR_FAILURE )
				return SMT_ERR_FAILURE;
			else
				return SmtProjectSegment( in, out, end, start );
		}

		/* -------------------------------------------------------------------- */
		/*      We will apply a binary search till we are within out            */
		/*      tolerance.                                                      */
		/* -------------------------------------------------------------------- */
		subStart = *start;
		subEnd = *end;

#define TOLERANCE 0.01

		while( fabs(subStart.x - subEnd.x) 
			+ fabs(subStart.y - subEnd.y) > TOLERANCE )
		{
			dbfPoint midPoint;

			midPoint.x = (subStart.x + subEnd.x) * 0.5;
			midPoint.y = (subStart.y + subEnd.y) * 0.5;

			testPoint = midPoint;

			if( SmtProjectPoint( in, out, &testPoint ) == SMT_ERR_FAILURE )
				subEnd = midPoint;
			else
				subStart = midPoint;
		}

		/* -------------------------------------------------------------------- */
		/*      Now reproject the end points and return.                        */
		/* -------------------------------------------------------------------- */
		*end = subStart;

		if( SmtProjectPoint( in, out, end ) == SMT_ERR_FAILURE
			|| SmtProjectPoint( in, out, start ) == SMT_ERR_FAILURE )
			return SMT_ERR_FAILURE;
		else
			return SMT_ERR_NONE;
	}

	/************************************************************************/
	/*                           SmtProjectRectGrid()                        */
	/************************************************************************/

#define NUMBER_OF_SAMPLE_POINTS 100

	int SmtProjectRectGrid(SmtProjection *in, SmtProjection *out, dbfRect *rect) 
	{
#ifdef USE_PROJ4
		dbfPoint prj_point;
		dbfRect prj_rect;
		int	  rect_initialized = 0, failure=0;
		int     ix, iy;

		double dx, dy;
		double x, y;

		dx = (rect->rt.x - rect->lb.x)/NUMBER_OF_SAMPLE_POINTS;
		dy = (rect->rt.y - rect->lb.y)/NUMBER_OF_SAMPLE_POINTS;

		/* first ensure the top left corner is processed, even if the rect
		turns out to be degenerate. */

		prj_point.x = rect->lb.x;
		prj_point.y = rect->lb.y;


		SmtProjectGrowRect(in,out,&prj_rect,&rect_initialized,&prj_point,&failure);

		failure = 0;
		for(ix = 0; ix <= NUMBER_OF_SAMPLE_POINTS; ix++ )
		{
			x = rect->lb.x + ix * dx;

			for(iy = 0; iy <= NUMBER_OF_SAMPLE_POINTS; iy++ )
			{
				y = rect->lb.y + iy * dy;

				prj_point.x = x;
				prj_point.y = y;
				SmtProjectGrowRect(in,out,&prj_rect,&rect_initialized,&prj_point,
					&failure);
			}
		}

		if( !rect_initialized )
		{
			if( out == NULL || out->proj == NULL 
				|| pj_is_latlong(in->proj) )
			{
				prj_rect.lb.x = -180;
				prj_rect.rt.x = 180;
				prj_rect.lb.y = -90;
				prj_rect.rt.y = 90;
			}
			else
			{
				prj_rect.lb.x = -22000000;
				prj_rect.rt.x = 22000000;
				prj_rect.lb.y = -11000000;
				prj_rect.rt.y = 11000000;
			}
		}
		else
		{
			;
		}

		rect->lb.x = prj_rect.lb.x;
		rect->lb.y = prj_rect.lb.y;
		rect->rt.x = prj_rect.rt.x;
		rect->rt.y = prj_rect.rt.y;

		if( !rect_initialized )
			return SMT_ERR_FAILURE;
		else
			return(SMT_ERR_NONE);
#else
		return(SMT_ERR_FAILURE);
#endif
	}

	/************************************************************************/
	/*                           SmtProjectRect()                            */
	/************************************************************************/

	int SmtProjectRect(SmtProjection *in, SmtProjection *out, dbfRect *rect) 
	{
		return SmtProjectPoint(in,out,&(rect->lb)) && SmtProjectPoint(in,out,&(rect->rt));   
	}

	/************************************************************************/
	/*                        SmtProjectionsDiffer()                         */
	/************************************************************************/

	/*
	** Compare two projections, and return 1 if they differ. 
	**
	** For now this is implemented by exact comparison of the projection
	** arguments, but eventually this should call a PROJ.4 function with
	** more awareness of the issues.
	**
	** NOTE: 0 is returned if either of the projection objects
	** has no arguments, since reprojection won't work anyways.
	*/

	int SmtProjectionsDiffer( SmtProjection *proj1, SmtProjection *proj2 )

	{
		int		i;

		if( proj1->numargs == 0 || proj2->numargs == 0 )
			return 0;

		if( proj1->numargs != proj2->numargs )
			return 1;

		/* This test should be more rigerous. */
		if( proj1->gt.need_geotransform 
			|| proj2->gt.need_geotransform )
			return 1;

		for( i = 0; i < proj1->numargs; i++ )
		{
			if( strcmp(proj1->args[i],proj2->args[i]) != 0 )
				return 1;
		}

		return 0;
	}

	/************************************************************************/
	/*                           SmtTestNeedWrap()                           */
	/************************************************************************/
	/*

	Frank Warmerdam, Nov, 2001. 

	See Also: 

	http://mapserver.gis.umn.edu/bugs/show_bug.cgi?id=15

	Proposal:

	Modify SmtProjectLine() so that it "dateline wraps" objects when necessary
	in order to preserve their shape when reprojecting to lat/long.  This
	will be accomplished by:

	1) As each vertex is reprojected, compare the X distance between that 
	vertex and the previous vertex.  If it is less than 180 then proceed to
	the next vertex without any special logic, otherwise:

	2) Reproject the center point of the line segment from the last vertex to
	the current vertex into lat/long.  Does it's longitude lie between the
	longitudes of the start and end point.  If yes, return to step 1) for
	the next vertex ... everything is fine. 

	3) We have determined that this line segment is suffering from 360 degree
	wrap to keep in the -180 to +180 range.  Now add or subtract 360 degrees
	as determined from the original sign of the distances.  

	This is similar to the code there now (though disabled in CVS); however, 
	it will ensure that big boxes will remain big, and not get dateline wrapped
	because of the extra test in step 2).  However step 2 is invoked only very
	rarely so this process takes little more than the normal process.  In fact, 
	if we were sufficiently concerned about performance we could do a test on the
	shape MBR in lat/long space, and if the width is less than 180 we know we never
	need to even do test 1). 

	What doesn't this resolve:

	This ensures that individual lines are kept in the proper shape when 
	reprojected to geographic space.  However, it does not:

	o Ensure that all rings of a polygon will get transformed to the same "side"
	of the world.  Depending on starting points of the different rings it is
	entirely possible for one ring to end up in the -180 area and another ring
	from the same polygon to end up in the +180 area.  We might possibly be
	able to achieve this though, by treating the multi-ring polygon as a whole
	and testing the first point of each additional ring against the last
	vertex of the previous ring (or any previous vertex for that matter).

	o It does not address the need to cut up lines and polygons into distinct
	chunks to preserve the correct semantics.  Really a polygon that 
	spaces the dateline in a -180 to 180 view should get split into two 
	polygons.  We haven't addressed that, though if it were to be addressed,
	it could be done as a followon and distinct step from what we are doing
	above.  In fact this sort of improvement (split polygons based on dateline
	or view window) should be done for all lat/long shapes regardless of 
	whether they are being reprojected from another projection. 

	o It does not address issues related to viewing rectangles that go outside
	the -180 to 180 longitude range.  For instance, it is entirely reasonable
	to want a 160 to 200 longitude view to see an area on the dateline clearly.
	Currently shapes in the -180 to -160 range which should be displayed in the
	180 to 200 portion of that view will not be because there is no recogition
	that they belong there. 


	*/

#ifdef USE_PROJ4
	static int SmtTestNeedWrap( dbfPoint pt1, dbfPoint pt2, dbfPoint pt2_geo,
		SmtProjection *in, 
		SmtProjection *out )

	{
		dbfPoint	middle;

		middle.x = (pt1.x + pt2.x) * 0.5;
		middle.y = (pt1.y + pt2.y) * 0.5;

		if( SmtProjectPoint( in, out, &pt1 ) == SMT_ERR_FAILURE 
			|| SmtProjectPoint( in, out, &pt2 ) == SMT_ERR_FAILURE
			|| SmtProjectPoint( in, out, &middle ) == SMT_ERR_FAILURE )
			return 0;

		/* 
		* If the last point was moved, then we are considered due for a
		* move to.
		*/
		if( fabs(pt2_geo.x-pt2.x) > 180.0 )
			return 1;

		/*
		* Otherwise, test to see if the middle point transforSmt
		* to be between the end points. If yes, no wrapping is needed.
		* Otherwise wrapping is needed.
		*/
		if( (middle.x < pt1.x && middle.x < pt2_geo.x)
			|| (middle.x > pt1.x && middle.x > pt2_geo.x) )
			return 1;
		else
			return 0;
	}
#endif /* def USE_PROJ4 */

	/************************************************************************/
	/*                            SmtProjFinder()                            */
	/************************************************************************/
#ifdef USE_PROJ4
	static char *Smt_proj_lib = NULL;
	static char *last_filename = NULL;

	static const char *SmtProjFinder( const char *filename)
	{
		if( last_filename != NULL )
			free( last_filename );

		if( filename == NULL )
			return NULL;

		if( Smt_proj_lib == NULL )
			return filename;

		last_filename = (char *) malloc(strlen(filename)+strlen(Smt_proj_lib)+2);
		sprintf( last_filename, "%s/%s", Smt_proj_lib, filename );

		return last_filename;
	}
#endif /* def USE_PROJ4 */

	/************************************************************************/
	/*                           SmtSetPROJ_LIB()                            */
	/************************************************************************/
	void SmtSetPROJ_LIB( const char *proj_lib, const char *pszRelToPath )

	{
#ifdef USE_PROJ4
		static int finder_installed = 0;
		char *extended_path = NULL;

		/* Handle relative path if applicable */
		if( proj_lib && pszRelToPath
			&& proj_lib[0] != '/'
			&& proj_lib[0] != '\\'
			&& !(proj_lib[0] != '\0' && proj_lib[1] == ':') )
		{
			struct stat stat_buf;
			extended_path = (char*) malloc(strlen(pszRelToPath)
				+ strlen(proj_lib) + 10);
			sprintf( extended_path, "%s/%s", pszRelToPath, proj_lib );

#ifndef S_ISDIR
#  define S_ISDIR(x) ((x) & S_IFDIR)
#endif            

			if( stat( extended_path, &stat_buf ) == 0 
				&& S_ISDIR(stat_buf.st_mode) )
				proj_lib = extended_path;
		}

		if( finder_installed == 0 && proj_lib != NULL)
		{
			finder_installed = 1;
			pj_set_finder( SmtProjFinder );
		}

		if (proj_lib == NULL) pj_set_finder(NULL);

		if( Smt_proj_lib != NULL )
		{
			free( Smt_proj_lib );
			Smt_proj_lib = NULL;
		}

		if( last_filename != NULL )
		{
			free( last_filename );
			last_filename = NULL;
		}

		if( proj_lib != NULL )
			Smt_proj_lib = strdup( proj_lib );

		if ( extended_path )
			free( extended_path );
#endif
	}

	/************************************************************************/
	/*                       SmtGetProjectionString()                        */
	/*                                                                      */
	/*      Return the projection string.                                   */
	/************************************************************************/

	char *SmtGetProjectionString(SmtProjection *proj)
	{
		char        *pszProjString = NULL;
		int         i = 0, nLen = 0;

		if (proj)
		{
			/* -------------------------------------------------------------------- */
			/*      Alloc buffer large enough to hold the whole projection defn     */
			/* -------------------------------------------------------------------- */
			for (i=0; i<proj->numargs; i++)
			{
				if (proj->args[i])
					nLen += (strlen(proj->args[i]) + 2);
			}

			pszProjString = (char*)malloc(sizeof(char) * nLen+1);
			pszProjString[0] = '\0';

			/* -------------------------------------------------------------------- */
			/*      Plug each arg into the string with a '+' prefix                 */
			/* -------------------------------------------------------------------- */
			for (i=0; i<proj->numargs; i++)
			{
				if (!proj->args[i] || strlen(proj->args[i]) == 0)
					continue;
				if (pszProjString[0] == '\0')
				{
					/* no space at beginning of line */
					if (proj->args[i][0] != '+')
						strcat(pszProjString, "+");
				}
				else
				{
					if (proj->args[i][0] != '+')
						strcat(pszProjString, " +");
					else
						strcat(pszProjString, " ");
				}
				strcat(pszProjString, proj->args[i]);
			}
		}

		return pszProjString;
	}

	/************************************************************************/
	/*                       SmtAxisNormalizePoints()                        */
	/*                                                                      */
	/*      Convert the passed points to "easting, northing" axis           */
	/*      orientation if they are not already.                            */
	/************************************************************************/

	void SmtAxisNormalizePoints( SmtProjection *proj, int count, 
		double *x, double *y )

	{
		int i;
		const char *axis = NULL;

		for( i = 0; i < proj->numargs; i++ )
		{
			if( strstr(proj->args[i],"epsgaxis=") != NULL )
			{
				axis = strstr(proj->args[i],"=") + 1;
				break;
			}
		}

		if( axis == NULL )
			return;

		if( stricmp(axis,"en") == 0 )
			return;

		if( stricmp(axis,"ne") != 0 )
		{
			return;
		}

		/* Switch axes */
		for( i = 0; i < count; i++ )
		{
			double tmp;

			tmp = x[i];
			x[i] = y[i];
			y[i] = tmp;
		}
	}

	/************************************************************************/
	/*                      SmtAxisDenormalizePoints()                       */
	/*                                                                      */
	/*      Convert points from easting,northing orientation to the         */
	/*      preferred epsg orientation of this SmtProjection.               */
	/************************************************************************/

	void SmtAxisDenormalizePoints( SmtProjection *proj, int count, 
		double *x, double *y )

	{
		/* For how this is essentially identical to normalizing */
		SmtAxisNormalizePoints( proj, count, x, y );
	}

}