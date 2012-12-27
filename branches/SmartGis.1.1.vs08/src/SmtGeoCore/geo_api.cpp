#include "geo_api.h"
#include "ml_mathlib.h"

using namespace Smt_Math;
using namespace Smt_Core;
using namespace Smt_Base;

#define	SMT_IS_BETWEEN(a, x, b)	(((a) <= (x) && (x) <= (b)) || ((b) <= (x) && (x) <= (a)))

//////////////////////////////////////////////////////////////////////////
//SmtPointToPolygon_New1用到
//获取相交点
bool   SmtGetCrossing(dbfPoint &Crossing, const dbfPoint &a1, const dbfPoint &a2, const dbfPoint &b1, const dbfPoint &b2, bool bExactMatch = true);
//获取线段上离已知点最近的点
double SmtGetNearestPointOnLine(const dbfPoint &Point, const dbfPoint &Ln_A, const dbfPoint &Ln_B, dbfPoint &Ln_Point, bool bExactMatch = true);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
long    SmtGetEnvlope(Envelope &env,dbfPoint *pPoints,int nPoint)
{
	double      dfMinX, dfMinY, dfMaxX, dfMaxY;

	if(nPoint < 2 || NULL == pPoints) 
		return SMT_ERR_INVALID_PARAM;

	dfMinX = dfMaxX = pPoints[0].x;
	dfMinY = dfMaxY = pPoints[0].y;

	for( int iPoint = 1; iPoint < nPoint; iPoint++ )
	{
		if( dfMaxX < pPoints[iPoint].x )
			dfMaxX = pPoints[iPoint].x;
		if( dfMaxY < pPoints[iPoint].y )
			dfMaxY = pPoints[iPoint].y;
		if( dfMinX > pPoints[iPoint].x )
			dfMinX = pPoints[iPoint].x;
		if( dfMinY > pPoints[iPoint].y )
			dfMinY = pPoints[iPoint].y;
	}

	env.MinX = dfMinX;
	env.MaxX = dfMaxX;
	env.MinY = dfMinY;
	env.MaxY = dfMaxY;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
double SmtDistance(double x1,double y1,double x2, double y2)
{
	 return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}

double SmtDistance(dbfPoint &pt1,dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext)
{
	if(nPoint < 2 || NULL == pPoints) 
		return SMT_C_INVALID_DBF_VALUE;

	dbfPoint ptPre,ptNext,ptCur(pt1.x,pt1.y);
	float Mintheta = 1.0;
	for (int i = 0; i < nPoint - 1; i++)
	{
		ptPre = pPoints[i];
		ptNext = pPoints[i+1];

		float theta = 0.;
		theta = SmtCosABC(ptPre,ptCur,ptNext);  
		if ( theta < Mintheta)
		{
			indexPre = i;
			indexNext = i + 1;
			Mintheta = theta;
		}
	}

	ptPre = pPoints[nPoint - 1];
	ptNext = pPoints[0];

	if(ptPre.x == ptNext.x && ptPre.y == ptNext.y)
	{
		float theta = 0.;
		theta = SmtCosABC(ptPre,ptCur,ptNext);  
		if ( theta < Mintheta)
		{
			indexPre = nPoint - 1;
			indexNext = 0;
			Mintheta = theta;
		}
	}

	ptPre = pPoints[indexPre];
	ptNext = pPoints[indexNext];
	
	double  theta = SmtCosABC( ptPre,ptNext,ptCur),dbfDistance = 0.;
	float sinABC = sqrt(1-theta*theta);
	Vector vBC(ptCur.x - ptNext.x,ptCur.y - ptNext.y);
	dbfDistance = abs(sinABC * vBC.GetLength());

	return  dbfDistance;
}

double SmtDistance(dbfPoint &pt1,std::vector<dbfPoint> &vPoints,int &indexPre,int &indexNext)
{
	if(vPoints.size() < 2) 
		return SMT_C_INVALID_DBF_VALUE;

	dbfPoint ptPre,ptNext,ptCur(pt1.x,pt1.y);
	float Mintheta = 1.0;
	for (int i = 0; i < vPoints.size() - 1; i++)
	{
		ptPre = vPoints[i];
		ptNext = vPoints[i+1];

		float theta = 0.;
		theta = SmtCosABC(ptPre,ptCur,ptNext);  
		if ( theta < Mintheta)
		{
			indexPre = i;
			indexNext = i + 1;
			Mintheta = theta;
		}
	}

	ptPre = vPoints[vPoints.size() - 1];
	ptNext = vPoints[0];

	if(ptPre.x == ptNext.x && ptPre.y == ptNext.y)
	{
		float theta = 0.;
		theta = SmtCosABC(ptPre,ptCur,ptNext);  
		if ( theta < Mintheta)
		{
			indexPre = vPoints.size() - 1;
			indexNext = 0;
			Mintheta = theta;
		}
	}

	ptPre = vPoints[indexPre];
	ptNext = vPoints[indexNext];

	double  theta = SmtCosABC( ptPre,ptNext,ptCur),dbfDistance = 0.;
	float sinABC = sqrt(1-theta*theta);
	Vector vBC(ptCur.x - ptNext.x,ptCur.y - ptNext.y);
	dbfDistance = abs(sinABC * vBC.GetLength());

	return  dbfDistance;
}

long  SmtLocate(dbfPoint &pt1,dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext)
{	
	if( nPoint < 2) 
		return SMT_ERR_INVALID_PARAM;

	dbfPoint PrePoint,NexPoint;
	float	 Mintheta = 1.0,temp;
	int      starti = indexPre;

	for (int i = starti; i < nPoint - 1; i++)
	{
		PrePoint = pPoints[i];
		NexPoint = pPoints[i+1];
		temp = SmtCosABC( PrePoint, pt1, NexPoint);

		if ( temp < Mintheta)
		{
			indexPre = i;
			indexNext = i + 1;
			Mintheta = temp;
		}

		if ( Mintheta == -1) 
		{
			return SMT_ERR_NONE;
		}
	}

	return SMT_ERR_FAILURE;
}

int   SmtGetH(dbfPoint A,dbfPoint B,dbfPoint P,dbfPoint& H)
{//返回值：-1A延长线上,0在线段端点之间,1在AB延长线上,2 AB重复(此时H无意义)

	int flag = 0;
	if (A.x == B.x && A.y == B.y) 
		flag = 2;
	else if (A.x == B.x) 
	{//平行于y轴
		H.x = A.x;
		H.y = P.y;
		float m,n,o;
		m = (P.y - A.y);
		n = (P.y - B.y);
		o = m*n;
		if (o < 0) flag = 0;
		else if (abs(m) < abs(n)) flag = -1;
		else flag = 1;
	}

	else if (A.y == B.y)
	{//平行于x轴		
		H.y = A.y;
		H.x = P.x;

		float m,n,o;
		m = (P.x - A.x);
		n = (P.x - B.x);
		o = m*n;
		if (o < 0) flag = 0;
		else if (abs(m) < abs(n))  flag = -1;
		else flag = 1;
	}
	else
	{
		float k;
		k = (B.y - A.y)/(B.x - A.x);
		H.x = (k*k*A.x +k*(P.y - A.y)+P.x)/(k*k + 1);
		H.y = k*(H.x-A.x) + A.y;

		//
		float m,n,o;
		m = (H.x - A.x);
		n = (H.x - B.x);
		o = m*n;

		if (o < 0) 
			flag = 0;
		else if (abs(m) < abs(n))  
			flag = -1;
		else flag = 1;
	}

	return flag;
}

long  SmtLocate(dbfPoint &pt1,std::vector<dbfPoint> &vPoints,int &indexPre,int &indexNext)
{	
	if( vPoints.size() < 2) 
		return SMT_ERR_INVALID_PARAM;

	dbfPoint PrePoint,NexPoint;
	float	 Mintheta = 1.0,temp;
	int      starti = indexPre;

	for (int i = starti; i < vPoints.size() - 1; i++)
	{
		PrePoint = vPoints[i];
		NexPoint = vPoints[i+1];
		temp = SmtCosABC( PrePoint, pt1, NexPoint);

		if ( temp < Mintheta)
		{
			indexPre = i;
			indexNext = i + 1;
			Mintheta = temp;
		}

		if ( Mintheta == -1) 
		{
			return SMT_ERR_NONE;
		}
	}

	return SMT_ERR_FAILURE;
}

double SmtDistance_New(dbfPoint &Point,dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext)
{
	if(nPoint < 2 || NULL == pPoints) 
		return SMT_C_INVALID_DBF_VALUE;

	int			i;
	double		d, Distance;
	dbfPoint		*pA, *pB, pt,Next;

	Distance	= -1.0;

	pB	= pPoints;
	pA	= pB + 1;

	Distance	= SmtGetNearestPointOnLine(Point, *pA, *pB, Next);

	for(i=1; i<nPoint && Distance!=0.0; i++, pB=pA++)
	{
		if(	(d = SmtGetNearestPointOnLine(Point, *pA, *pB, pt)) >= 0.0&&
			(d < Distance || Distance < 0.0))
		{
			Distance	= d;
			Next		= pt;
			indexPre++;
			indexNext = indexPre+1;
		}
	}

	return( Distance );
}

double SmtDistance_New(dbfPoint &Point,std::vector<dbfPoint> &vPoints,int &indexPre,int &indexNext)
{
	if(vPoints.size() < 2) 
		return SMT_C_INVALID_DBF_VALUE;

	int			i;
	double		d, Distance;
	dbfPoint	pt,Next;

	Distance	= -1.0;


	std::vector<dbfPoint>::iterator iterA,iterB;
	iterB = vPoints.begin();
	iterA = iterB+1;
	
	Distance	= SmtGetNearestPointOnLine(Point, *iterA, *iterB, Next);

	for(i=1; i < vPoints.size() && Distance!=0.0; i++, iterB=iterA++)
	{
		if(	(d = SmtGetNearestPointOnLine(Point, *iterA, *iterB, pt)) >= 0.0&&
			(d < Distance || Distance < 0.0))
		{
			Distance	= d;
			Next		= pt;
			indexPre++;
			indexNext = indexPre+1;
		}
	}

	return( Distance );
}

long   SmtPointToLineSect(dbfPoint &C,dbfPoint &A,dbfPoint &B)
{
	/*
	B ^
	|
	P1	   |        P2
	|v1
	|
	A |
	*/

	Vector v1(B.x - A.x,B.y - A.y),v2(C.x - A.x,C.y - A.y);

	double f = v1.CrossProduct(v2);

	if (f < 0) 
		return 1;
	else if (f > 0) 
		return -1;
	else 
		return 0;
}

long   SmtPointToPolygon(dbfPoint &pt,dbfPoint *pPoints,int nPoint)
{
	if(nPoint < 3 || NULL == pPoints) 
		return -2;

	int i,n =nPoint;
	double cos0;
	double flag=0;//用于累加角度
	double m;
	dbfPoint ptPre,ptNext;
	for(i=n-2;i>=0;i--)
	{
		ptPre = pPoints[i];
		ptNext = pPoints[i+1];

		//叉积判断点是否在线段
		Vector v1(ptNext.x - ptPre.x,ptNext.y - ptPre.y),v2(pt.x - ptPre.x,pt.y - ptPre.y);
		m = v1.CrossProduct(v2);
		if( m==0 )
		{
			ptPre = pPoints[i];
			ptNext = pPoints[i+1];

			if(pt.x>=min(ptNext.x,ptPre.x) && pt.x<=max(ptNext.x,ptPre.x) && 
			   pt.y>=min(ptNext.y,ptPre.y) && pt.y<=max(ptNext.y,ptPre.y) )
			{
				//在边上
				return 0;
			}
			continue;
		}

		cos0 = SmtCosABC(ptNext,pt,ptPre);

		if( m > 0 )
			flag+=acos(cos0);
		else
			flag-=acos(cos0);
	}

	if(fabs(flag)<3.0)
		return  1; //点在多边形外部
	else
		return -1; //点在多边形内部
}

long   SmtPointToPolygon_New(dbfPoint &pt,dbfPoint *pPoints,int nPoint)
{
	if(nPoint < 3 || NULL == pPoints) 
		return -2;

	int i,j,k,wn=0;
	int n =nPoint;
	for(i=n-1, j=0; j<n; i=j, j++)
	{
		k = (pt.x - pPoints[i].x) * (pPoints[j].y - pPoints[i].y) - (pPoints[j].x - pPoints[i].x) * (pt.y - pPoints[i].y);
		if((pt.y >= pPoints[i].y && pt.y <= pPoints[j].y)||(pt.y <= pPoints[i].y && pt.y >= pPoints[j].y))
		{
			if( k < 0)
				wn++;
			else if(k > 0)
				wn--;
			else
			{
				if( (pt.y <= pPoints[i].y && pt.y >= pPoints[j].y && pt.x <= pPoints[i].x && pt.x >= pPoints[j].x) ||
					(pt.y <= pPoints[i].y && pt.y >= pPoints[j].y && pt.x >= pPoints[i].x && pt.x <= pPoints[j].x) ||
					(pt.y >= pPoints[i].y && pt.y <= pPoints[j].y && pt.x <= pPoints[i].x && pt.x >= pPoints[j].x) ||
					(pt.y >= pPoints[i].y && pt.y <= pPoints[j].y && pt.x >= pPoints[i].x && pt.x <= pPoints[j].x) )

					return 0; //点在多边形边界上
			}
		}
	}
	if(wn == 0)
		return  1; //点在多边形外部
	else
		return -1; //点在多边形内部
}

long SmtPointToPolygon_New1(dbfPoint &pt,dbfPoint *pPoints,int nPoint)
{
	if(nPoint < 3 || NULL == pPoints) 
		return -2;

	Envelope env;
	SmtGetEnvlope(env,pPoints,nPoint);

	if(	nPoint > 2 /*&& env.Contains(pt.x,pt.y)*/ )
	{
		int			nCrossings;
		dbfPoint	A, B, C, *pA, *pB;

		nCrossings	= 0;

		A.x			= env.MinX;
		B.x			= pt.x;
		A.y = B.y	= pt.y;

		pB	= pPoints + nPoint - 1;
		pA	= pPoints;

		for(int iPoint=0, goNext=0; iPoint<nPoint; iPoint++, pB=pA++)
		{
			if( pA->y != pB->y )
			{
				if( pA->y == pt.y )
				{
					goNext	= pA->y > pB->y ? 1 : -1;
				}
				else if( goNext )	// pB->y == y
				{
					if( ((goNext > 0 && pA->y > pB->y) || (goNext < 0 && pA->y < pB->y)) && pB->x <= B.x )
						nCrossings++;

					goNext	= 0;
				}
				else if( ((pB->y < pt.y && pt.y <= pA->y) || (pB->y > pt.y && pt.y >= pA->y)) && (pB->x < pt.x || pA->x < pt.x) )
				{
					if( SmtGetCrossing(C, *pA, *pB, A, B) )
					{
						nCrossings++;
					}
				}
			}
		}

		if ( nCrossings % 2 != 0)
			return -1;
		else
			return 1;
	}

	return 1;
}


bool	SmtGetCrossing(dbfPoint &Crossing, const dbfPoint &a1, const dbfPoint &a2, const dbfPoint &b1, const dbfPoint &b2, bool bExactMatch)
{
	double	lambda, div, a_dx, a_dy, b_dx, b_dy;

	if( bExactMatch
		&&	((max(a1.x, a2.x) < min(b1.x, b2.x))||
		(min(a1.x, a2.x) > max(b1.x, b2.x))||
		(max(a1.y, a2.y) < min(b1.y, b2.y))||
		(min(a1.y, a2.y) > max(b1.y, b2.y))))
	{
		return( false );
	}

	a_dx	= a2.x - a1.x;
	a_dy	= a2.y - a1.y;

	b_dx	= b2.x - b1.x;
	b_dy	= b2.y - b1.y;

	if( (div = a_dx * b_dy - b_dx * a_dy) != 0.0 )
	{
		lambda		= ((b1.x - a1.x) * b_dy - b_dx * (b1.y - a1.y)) / div;

		Crossing.x	= a1.x + lambda * a_dx;
		Crossing.y	= a1.y + lambda * a_dy;

		if( !bExactMatch )
		{
			return( true );
		}
		else if( 0.0 <= lambda && lambda <= 1.0 )
		{
			lambda	= ((b1.x - a1.x) * a_dy - a_dx * (b1.y - a1.y)) / div;

			if( 0.0 <= lambda && lambda <= 1.0 )
			{
				return( true );
			}
		}
	}

	return( false );
}

double		SmtGetNearestPointOnLine(const dbfPoint &Point, const dbfPoint &Ln_A, const dbfPoint &Ln_B, dbfPoint &Ln_Point, bool bExactMatch)
{
	double		dx, dy, Distance, d;
	dbfPoint	Point_B;

	Point_B.x	= Point.x - (Ln_B.y - Ln_A.y);
	Point_B.y	= Point.y + (Ln_B.x - Ln_A.x);

	if( SmtGetCrossing(Ln_Point, Ln_A, Ln_B, Point, Point_B, false) )
	{
		if( !bExactMatch || (bExactMatch && SMT_IS_BETWEEN(Ln_A.x, Ln_Point.x, Ln_B.x) && SMT_IS_BETWEEN(Ln_A.y, Ln_Point.y, Ln_B.y)) )
		{
			dx			= Point.x - Ln_Point.x;
			dy			= Point.y - Ln_Point.y;
			Distance	= sqrt(dx*dx + dy*dy);
		}
		else
		{
			dx			= Point.x - Ln_A.x;
			dy			= Point.y - Ln_A.y;
			d			= sqrt(dx*dx + dy*dy);

			dx			= Point.x - Ln_B.x;
			dy			= Point.y - Ln_B.y;
			Distance	= sqrt(dx*dx + dy*dy);

			if( d < Distance )
			{
				Distance	= d;
				Ln_Point	= Ln_A;
			}
			else
			{
				Ln_Point	= Ln_B;
			}
		}

		return( Distance );
	}

	return( -1.0 );
}


//////////////////////////////////////////////////////////////////////////
double  SmtCalcPolygonArea(dbfPoint *pPoints,int nPoint)
{
	double dfAreaSum = 0.0;
	int i;

	if( pPoints == NULL ||nPoint < 2 )
		return 0;

	dfAreaSum = pPoints[0].x * (pPoints[1].y - pPoints[nPoint-1].y);

	for( i = 1; i < nPoint-1; i++ )
	{
		dfAreaSum += pPoints[i].x * (pPoints[i+1].y - pPoints[i-1].y);
	}

	dfAreaSum += pPoints[nPoint-1].x * (pPoints[0].y - pPoints[nPoint-2].y);

	return 0.5 * fabs(dfAreaSum);
}

double   SmtCalcTriArea(dbfPoint A,dbfPoint B,dbfPoint C)
{
	return (0.5 * ((B.x-A.x)*(C.y-A.y) - (B.y-A.y)*(C.x-A.x)));
}

//////////////////////////////////////////////////////////////////////////
double  SmtCosABC(dbfPoint A,dbfPoint B,dbfPoint C)
{
	double theta;
	Vector BA(A.x - B.x,A.y - B.y),BC(C.x - B.x,C.y - B.y);
	theta = (BA*BC)/(BC.GetLength()*BA.GetLength());  

	return theta;
}

//////////////////////////////////////////////////////////////////////////
long  SmtInterpolate_Lugrange(dbfPoint *& pPnts,int &nPnts,dbfPoint * pCtrlPnts,int nCtrlPnts,int nSection)
{
	if (NULL == pCtrlPnts || nCtrlPnts < 4 || nSection < 1)
		return SMT_ERR_INVALID_PARAM;
	
	float **ppfBlend = new float*[4];
	float **ppBlend = new float*[4];
	float **pplBlend = new float*[4];

	for (int i = 0; i < 4;i++)
	{
		ppfBlend[i] = new float[nSection];
		ppBlend[i] = new float[nSection];
		pplBlend[i] = new float[nSection];
	}

	long xTmp[4],yTmp[4];
	float u,v,w,x,y;
	for ( int i = 0; i < nSection ;i++ )
	{
		u = (float)i/nSection;
		ppBlend[0][i] = u * (u-1) * ( u -2 )/(-6);
		ppBlend[1][i] = ( u + 1 ) * ( u - 1 )* ( u - 2 )/2;
		ppBlend[2][i] = ( u + 1 ) * u * ( u -2 )/ (-2);
		ppBlend[3][i] = ( u + 1 ) * u * ( u - 1 ) /6;

		v = u - 1;
		ppfBlend[0][i] = v * (v-1) * ( v -2 )/(-6);
		ppfBlend[1][i] = ( v + 1 ) * ( v - 1 )* ( v - 2 )/2;
		ppfBlend[2][i] = ( v + 1 ) * v * ( v -2 )/ (-2);
		ppfBlend[3][i] = ( v + 1 ) * v * ( v - 1 ) /6;

		w = u + 1;
		pplBlend[0][i] = w * (w-1) * ( w -2 )/(-6);
		pplBlend[1][i] = ( w + 1 ) * ( w - 1 )* ( w - 2 )/2;
		pplBlend[2][i] = ( w + 1 ) * w * ( w -2 )/ (-2);
		pplBlend[3][i] = ( w + 1 ) * w * ( w - 1 ) /6;
	}
	for (int i = 0 ;i < 4 ; i++ )
	{
		xTmp[i] = pCtrlPnts[i].x;
		yTmp[i] = pCtrlPnts[i].y;
	}

	int nNewCount = 0;
	nPnts = (nCtrlPnts - 2)*nSection+2;
	pPnts = new dbfPoint[nPnts];
	//////////////////////////////////////////////////////////////////////////////////////////
	pPnts[nNewCount++] = pCtrlPnts[0];

	for ( int j = 0; j < nSection;j++ )
	{
		x=y=0.0;
		for (int i = 0; i< 4 ; i ++ )
		{
			x+=xTmp[i] * ppfBlend[i][j];
			y+=yTmp[i] * ppfBlend[i][j];
		}
		pPnts[nNewCount].x = x;
		pPnts[nNewCount++].y = y;
	}
	////////////////////////////////////////////////////////////////////////////////////////////
	for ( int m = 4;m < nCtrlPnts ; m++)
	{
		for (int j = 0;j < nSection; j++)
		{
			x=y=0;
			for (int i = 0;i < 4 ; i++)
			{
				x+=xTmp[i]*ppBlend[i][j];
				y+=yTmp[i]*ppBlend[i][j];
			}
			pPnts[nNewCount].x = x;
			pPnts[nNewCount++].y = y;
		}
		for (int i = 0 ;i < 3; i++)
		{
			xTmp[i] = xTmp[i+1];
			yTmp[i] = yTmp[i+1];
		}
		xTmp[3]= pCtrlPnts[m].x;
		yTmp[3]= pCtrlPnts[m].y;
	}
	///////////////////////////////////////////////////////////////////////////////////////////
	for ( int j = 0; j < nSection;j++)
	{
		x=y=0;
		for ( int i = 0 ; i < 4; i++ )
		{
			x+=xTmp[i]*pplBlend[i][j];
			y+=yTmp[i]*pplBlend[i][j];
		}
		pPnts[nNewCount].x = x;
		pPnts[nNewCount++].y = y;
	}

	pPnts[nNewCount].x = xTmp[3];
	pPnts[nNewCount++].y = yTmp[3];

	//释放
	for (int i = 0; i < 4;i++)
	{
		SMT_SAFE_DELETE_A(ppfBlend[i]);
		SMT_SAFE_DELETE_A(ppBlend[i]);
		SMT_SAFE_DELETE_A(pplBlend[i]);
	}

	SMT_SAFE_DELETE_A(ppfBlend);
	SMT_SAFE_DELETE_A(ppBlend);
	SMT_SAFE_DELETE_A(pplBlend);
	
	return SMT_ERR_NONE;
}

long  SmtInterpolate_Geometry(dbfPoint *& pPnts,int &nPnts,dbfPoint * pCtrlPnts,int nCtrlPnts,float dt)
{
	if (NULL == pCtrlPnts || nCtrlPnts < 4 || dt < 0.)
		return SMT_ERR_INVALID_PARAM;

	int		i , j , k , n ;
	float	t;
	dbfPoint*q = NULL,*r = NULL;

	q = new dbfPoint[nCtrlPnts];
	r = new dbfPoint[nCtrlPnts];

	int nNewCount = 0;
	nPnts = (1/dt)+2 ;
	pPnts = new dbfPoint[nPnts];

	pPnts[0] = pCtrlPnts[0];
	for ( t = 0 ; t <= 1 ; t+= dt )
	{
		for ( i = 0; i < nCtrlPnts; i++ ) 
			r[i] = pCtrlPnts[i];

		n = nCtrlPnts-1;
		while ( n > 0)
		{
			for ( k = 0; k < n ; k++ )
			{
				q[k].x = r[k].x + t * (r [k+1].x - r[k].x);
				q[k].y = r[k].y + t * (r [k+1].y - r[k].y);
			}

			n=n-1;
			for ( j = 0; j <= n ; j++ )
				r[j] = q[j];
		}
		pPnts[nNewCount++] = r[0];
	}

	pPnts[nNewCount++] = pCtrlPnts[nCtrlPnts - 1];

	SMT_SAFE_DELETE_A(q);
	SMT_SAFE_DELETE_A(r);
	
	return SMT_ERR_NONE;
}

long  SmtInterpolate_BSpline(dbfPoint *& pPnts,int &nPnts,dbfPoint * pCtrlPnts,int nCtrlPnts,float dt)
{
	if (NULL == pCtrlPnts || nCtrlPnts < 4 || dt < 0.)
		return SMT_ERR_INVALID_PARAM;

	float B[4];
	long xTmp[4],yTmp[4];
	float x,y,t;
	for ( int i = 0 ;i < 4 ; i++ )
	{
		xTmp[i] = pCtrlPnts[0].x;
		yTmp[i] = pCtrlPnts[0].y;
	}

	int nNewCount = 0;
	nPnts = nCtrlPnts * (1/dt+1)+2 ;
	pPnts = new dbfPoint[nPnts];

	pPnts[nNewCount++] = pCtrlPnts[0];

	for ( int m = 0;m < nCtrlPnts ; m++)
	{
		for ( t = 0; t < 1; t+=dt)
		{
			B[0] = pow((1-t),3)/6.0; 
			B[1] = (3*t*t*t- 6 * t * t + 4)/6.0;
			B[2] = (- 3* t*t*t + 3*t*t + 3 * t + 1)/6.0;
			B[3] = t*t*t / 6.0;	

			x=y=0;
			for (int i = 0;i < 4 ; i++)
			{
				x+=xTmp[i]*B[i];
				y+=yTmp[i]*B[i];
			}

			pPnts[nNewCount].x = x;
			pPnts[nNewCount++].y = y;
		}

		for (int i = 0 ;i < 3; i++)
		{
			xTmp[i] = xTmp[i+1];
			yTmp[i] = yTmp[i+1];
		}
		xTmp[3]= pCtrlPnts[m].x;
		yTmp[3]= pCtrlPnts[m].y;
	}

	pPnts[nNewCount++] = pCtrlPnts[nCtrlPnts - 1];

	return SMT_ERR_NONE;
}

long  SmtInterpolate_3Spline(dbfPoint *& pPnts,int &nPnts,dbfPoint * pCtrlPnts,int nCtrlPnts,int nInterCount)
{
	int n = nCtrlPnts -1 , m = nInterCount ;
	int i , j;
	double *Q1_x , *Q1_y , * alfa_x, *alfa_y , * beta_x , * beta_y , * gama_x, * gama_y;
	double ax,ay,bx,by,cx,cy,dx,dy,t;	
	Q1_x = new double [n+1];
	Q1_y = new double [n+1];
	alfa_x = new double [n+1];
	alfa_y = new double [n+1];
	beta_x = new double [n+1];
	beta_y = new double [n+1];
	gama_x = new double [n+1];
	gama_y = new double [n+1];
	float x , y;
	Q1_x[0] = 1; Q1_y[0] = 1;Q1_x[n] = 1; Q1_y[n] = 1;
	beta_x[0] = 0;beta_y[0] = 0; alfa_x[0] = 0;alfa_y[0] = 0;
	gama_x[1]=3*(pCtrlPnts[2].x - pCtrlPnts[0].x ) -Q1_x[0];
	gama_y[1]=3*(pCtrlPnts[2].y - pCtrlPnts[0].y ) -Q1_y[0];
	gama_x[n-1]=3*(pCtrlPnts[n].x - pCtrlPnts[n-2].x ) -Q1_x[n];
	gama_y[n-1]=3*(pCtrlPnts[n].y - pCtrlPnts[n-2].y ) -Q1_y[n];
	for ( i =1 ;i < n ;i++)
	{
		if ( i != 1 && i != n-1)
		{
			gama_x[i]=3*(pCtrlPnts[i+1].x - pCtrlPnts[i-1].x ) ;
			gama_y[i]=3*(pCtrlPnts[i+1].y - pCtrlPnts[i-1].y ) ;
		}
		beta_x[i] = -1/(4+beta_x[i-1]);
		beta_y[i] = -1/(4+beta_y[i-1]);
		alfa_x[i] = (alfa_x[i-1]-gama_x[i])*beta_x[i];
		alfa_y[i] = (alfa_y[i-1]-gama_y[i])*beta_y[i];
	}
	for ( i = n-1 ;i > 0 ;i --)
	{
		Q1_x[i] = alfa_x[i]+beta_x[i]*Q1_x[i+1];
		Q1_y[i] = alfa_y[i]+beta_y[i]*Q1_y[i+1];
	}

	int nNewCount = 0;
	nPnts = m*n+1 ;
	pPnts = new dbfPoint[nPnts];

	pPnts[nNewCount++] = pCtrlPnts[0];

	for ( i =1 ;i <= n ;i++)
	{
		ax = Q1_x[i] + Q1_x[i-1] - 2*(pCtrlPnts[i].x - pCtrlPnts[i-1].x);
		ay = Q1_y[i] + Q1_y[i-1] - 2*(pCtrlPnts[i].y - pCtrlPnts[i-1].y);
		bx = -Q1_x[i] - 2*Q1_x[i-1] + 3*(pCtrlPnts[i].x - pCtrlPnts[i-1].x);
		by = -Q1_y[i] - 2*Q1_y[i-1] + 3*(pCtrlPnts[i].y - pCtrlPnts[i-1].y);
		cx = Q1_x[i-1];
		cy = Q1_y[i-1];
		dx = pCtrlPnts[i-1].x;
		dy = pCtrlPnts[i-1].y;
		for ( j = 1 ;j <= m ; j++ )
		{
			t = (float)j/m;
			x = ax*t*t*t + bx*t*t + cx * t +dx;
			y = ay*t*t*t + by*t*t + cy * t +dy;

			pPnts[nNewCount].x = x;
			pPnts[nNewCount++].y = y;
		}
	}

	delete []Q1_x;
	delete []Q1_y;
	delete []alfa_x;
	delete []alfa_y;
	delete []beta_x;
	delete []beta_y;
	delete []gama_x;
	delete []gama_y;

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
#define SMT_INF 1000000000 

bool SmtLineSegOverLineSeg(const float &x1,const float & y1,
						const float &x2,const float & y2,
						const float & u1,const float & v1,
						const float & u2,const float & v2)
						//判断线段(x1,y1)--(x2,y2)和线段(u1,v1)--(u2,v2)是否相交（不包括端点）
{
	//判断(u1,v1)和(u2,v2)是否在直线(x1,y1)-(x2,y2)异同侧
	bool u1v1_separate_u2v2=false;
	float a=(v1-y1)*(x2-x1)-(y2-y1)*(u1-x1);
	float b=(v2-y1)*(x2-x1)-(y2-y1)*(u2-x1);
	if(a*b<0)
		u1v1_separate_u2v2=true;

	//判断(x1,y1)和(x2,y2)是否在直线(u1,v1)-(u2,v2)异同侧
	bool x1y1_separate_x2y2=false;
	a=(y1-v1)*(u2-u1)-(v2-v1)*(x1-u1);
	b=(y2-v1)*(u2-u1)-(v2-v1)*(x2-u1);

	if(a*b<0)x1y1_separate_x2y2=true;

	return x1y1_separate_x2y2*u1v1_separate_u2v2;
}

bool SmtPointInTri(const dbfPoint &v,const dbfPoint &v1,const dbfPoint &v2,const dbfPoint &v3)
{//看v是否在三角形v1,v2,v3内
	//作直线(v[X],v[Y])-(inf,v[Y])看是否与线段v1v2,线段v2v3,线段v3v4中恰一个有交点
	bool over12=SmtLineSegOverLineSeg(v.x,v.y,SMT_INF,v.y,v1.x,v1.y,v2.x,v2.y);
	bool over23=SmtLineSegOverLineSeg(v.x,v.y,SMT_INF,v.y,v2.x,v2.y,v3.x,v3.y);
	bool over31=SmtLineSegOverLineSeg(v.x,v.y,SMT_INF,v.y,v3.x,v3.y,v1.x,v1.y);

	if(over12+over23+over31==1)
	{//说明在三角形内
		return true;
	}
	return false;
}

double SmtIsCCW(dbfPoint & a, dbfPoint & b, dbfPoint & c)
{
	return ((b.x - a.x)*(c.y - b.y) - (c.x - b.x)*(b.y - a.y));
}

long SmtDivPolygenIntoTriMesh(vector<SmtTriangle> &trilist,dbfPoint *pPoints,int nPoint)
//将多边形切割成三角网格
//对顺时针的非自相交多边形得到正确切割，前端返回true
//对自相交多边形和逆时针多边形所得IDtriList为空，前端返回false
{
	//如果顶点数不足三个，直接退出
	nPoint--;
	if (NULL == pPoints || nPoint < 3)
		return SMT_ERR_INVALID_PARAM;

	//自相交检测（如果有自相交，则返回）
	int n = nPoint;
	for(int i=0;i<=n-1;i++)
	{
		int i1=i;
		int i2=(i+1)%n;
		const dbfPoint & v1 = pPoints[i1];
		const dbfPoint & v2 = pPoints[i2];
		//线段v1v2与其后各边进行相交检测
		for(int j=i2;j<=n-1;j++)
		{
			int j1=j;
			int j2=(j+1)%n;
			if(j1==i2||j2==i1)
				continue;//跳过相邻边

			const dbfPoint & u1 = pPoints[j1];
			const dbfPoint & u2 = pPoints[j2];
			//对v1v2与u1u2进行相交检测
			if(SmtLineSegOverLineSeg(v1.x,v1.y,v2.x,v2.y,u1.x,u1.y,u2.x,u2.y))
			{
				return SMT_ERR_FAILURE;
			}
		}
	}//一定无自相交
	//顺时针层层削减掉非凹角
	vector<dbfPoint>		tvlist;//削减后剩余的顶点列表
	vector<int>				vIndex;

	//将vlist拷贝给tvlist并填充顶点的永久编号
	tvlist.resize(nPoint);
	vIndex.resize(nPoint);
	for(int i = 0;i <= nPoint-1;i++)
	{
		tvlist[i] = pPoints[i];
		vIndex[i] = i;
	}//tvlist初始化完成

	int i=0;//顶点下标
	int count=0;//统计削减连续不成功次数
	while(1)
	{
		//进行一次削减尝试
		//尝试削掉三角形i,i+1,i+2
		//看i,i+1,i+2是否为凸角且tvlist中无点落在三角形i,i+1,i+2内
		//看是否为凸角
		int n=(int)tvlist.size();
		int i1=i%n;
		int i2=(i+1)%n;
		int i3=(i+2)%n;
		dbfPoint &v1=tvlist[i1];
		dbfPoint &v2=tvlist[i2];
		dbfPoint &v3=tvlist[i3];
		float vec1[2]={v2.x-v1.x,v2.y-v1.y};
		float vec2[2]={v3.x-v2.x,v3.y-v2.y};
		float cross12= vec2[0]*vec1[1]-vec1[0]*vec2[1];
		bool isHeaveOrEven=false;//是否非凹
		//顺时针情况下凸角叉积<0，平角叉积=0，凹角叉积>0
		if(cross12 >=0)
		{//如果非凹
			isHeaveOrEven=true;
		}//得到isHeaveOrEven
		//看vlist中是否有点落在三角形i,i+1,i+2内
		bool isInTri=false;//是否在三角形内
		for(int j=0;j<=n-1;j++)
		{
			if(j==i1||j==i2||j==i3)
				continue;
			dbfPoint &v=tvlist[j];
			//看v是否在三角形v1,v2,v3内
			if(SmtPointInTri(v,v1,v2,v3))
			{
				isInTri=true;
				break;
			}
		}//得到isInTri

		if(isHeaveOrEven&&!isInTri)
		{//可以削减掉三角形i1,i2,i3
			SmtTriangle tri;
			tri.a = vIndex[i3];
			tri.b = vIndex[i2];
			tri.c = vIndex[i1];

			trilist.push_back(tri);

			//在tvlist中去掉i2
			tvlist.erase(tvlist.begin()+i2);
			vIndex.erase(vIndex.begin()+i2);
			i++;
			count=0;

			if((int)tvlist.size()==2)
			{//如果只剩两点了
				//切割完毕，返回
				//调整顺序
				for (int i = 0; i < trilist.size();i++)
				{
					if (SmtIsCCW(pPoints[trilist[i].a],pPoints[trilist[i].b],pPoints[trilist[i].c]) > 0)
					{
						int temp = trilist[i].b;
						trilist[i].b = trilist[i].c;
						trilist[i].c = temp;
					}
				}
				return SMT_ERR_NONE;
			}
		}else
		{//不可削减掉三角形i1,i2,i3
			i++;
			count++;
			if(count==(int)tvlist.size())
			{//如果已连续削减一周都没有能削减下来，说明不可削减
				trilist.resize(0);

				return SMT_ERR_FAILURE;
			}
		}
	}

	return SMT_ERR_NONE;
}
