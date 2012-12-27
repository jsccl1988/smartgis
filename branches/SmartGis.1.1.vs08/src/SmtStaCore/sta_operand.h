/*
File:    sta_operand.h

Desc:    SmartGis 操作数

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.22

Copyright (c) 2010 CCL. All rights reserved.
*/

#ifndef _STA_OPERAND_H
#define _STA_OPERAND_H

#include "smt_core.h"
#include "sta_bas_struct.h"
#include <cmath>
#include <stack>
#include <sstream>

namespace Smt_StaCore
{
	/*
	说明：	自己不负责内存管理
	必须使用attach和detach(detach必须显式调用)
	*/
	class SMT_EXPORT_CLASS SmtOperand
	{
	public:

		SmtOperand(void);
		SmtOperand(double *pDbfs,long lNum); 

		~SmtOperand();

	public:
		const SmtOperand&				operator =( const SmtOperand&other);

	public:
		bool							operator==(const SmtOperand&other);

	public:
		//失败原因是未detach已有对象，必须显式detach已有对象
		long							Attach (double *pDbfs,long lNum);

		//失败原因是未attach对象
		long							Dettach (double *&pDbfs,long &lNum);

	public:
		inline long						GetRealNum(void) const {return m_lRealNum;}
		inline double					*GetRealsPtr(void) const {return m_pDbfValues;}

		inline long						SetReal(long lIndex,double dbfValue)
		{
			if (!m_pDbfValues || lIndex < 0 || lIndex >= m_lRealNum)
				return SMT_ERR_INVALID_PARAM;

			m_pDbfValues[lIndex] = dbfValue;

			return SMT_ERR_NONE;
		}

		inline long						GetReal(long lIndex,double &dbfValue) const
		{
			if (!m_pDbfValues || lIndex < 0 || lIndex >= m_lRealNum)
				return SMT_ERR_INVALID_PARAM;

			dbfValue = m_pDbfValues[lIndex];

			return SMT_ERR_NONE;
		}

		inline long						SetReals(double *pDbfs,long lNum)
		{
			if( m_bAttached && pDbfs != m_pDbfValues && lNum  ==  m_lRealNum)
			{
				memcpy(m_pDbfValues,pDbfs,m_lRealNum*sizeof(double));
				return SMT_ERR_NONE;
			}
			else
				return SMT_ERR_INVALID_PARAM;
		}

		void							ResetReal(double dbfValue);

	public:
		//operator
		SmtOperand						&operator +(SmtOperand& rightHand);
		SmtOperand						&operator -(SmtOperand& rightHand);
		SmtOperand						&Max(SmtOperand& rightHand);			//对应位置取大值，当右操作数为一个实数时，则左操作数与该实数做运算
		SmtOperand						&Min(SmtOperand& rightHand);			//对应位置取小值，当右操作数为一个实数时，则左操作数与该实数做运算
		SmtOperand						&operator *(SmtOperand& rightHand);	//对应位置相乘，当右操作数为一个实数时，则左操作数与该实数做运算
		SmtOperand						&operator /(SmtOperand& rightHand);	//对应位置相除，当右操作数为一个实数时，则左操作数与该实数做运算

		SmtOperand						&operator ^(SmtOperand& rightHand);	//取幂，右操作数必须为一个实数

		SmtOperand						&Ln(void);								//求自然对数
		SmtOperand						&Log10(void);							//求log10
		SmtOperand						&LogX(double dbfValue);					//求logX
		SmtOperand						&Power(double dbfValue);				//求幂

		SmtOperand						&Sin(void);								//正弦
		SmtOperand						&Cos(void);								//余弦
		SmtOperand						&Tan(void);								//正切

		SmtOperand						&ASin(void);							//反正弦
		SmtOperand						&ACos(void);							//反余弦
		SmtOperand						&ATan(void);							//反正切

		SmtOperand						&StdByMaxMin(void);						//极差标准化
		SmtOperand						&StdByDeviation(void);					//标准差标准化
		SmtOperand						&StdBySum(void);						//总和标准化
		SmtOperand						&StdByMax(void);						//极大值标准化
		SmtOperand						&StdByMin(void);						//极小值标准化

		SmtOperand						&RealCvt(long lMtd);					//数据转换
		void							RealSta(SmtStaticResult &rs,long lStaType);				//数据统计

	protected:
		long							m_lRealNum;
		double							*m_pDbfValues;
		bool							m_bAttached;
	};

	typedef								SmtOperand* 			SmtOperandPtr;
}

#if !defined(Export_SmtStaCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtStaCoreD.lib")
#       else
#          pragma comment(lib,"SmtStaCore.lib")
#	    endif  
#endif

#endif //_STA_OPERAND_H