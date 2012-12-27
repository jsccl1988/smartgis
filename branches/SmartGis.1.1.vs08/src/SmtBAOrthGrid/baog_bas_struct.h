#ifndef _BOG_BAS_STRUCT_H
#define _BOG_BAS_STRUCT_H

#include "smt_bas_struct.h"
#include "rd_renderdevice.h"

using namespace Smt_Core;
using namespace Smt_Rd;

namespace Smt_BAOrthGrid
{
	struct GridCell 
	{
		bool			IsSelected;          //�Ƿ�ѡ��
		int				MaskerClr;           //�����ɫ
		dbfPoint		P;					 //���ĵ�����
	};

	struct CtrlPoint 
	{
		dbfPoint		P;
		int				PreIndex;
		int				NexIndex;
	};

	typedef std::vector<CtrlPoint>			vCtrlPoints;
	typedef std::vector<dbfPoint>			vdbfPoints;  
	typedef std::vector<dbfPoint>			vCurvePoints;  

	struct SmtBAOGBoudary
	{
		int				flag;//0,1,2,3  ���ڣ��±߽磬�ұ߽磬�ϱ߽磬��߽�
		int				start,end;
		int				index;
		bool			can_slide;
		bool			can_sample;
		vCtrlPoints		ctrlPts;
		vCurvePoints	curvePts;

		void Clear()
		{
			flag = -1;
			start = -1;
			end = -1;
			index = -1;
			can_slide = true;
			can_sample = true;
			ctrlPts.clear();
			curvePts.clear();
		}
	};

	typedef std::vector<SmtBAOGBoudary *> vBoudaryPtrs;

	//////////////////////////////////////////////////////////////////////////
	const int         nMaxTimes = 5000;
	const double      ddEps     = 0.1;
	const double      fInvalidNum = 1.00000e+006;

#define InValid(P) ((P.x) == fInvalidNum || (P.y) == fInvalidNum)

}

#endif// _BOG_BAS_STRUCT_H