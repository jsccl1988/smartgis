/*
File:    geo_api.h

Desc:    SmartGis基础Geom API

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GEO_API_H
#define _GEO_API_H

#include "smt_bas_struct.h"
#include "bl_envelope.h"

//////////////////////////////////////////////////////////////////////////

/************************************************************************/
/* 
描述:获取离散点的外包矩形MBR
输入:
输出:
返回:                                                              
*/
/************************************************************************/
long   SMT_EXPORT_API SmtGetEnvlope(Smt_Base::Envelope &env,Smt_Core::dbfPoint *pPoints,int nPoint);


//////////////////////////////////////////////////////////////////////////

/************************************************************************/
/* 
描述:求两点之间距离
输入:
输出:
返回:                                                                 
*/
/************************************************************************/

double SMT_EXPORT_API SmtDistance(double x1,double y1,double x2, double y2);


/************************************************************************/
/* 
描述:求点于折线的距离
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
double SMT_EXPORT_API SmtDistance(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
double SMT_EXPORT_API SmtDistance(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

/************************************************************************/
/* 
描述:求点于折线的距离
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
double SMT_EXPORT_API SmtDistance_New(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
double SMT_EXPORT_API SmtDistance_New(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

/************************************************************************/
/* 
描述:找到点point位于曲线中的曲线段端点indexPre / indexNext之间
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtLocate(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
long  SMT_EXPORT_API SmtLocate(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

//////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* 
描述:判定点向量AB的关系
输入:
输出:
返回:  -1 P在向量AB左侧，0 P 在直线AB上，1 P在向量AB右侧                                                                
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToLineSect(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint &ptA,Smt_Core::dbfPoint &ptB);


/************************************************************************/
/* 
描述:判定点于多边形的关系
输入:
输出:
返回:  -1 in, 0 on, 1 out                                                             
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
描述:判断点与多边形的位置关系
若点在多变形内部，返回 -1；若点在多变形边界上，返回 0；若点在多变形外部，返回 1；
该算法不仅和射线算法有相同的效率，而且对射线算法中特殊情况的处理近乎完美，也很好的避开了对转角算法中arccos值的计算，
该算法为《地理信息系统算法基础》（张宏等，科学出版社）中转角算法的改进版本。
输入:
输出:
返回:  -1 in, 0 on, 1 out                                                               
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon_New(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
描述:判定点于多边形的关系
输入:
输出:
返回:  -1 in, 0 on, 1 out                                                               
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon_New1(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);

/************************************************************************/
/* 
描述:计算多边形面积
输入:
输出:
返回:                                                           
*/
/************************************************************************/
double   SMT_EXPORT_API SmtCalcPolygonArea(Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
描述:计算三角形面积
输入:
输出:
返回:                                                           
*/
/************************************************************************/
double   SMT_EXPORT_API SmtCalcTriArea(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint C);


/************************************************************************/
/* 
描述:求过p点垂直于线段AB的直线于直线AB的垂足
输入:
输出:
返回:                                                           
*/
/************************************************************************/
int   SMT_EXPORT_API	SmtGetH(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint P,Smt_Core::dbfPoint& H);

/************************************************************************/
/* 
描述:求角ABC的余弦值
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
double  SMT_EXPORT_API SmtCosABC(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint C);


//////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* 
描述:拉格朗日插值
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_Lugrange(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,int nSection = 50);


/************************************************************************/
/* 
描述:几何插值
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_Geometry(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,float dt = 0.02);

/************************************************************************/
/* 
描述:B样条插值
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_BSpline(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,float dt = 0.02);

/************************************************************************/
/* 
描述:三次样条插值
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_3Spline(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,int nInterCount = 50);

/************************************************************************/
/* 
描述:任意多边形分割成三角形（主要是完成凹多边形的三角分割）
输入:
输出:
返回:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtDivPolygenIntoTriMesh(vector<Smt_Core::SmtTriangle> &trilist,Smt_Core::dbfPoint *pPoints,int nPoint);

#if !defined(Export_SmtGeoCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGeoCoreD.lib")
#       else
#          pragma comment(lib,"SmtGeoCore.lib")
#	    endif  
#endif

#endif //_GEO_API_H

