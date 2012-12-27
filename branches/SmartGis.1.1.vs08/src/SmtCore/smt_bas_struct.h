/*
File:    smt_bas_struct.h

Desc:    SmartGis基础数据类型文件

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_BAS_STRUCT_H
#define _SMT_BAS_STRUCT_H

#include "smt_core.h"
/*
int			4
uint		4
uhort		2
ushort		2
char		1
uchar		1
bool		1
byte		1
double		8
*/
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned long  ulong;
typedef unsigned int   uint;
typedef ushort         varType;
typedef unsigned char  byte;

#define TEMP_BUFFER_SIZE 255

namespace Smt_Core
{
	struct lPoint 
	{
		long x,y;

		lPoint(void)
		{
			x = 0;
			y = 0;
		}

		lPoint(long _x,long _y)
		{
			x = _x;
			y = _y;
		}

		inline bool operator == (lPoint lpt) const
		{
			return (x == lpt.x && y == lpt.y);
		}

		inline bool operator != (lPoint lpt) const
		{
			return !(*this==lpt);
		}
	};

	struct l3DPoint 
	{
		long x,y,z;

		l3DPoint(void)
		{
			x = 0;
			y = 0;
			z = 0;
		}

		l3DPoint(long _x,long _y,long _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}

		inline bool operator == (l3DPoint lpt) const
		{
			return (x == lpt.x && y == lpt.y && z == lpt.z);
		}

		inline bool operator != (l3DPoint lpt) const
		{
			return !(*this==lpt);
		}
	};

	struct lRect 
	{
		lPoint lb;
		lPoint rt;

		inline void Merge(long x, long y )
		{
			lPoint point0(0,0);
			if (lb == point0 && rt == point0)
			{
				lb = rt = lPoint(x,y);
			}
			else
			{
				lb.x = min(lb.x,x);
				rt.x = max(rt.x,x);
				lb.y = min(lb.y,y);
				rt.y = max(rt.y,y);
			}
		}

		inline long Height(void) const
		{
			return abs(rt.y-lb.y);
		}

		inline long Width(void) const
		{
			return abs(rt.x - lb.x);
		}
	};

	struct fPoint 
	{
		float x,y;

		fPoint(void)
		{
			x = 0;
			y = 0;
		}

		fPoint(float _x,float _y)
		{
			x = _x;
			y = _y;
		}

		inline bool operator == (fPoint lpt) const
		{
			return (SMT_EQUAL(x,lpt.x) && SMT_EQUAL(y ,lpt.y));
		}

		inline bool operator != (fPoint lpt) const
		{
			return !(*this==lpt);
		}
	};

	struct f3DPoint 
	{
		float x,y,z;

		f3DPoint(void)
		{
			x = 0;
			y = 0;
			z = 0;
		}

		f3DPoint(float _x,float _y,float _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}

		inline bool operator == (f3DPoint lpt) const
		{
			return (SMT_EQUAL(x,lpt.x) &&SMT_EQUAL(y ,lpt.y) && SMT_EQUAL(z ,lpt.z));
		}

		inline bool operator != (f3DPoint lpt) const
		{
			return !(*this == lpt);
		}
	};

	struct fRect 
	{
		fPoint lb;
		fPoint rt;

		inline void Merge(float x, float y ) 
		{
			fPoint point0(0,0);
			if (lb == point0 && rt == point0)
			{
				lb = rt = fPoint(x,y);
			}
			else
			{
				lb.x = min(lb.x,x);
				rt.x = max(rt.x,x);
				lb.y = min(lb.y,y);
				rt.y = max(rt.y,y);
			}
		}

		inline float Height(void) const
		{
			return fabs(rt.y-lb.y);
		}

		inline float Width(void) const
		{
			return fabs(rt.x - lb.x);
		}
	};

	struct dbfPoint 
	{
		double x,y;

		dbfPoint(void)
		{
			x = 0;
			y = 0;
		}

		dbfPoint(double _x,double _y)
		{
			x = _x;
			y = _y;
		}

		inline bool operator == (dbfPoint lpt) const
		{
			return (SMT_EQUAL(x,lpt.x) &&SMT_EQUAL(y ,lpt.y));
		}

		inline bool operator != (dbfPoint lpt) const
		{
			return !(SMT_EQUAL(x,lpt.x) &&SMT_EQUAL(y ,lpt.y));
		}
	};

	struct dbf3DPoint 
	{
		double x,y,z;

		dbf3DPoint(void)
		{
			x = 0;
			y = 0;
			z = 0;
		}

		dbf3DPoint(double _x,double _y,double _z)
		{
			x = _x;
			y = _y;
			z = _z;
		}

		inline bool operator == (dbf3DPoint lpt) const
		{
			return (SMT_EQUAL(x,lpt.x) &&SMT_EQUAL(y ,lpt.y));
		}

		inline bool operator != (dbf3DPoint lpt) const
		{
			return !(SMT_EQUAL(x,lpt.x) &&SMT_EQUAL(y ,lpt.y));
		}
	};

	struct dbfRect 
	{
		dbfPoint lb;
		dbfPoint rt;

		inline void Merge(double x, double y )
		{
			dbfPoint point0(0,0);
			if (lb == point0 && rt == point0)
			{
				lb = rt = dbfPoint(x,y);
			}
			else
			{
				lb.x = min(lb.x,x);
				rt.x = max(rt.x,x);
				lb.y = min(lb.y,y);
				rt.y = max(rt.y,y);
			}
		}

		inline double Height(void) const
		{
			return fabs(rt.y-lb.y);
		}

		inline double Width(void) const
		{
			return fabs(rt.x - lb.x);
		}
	};

	//////////////////////////////////////////////////////////////////////////
	struct SmtTriangle 
	{
		long a,b,c;
		bool bDelete;

		SmtTriangle():bDelete(false)
		{
			a = b = c = -1;
		}
	};

	typedef SmtTriangle		Smt3DTriangle;

	//////////////////////////////////////////////////////////////////////////
	struct SmtTile
	{//不维护内存
		long	lID;
		long	lTileBufSize;
		char*	pTileBuf;
		long	lImageCode;
		bool	bVisible;

		fRect	rtTileRect;

		SmtTile():pTileBuf(NULL)
			,lTileBufSize(0)
			,lImageCode(-1)
			,lID(-1)
			,bVisible(true)
		{

		}
	};

	typedef SmtTile		SmtWSTile;

	//////////////////////////////////////////////////////////////////////////
	enum  SmtVarType
	{
		/** Simple 32bit integer */                   SmtInteger		= 0,
		/** List of 32bit integers */                 SmtIntegerList	= 1,
		/** Double Precision floating point */        SmtBool			= 2,        
		/** Double Precision floating point */        SmtReal			= 3,             
		/** List of doubles */                        SmtRealList		= 4,
		/** Unsigned char  */                         SmtByte			= 5,
		/** String of ASCII chars */                  SmtString			= 6,
		/** Array of strings */                       SmtStringList		= 7,
		/** Raw Binary data */                        SmtBinary			= 10,
		/** Date */                                   SmtDate			= 11,
		/** Time */                                   SmtTime			= 12,
		/** Date and Time */                          SmtDateTime		= 13,
        /** Unknown */                                SmtUnknown		=14
	} ;

	struct SmtVariant 
	{
		union
		{								//			内存方式
			int         iVal;			//4			****
			double      dbfVal;			//8			********
			bool        boolVal;		//1			*
			byte        byteVal;		//1			*
			char       *bstrVal;		//4			****

			struct {
				int     nCount;
				int     *paList;
			} iValList;					//8			********

			struct {
				int     nCount;
				double  *paList;
			} dbfValList;				//8			********

			struct {
				int     nCount;
				char    **paList;
			} bstrValList;				//8			********

			struct {
				int     nCount;
				byte   *paData;
			} blobVal;					//8			********

			struct {
				ushort Year;
				byte   Month;
				byte   Day;
				byte   Hour;
				byte   Minute;
				byte   Second;
				byte   TZFlag; /* 0=unknown, 1=localtime(ambiguous), 
								100=GMT, 104=GMT+1, 80=GMT-5, etc */
			} dateVal;					//8			********
		};
		varType			Vt;				//2			^^
		ushort			usReserved1;	//2			##
		ushort			usReserved2;	//2			##
		ushort			usReserved3;	//2			##
	};									//16		********######^^(增加6个字节ushort无需字节补齐)							
}

#endif //_SMT_BAS_STRUCT_H