#include "sta_calculator.h"
#include "sta_api.h"
#include "sta_math_api.h"

namespace Smt_StaCore
{
	SmtOperand::SmtOperand(void):m_pDbfValues(NULL)
		,m_lRealNum(0)
		,m_bAttached(false)
	{

	}

	SmtOperand::SmtOperand(double *pDbfs,long lNum):m_pDbfValues(NULL)
		,m_lRealNum(0)
		,m_bAttached(false)
	{
		Attach(pDbfs,lNum);
	}

	SmtOperand::~SmtOperand()
	{
		if(m_bAttached)
		{
			m_pDbfValues = NULL;
			m_lRealNum = 0;
			m_bAttached = false;
		}
	}

	const SmtOperand & SmtOperand::operator = ( const SmtOperand &other)
	{
		if (!((*this) == other)) 
			this->Attach(other.GetRealsPtr(),other.GetRealNum());

		return *this;
	}

	bool SmtOperand::operator==(const SmtOperand &other)
	{
		return (this == &other);
	}
	//////////////////////////////////////////////////////////////////////////
	long SmtOperand::Attach (double *pDbfs,long	lNum)
	{
		if( !m_bAttached && pDbfs && lNum > 0)
		{
			m_pDbfValues = pDbfs;
			m_lRealNum = lNum;

			m_bAttached = true;
			return SMT_ERR_NONE;
		}
		else
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	//失败原因是未attach对象
	long SmtOperand::Dettach (double *&pDbfs,long	&lNum)
	{
		if(m_bAttached)
		{
			pDbfs = m_pDbfValues;
			m_pDbfValues = NULL;

			lNum = m_lRealNum;
			m_lRealNum = 0;

			m_bAttached = false;
			return SMT_ERR_NONE;
		}
		else
		{
			return SMT_ERR_FAILURE;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtOperand::ResetReal(double dbfValue)
	{
		if (!m_pDbfValues || m_lRealNum < 1)
			return;

		for (int i = 0; i < m_lRealNum ; i++)
			m_pDbfValues[i] = dbfValue;
	}

	SmtOperand & SmtOperand::operator +( SmtOperand& rightHand)
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] += dbfPtr[i];
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] += dbfValue;
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::operator -(SmtOperand &rightHand)
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] -= dbfPtr[i];
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] -= dbfValue;
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::Max(SmtOperand& rightHand)			//对应位置取大值
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] = max(dbfPtr[i],m_pDbfValues[i]);
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] = max(dbfValue,m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::Min(SmtOperand& rightHand)			//对应位置取小值
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] = min(dbfPtr[i],m_pDbfValues[i]);
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] = min(dbfValue,m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::operator *(SmtOperand &rightHand)
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] *= dbfPtr[i];
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				for (int i = 0; i < m_lRealNum ; i++)
					m_pDbfValues[i] *= dbfValue;
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::operator /(SmtOperand &rightHand)
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (m_lRealNum == rightHand.GetRealNum())
			{
				for (int i = 0; i < m_lRealNum ; i++)
				{
					if (dbfPtr[i] != 0)
						m_pDbfValues[i] /= dbfPtr[i];
				}
			}
			else if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				if (dbfValue != 0)
				{
					for (int i = 0; i < m_lRealNum ; i++)
						m_pDbfValues[i]/= dbfValue;
				}	
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::operator ^(SmtOperand &rightHand)
	{
		double * dbfPtr = rightHand.GetRealsPtr();
		if (m_pDbfValues && m_lRealNum >0 )
		{	
			if (1 == rightHand.GetRealNum())
			{
				double dbfValue = *dbfPtr;
				Power(dbfValue);
			}
		}

		return *this;
	}

	SmtOperand& SmtOperand::Ln(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			double dbfMin = 0.;
			SmtMin(dbfMin,m_pDbfValues,m_lRealNum);

			/*
			适用于服从对数正态分布的数据，由于这类数据分布是
			最偏斜的，很可能出现0 值，当取对数时，这些值可能
			呈大的负值，为了避免这个缺点，故在取对数前首先对
			所有数据加上一个常数。X =  log(X+ C)
			*/
			if (dbfMin < 1)
				dbfMin = fabs(dbfMin)+1;
			else
				dbfMin = 1;

			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = log(m_pDbfValues[i]+dbfMin);
			}
		}
		return *this;
	}

	SmtOperand & SmtOperand::Log10(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			double dbfMin = 0.;
			SmtMin(dbfMin,m_pDbfValues,m_lRealNum);

			/*
			适用于服从对数正态分布的数据，由于这类数据分布是
			最偏斜的，很可能出现0 值，当取对数时，这些值可能
			呈大的负值，为了避免这个缺点，故在取对数前首先对
			所有数据加上一个常数。X =  log(X+ C)
			*/
			if (dbfMin < 1)
				dbfMin = fabs(dbfMin)+1;
			else
				dbfMin = 1;

			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = log10(m_pDbfValues[i]/*+dbfMin*/);
			}
		}

		return *this;
	}

	SmtOperand& SmtOperand::LogX(double dbfValue)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0 && !SmtIsRealEqual(m_pDbfValues[i],1.))
					m_pDbfValues[i] = log(m_pDbfValues[i])/log(dbfValue);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::Power(double dbfValue)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
				m_pDbfValues[i] = pow(m_pDbfValues[i],dbfValue);
		}

		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	SmtOperand & SmtOperand::Cos(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = cos(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::Sin(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = sin(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::Tan(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = tan(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::ASin(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = asin(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::ACos(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = acos(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::ATan(void)
	{
		if (m_pDbfValues && m_lRealNum >0)
		{	
			for (int i = 0; i < m_lRealNum ; i++)
			{
				if(m_pDbfValues[i] > 0)
					m_pDbfValues[i] = atan(m_pDbfValues[i]);
			}
		}

		return *this;
	}

	SmtOperand & SmtOperand::StdByMaxMin(void)						//极差标准化
	{
		SmtStdByMaxMin(m_pDbfValues,m_lRealNum);

		return *this;
	}

	SmtOperand& SmtOperand::StdByDeviation(void)					//标准差标准化
	{
		SmtStdByDeviation(m_pDbfValues,m_lRealNum);

		return *this;
	}

	SmtOperand& SmtOperand::StdBySum(void)						//总和标准化
	{
		SmtStdBySum(m_pDbfValues,m_lRealNum);

		return *this;
	}

	SmtOperand& SmtOperand::StdByMax(void)						//极大值标准化
	{
		SmtStdByMaxVal(m_pDbfValues,m_lRealNum);

		return *this;
	}

	SmtOperand& SmtOperand::StdByMin(void)						//极小值标准化
	{
		SmtStdByMinVal(m_pDbfValues,m_lRealNum);

		return *this;
	}

	SmtOperand& SmtOperand::RealCvt(long lMtd)
	{
		switch (lMtd)
		{
		case DTRT_STD_SUM:
			return StdBySum();
			
		case DTRT_STD_MAX:
			return StdByMax();

		case DTRT_STD_MIN:
			return StdByMin();

		case DTRT_STD_DEVIATION:
			return StdByDeviation();

		case DTRT_STD_MAXMIN:
			return StdByMaxMin();

		case DTRT_LN:
			return Ln();

		case DTRT_LOG10:
			return Log10();

		case DTRT_SIN:
			return Sin();

		case DTRT_COS:
			return Cos();

		case DTRT_TAN:
			return Tan();

		case DTRT_ASIN:
			return ASin();

		case DTRT_ACOS:
			return ACos();

		case DTRT_ATAN:
			return ATan();

		default:
			return *this;
		}
	}

	void SmtOperand::RealSta(SmtStaticResult &rs,long lStaType)
	{
		rs.lStaDataNum = m_lRealNum;

		if (lStaType & DTSTA_MAX)
		{
			SmtMax(rs.dbfMax,m_pDbfValues,m_lRealNum);
		}

		if (lStaType & DTSTA_MIN)
		{
			SmtMin(rs.dbfMin,m_pDbfValues,m_lRealNum);
		}

		if (lStaType & DTSTA_AVG)
		{
			SmtAvg(rs.dbfAvg,m_pDbfValues,m_lRealNum);
		}

		if (lStaType & DTSTA_MEDIAN)
		{
			SmtMedian(rs.dbfMedian,m_pDbfValues,m_lRealNum);
		}

		if (lStaType & DTSTA_STDDEV)
		{
			SmtStdDeviation(rs.dbfStdDeviation,m_pDbfValues,m_lRealNum);
		}
		
		if (lStaType & DTSTA_VARCOEF)
		{
			//SmtMedian(dbfVarCoef,vVals);
		}

		if (lStaType & DTSTA_VARIANCE)
		{
			SmtVariance(rs.dbfVariance,m_pDbfValues,m_lRealNum);
		}
	}
}