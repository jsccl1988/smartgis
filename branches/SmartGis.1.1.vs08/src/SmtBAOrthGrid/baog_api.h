/*
File:    bog_api.h

Desc:    SmtBAOrthGrid ����API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BAOG_API_H
#define _BAOG_API_H

#include "baog_bas_struct.h"

namespace Smt_BAOrthGrid
{
	void		Spline3(vCtrlPoints & ctrlPts,vCurvePoints & interPts,int np);//����������ֵ��ctrlPts���Ƶ㡣ÿ�������Ƶ�֮���ֵnp���㣬��ֵ�õ��ĵ�洢��interPts��
	void		CurveSampling(vCurvePoints & orgCurve,vCurvePoints & newCurve,int N);
	void		GetCtrlPointsPosOnCurve(vCtrlPoints  & ctrlPts,vCurvePoints & curvePts);
	void		SetCtrlPoints(vCtrlPoints & ctrlPts,vdbfPoints & Pts);
	void		SetCurvePoints(vCtrlPoints& ctrlPts,vdbfPoints &Pts);
}

#endif //_BAOG_API_H
