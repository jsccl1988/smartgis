/*
File:    sta_operand.h

Desc:    SmartGis ������

Version: Version 1.0

Writter:  �´���

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
	˵����	�Լ��������ڴ����
	����ʹ��attach��detach(detach������ʽ����)
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
		//ʧ��ԭ����δdetach���ж��󣬱�����ʽdetach���ж���
		long							Attach (double *pDbfs,long lNum);

		//ʧ��ԭ����δattach����
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
		SmtOperand						&Max(SmtOperand& rightHand);			//��Ӧλ��ȡ��ֵ�����Ҳ�����Ϊһ��ʵ��ʱ��������������ʵ��������
		SmtOperand						&Min(SmtOperand& rightHand);			//��Ӧλ��ȡСֵ�����Ҳ�����Ϊһ��ʵ��ʱ��������������ʵ��������
		SmtOperand						&operator *(SmtOperand& rightHand);	//��Ӧλ����ˣ����Ҳ�����Ϊһ��ʵ��ʱ��������������ʵ��������
		SmtOperand						&operator /(SmtOperand& rightHand);	//��Ӧλ����������Ҳ�����Ϊһ��ʵ��ʱ��������������ʵ��������

		SmtOperand						&operator ^(SmtOperand& rightHand);	//ȡ�ݣ��Ҳ���������Ϊһ��ʵ��

		SmtOperand						&Ln(void);								//����Ȼ����
		SmtOperand						&Log10(void);							//��log10
		SmtOperand						&LogX(double dbfValue);					//��logX
		SmtOperand						&Power(double dbfValue);				//����

		SmtOperand						&Sin(void);								//����
		SmtOperand						&Cos(void);								//����
		SmtOperand						&Tan(void);								//����

		SmtOperand						&ASin(void);							//������
		SmtOperand						&ACos(void);							//������
		SmtOperand						&ATan(void);							//������

		SmtOperand						&StdByMaxMin(void);						//�����׼��
		SmtOperand						&StdByDeviation(void);					//��׼���׼��
		SmtOperand						&StdBySum(void);						//�ܺͱ�׼��
		SmtOperand						&StdByMax(void);						//����ֵ��׼��
		SmtOperand						&StdByMin(void);						//��Сֵ��׼��

		SmtOperand						&RealCvt(long lMtd);					//����ת��
		void							RealSta(SmtStaticResult &rs,long lStaType);				//����ͳ��

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