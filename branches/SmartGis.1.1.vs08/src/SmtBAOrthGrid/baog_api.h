/*
File:    bog_api.h

Desc:    SmtBAOrthGrid 基础API

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BAOG_API_H
#define _BAOG_API_H

#include "baog_bas_struct.h"

namespace Smt_BAOrthGrid
{
	void		Spline3(vCtrlPoints & ctrlPts,vCurvePoints & interPts,int np);//三次样条插值，ctrlPts控制点。每两个控制点之间插值np个点，插值得到的点存储于interPts中
	void		CurveSampling(vCurvePoints & orgCurve,vCurvePoints & newCurve,int N);
	void		GetCtrlPointsPosOnCurve(vCtrlPoints  & ctrlPts,vCurvePoints & curvePts);
	void		SetCtrlPoints(vCtrlPoints & ctrlPts,vdbfPoints & Pts);
	void		SetCurvePoints(vCtrlPoints& ctrlPts,vdbfPoints &Pts);
}

#endif //_BAOG_API_H
