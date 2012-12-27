/*
File:    geo_api.h

Desc:    SmartGis����Geom API

Version: Version 1.0

Writter:  �´���

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
����:��ȡ��ɢ����������MBR
����:
���:
����:                                                              
*/
/************************************************************************/
long   SMT_EXPORT_API SmtGetEnvlope(Smt_Base::Envelope &env,Smt_Core::dbfPoint *pPoints,int nPoint);


//////////////////////////////////////////////////////////////////////////

/************************************************************************/
/* 
����:������֮�����
����:
���:
����:                                                                 
*/
/************************************************************************/

double SMT_EXPORT_API SmtDistance(double x1,double y1,double x2, double y2);


/************************************************************************/
/* 
����:��������ߵľ���
����:
���:
����:                                                                 
*/
/************************************************************************/
double SMT_EXPORT_API SmtDistance(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
double SMT_EXPORT_API SmtDistance(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

/************************************************************************/
/* 
����:��������ߵľ���
����:
���:
����:                                                                 
*/
/************************************************************************/
double SMT_EXPORT_API SmtDistance_New(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
double SMT_EXPORT_API SmtDistance_New(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

/************************************************************************/
/* 
����:�ҵ���pointλ�������е����߶ζ˵�indexPre / indexNext֮��
����:
���:
����:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtLocate(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint,int &indexPre,int &indexNext);
long  SMT_EXPORT_API SmtLocate(Smt_Core::dbfPoint &pt1,std::vector<Smt_Core::dbfPoint> &vPoints,int &indexPre,int &indexNext);

//////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* 
����:�ж�������AB�Ĺ�ϵ
����:
���:
����:  -1 P������AB��࣬0 P ��ֱ��AB�ϣ�1 P������AB�Ҳ�                                                                
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToLineSect(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint &ptA,Smt_Core::dbfPoint &ptB);


/************************************************************************/
/* 
����:�ж����ڶ���εĹ�ϵ
����:
���:
����:  -1 in, 0 on, 1 out                                                             
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
����:�жϵ������ε�λ�ù�ϵ
�����ڶ�����ڲ������� -1�������ڶ���α߽��ϣ����� 0�������ڶ�����ⲿ������ 1��
���㷨�����������㷨����ͬ��Ч�ʣ����Ҷ������㷨����������Ĵ������������Ҳ�ܺõıܿ��˶�ת���㷨��arccosֵ�ļ��㣬
���㷨Ϊ��������Ϣϵͳ�㷨���������ź�ȣ���ѧ�����磩��ת���㷨�ĸĽ��汾��
����:
���:
����:  -1 in, 0 on, 1 out                                                               
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon_New(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
����:�ж����ڶ���εĹ�ϵ
����:
���:
����:  -1 in, 0 on, 1 out                                                               
*/
/************************************************************************/
long   SMT_EXPORT_API SmtPointToPolygon_New1(Smt_Core::dbfPoint &pt1,Smt_Core::dbfPoint *pPoints,int nPoint);

/************************************************************************/
/* 
����:�����������
����:
���:
����:                                                           
*/
/************************************************************************/
double   SMT_EXPORT_API SmtCalcPolygonArea(Smt_Core::dbfPoint *pPoints,int nPoint);


/************************************************************************/
/* 
����:�������������
����:
���:
����:                                                           
*/
/************************************************************************/
double   SMT_EXPORT_API SmtCalcTriArea(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint C);


/************************************************************************/
/* 
����:���p�㴹ֱ���߶�AB��ֱ����ֱ��AB�Ĵ���
����:
���:
����:                                                           
*/
/************************************************************************/
int   SMT_EXPORT_API	SmtGetH(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint P,Smt_Core::dbfPoint& H);

/************************************************************************/
/* 
����:���ABC������ֵ
����:
���:
����:                                                                 
*/
/************************************************************************/
double  SMT_EXPORT_API SmtCosABC(Smt_Core::dbfPoint A,Smt_Core::dbfPoint B,Smt_Core::dbfPoint C);


//////////////////////////////////////////////////////////////////////////
/************************************************************************/
/* 
����:�������ղ�ֵ
����:
���:
����:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_Lugrange(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,int nSection = 50);


/************************************************************************/
/* 
����:���β�ֵ
����:
���:
����:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_Geometry(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,float dt = 0.02);

/************************************************************************/
/* 
����:B������ֵ
����:
���:
����:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_BSpline(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,float dt = 0.02);

/************************************************************************/
/* 
����:����������ֵ
����:
���:
����:                                                                 
*/
/************************************************************************/
long  SMT_EXPORT_API SmtInterpolate_3Spline(Smt_Core::dbfPoint *& pPnts,int &nPnts,Smt_Core::dbfPoint * pCtrlPnts,int nCtrlPnts,int nInterCount = 50);

/************************************************************************/
/* 
����:�������ηָ�������Σ���Ҫ����ɰ�����ε����Ƿָ
����:
���:
����:                                                                 
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

