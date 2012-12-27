#include "sta_calculator.h"

namespace Smt_StaCore
{
	SmtCalculator::SmtCalculator(void):m_strExpression("")
	{

	}

	SmtCalculator::~SmtCalculator(void)
	{
		//���
		Clear();
	}

	const SmtCalculator& SmtCalculator::operator =( const SmtCalculator&other)
	{
		return *this;
	}

	long SmtCalculator::ParseOperand(string &strOpernd,string strOperndName)
	{
		strOpernd = strOperndName;

		int			nPos1,nPos2;
		nPos1 = strOpernd.find('[');
		nPos2 = strOpernd.find(']');

		if (nPos1 != string::npos && nPos2 != string::npos)
			strOpernd = strOpernd.substr(nPos1+1,nPos2-nPos1-1);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	long SmtCalculator::GetOperandNames(vector<string> &vOperandNames)
	{
		vOperandNames.clear();

		SplitExpression();//���ֱ��ʽ

		for (int i = 0; i < m_vStrSplitExp.size();i++)
		{
			string strOperand = "";
			string strTmp = m_vStrSplitExp[i];

			if (strTmp.at(0) == '[')
			{
				if (SMT_ERR_NONE == ParseOperand(strOperand,strTmp))
					vOperandNames.push_back(strOperand);
			}
		}
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	//�����ʽ�Ƿ���ȷ
	long SmtCalculator::Check(const char* szExp)
	{
		if (strlen(szExp) < 1)
			return SMT_ERR_INVALID_PARAM;
		 
		return SMT_ERR_NONE;
	}

	long SmtCalculator::Run(SmtOperand &resVal)
	{
		if (m_strExpression == "")
			return SMT_ERR_FAILURE;
		
		while(!m_gcOperands.empty())
			m_gcOperands.pop();

		m_vStrPostfixSplitExp.clear();
		m_vStrSplitExp.clear();

		SplitExpression();//���ֱ��ʽ

		Postfix();//����׺���ʽת��Ϊ��׺���ʽ

		string strEle ; 	
		int index = 0;

		while ( strEle = m_vStrPostfixSplitExp[index] , strEle != "=" )
		{
			if (strEle == "+" || strEle == "-" || strEle == "*" || strEle == "/"|| strEle == "^"|| strEle == "#")
			{//��������ִ�м���
				DoOperator (strEle[0]) ;
			}
			else
			{//������
				SmtOperand operVal;
				if (strEle.at(0) == '[')
				{//ʵ����������
					string strOperand;
					double *dbfValPtr = NULL;
					long   lNum = 0;
			
					if (SMT_ERR_NONE == ParseOperand(strOperand,strEle) && SMT_ERR_NONE == GetOperValsByName(dbfValPtr,lNum,strOperand.c_str()))
						operVal.Attach(dbfValPtr,lNum);
				}
				else if (strEle.at(0) == 'e')
				{//��Ȼ��e��Ŀǰ˵����ȡln����������e�����ѹһ���ղ�������ջ
					;
				}
				else
				{//ʵ��������
					double *dbfValPtr = new double;
					*dbfValPtr = atof(strEle.c_str());
					m_vDbfPtrs.push_back(dbfValPtr);
					operVal.Attach(dbfValPtr,1);
				}

				AddOperand (operVal);//������������ջ��
			}

			index++;
		}

		//������
		SmtOperand operValRes = m_gcOperands.top();
		resVal.SetReals(operValRes.GetRealsPtr(),operValRes.GetRealNum());

		return SMT_ERR_NONE;
	}

	long SmtCalculator::Clear(void)
	{//�ÿ�
		while(!m_gcOperands.empty())
			m_gcOperands.pop();

		m_vStrPostfixSplitExp.clear();
		m_vStrSplitExp.clear();
		
		//�ͷ��ڴ�
		vector<double*>::iterator iterDbfPtr = m_vDbfPtrs.begin();
		while (iterDbfPtr != m_vDbfPtrs.end())
		{	
			SMT_SAFE_DELETE(*iterDbfPtr);
			iterDbfPtr ++;
		}	
		m_vDbfPtrs.clear();

		//�ͷ��ڴ�
		map<string,double *>::iterator iterDbfValsPtr;

		iterDbfValsPtr = m_nameToOperdbfValsPtrs.begin();
		while (iterDbfValsPtr != m_nameToOperdbfValsPtrs.end())
		{
			SMT_SAFE_DELETE_A(iterDbfValsPtr->second);
			iterDbfValsPtr++;
		}
		m_nameToOperdbfValsPtrs.clear();

		m_nameToOperdbfValsNums.clear();

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	//��Ӳ�����
	long SmtCalculator::AddOperValsByName(double * &pDbfVals ,long lNum,const char*szName)
	{
		map<string,double *>::iterator iter;
		iter = m_nameToOperdbfValsPtrs.find(szName);
		if (iter != m_nameToOperdbfValsPtrs.end())
			return SMT_ERR_FAILURE;

		m_nameToOperdbfValsPtrs.insert(map<string,double *>::value_type(szName,pDbfVals));
		m_nameToOperdbfValsNums.insert(map<string,long>::value_type(szName,lNum));

		return SMT_ERR_NONE;
	}

	//��ȡ������
	long SmtCalculator::GetOperValsByName(double * &pDbfVals ,long &lNum,const char*szName)
	{
		map<string,double *>::iterator iter;
		iter = m_nameToOperdbfValsPtrs.find(szName);
		if (iter == m_nameToOperdbfValsPtrs.end())
			return SMT_ERR_FAILURE;

		pDbfVals = iter->second;
		lNum = m_nameToOperdbfValsNums[szName];

		return SMT_ERR_NONE;
	}

	//�Ƴ�������
	long SmtCalculator::RemoveOperValsByName(double * &pDbfVals ,long lNum,const char*szName)
	{
		map<string,double *>::iterator iter;
		iter = m_nameToOperdbfValsPtrs.find(szName);
		if (iter == m_nameToOperdbfValsPtrs.end())
			return SMT_ERR_FAILURE;

		pDbfVals = iter->second;
		lNum = m_nameToOperdbfValsNums[szName];
		m_nameToOperdbfValsPtrs.erase(iter);

		map<string,long>::iterator iter1;
		iter1 = m_nameToOperdbfValsNums.find(szName);
		m_nameToOperdbfValsNums.erase(iter1);

		return SMT_ERR_NONE;
	}

	//ɾ��������
	long SmtCalculator::DeleteOperValsByName(const char*szName)
	{
		map<string,double *>::iterator iter;
		iter = m_nameToOperdbfValsPtrs.find(szName);
		if (iter == m_nameToOperdbfValsPtrs.end())
			return SMT_ERR_FAILURE;

		SMT_SAFE_DELETE_A(iter->second);
		m_nameToOperdbfValsPtrs.erase(iter);
		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtCalculator::SplitExpression()
	{
		if (m_strExpression == "")
			return;

		m_vStrSplitExp.clear();

		string	strExp = m_strExpression + "=";
		string  strTmp = "";
		char	ch;
		int index = 0;
		while(ch = strExp[index],index < strExp.length())
		{
			if (ch == '[')
			{//����������
				strTmp = "";
				for (;ch != ']' && index < strExp.length();index++)
				{
					ch = strExp[index];
					strTmp += ch;
				}
			
				m_vStrSplitExp.push_back(strTmp);
			}
			else if (ch == '{')
			{//#���log
				strTmp = "";
				char szBuf[10];
				char oper = ' ';
				for (;ch != '}' && index < strExp.length();index++)
				{
					ch = strExp[index];
					strTmp += ch;
				}

				strTmp = strTmp.substr(1,strTmp.length()-2);

				strcpy(szBuf,strTmp.c_str());
				strTmp = strlwr(szBuf);
				
				if (strTmp == "log")
				{
					oper = '#';
					strTmp = "";
					strTmp += oper;
					m_vStrSplitExp.push_back(strTmp);
				}
				else if (strTmp == "ln")
				{
					oper = '#';
					strTmp = "";
					strTmp += oper;
					m_vStrSplitExp.push_back(strTmp);

					//����Ȼ��e
					m_vStrSplitExp.push_back("e");
				}
			}
			else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^'||ch == '#'||
				ch == '=' || ch == '(' ||ch == ')')
			{//���� #����ȡ����
				strTmp = "";
				strTmp += ch;
				m_vStrSplitExp.push_back(strTmp);
				index ++;
			}
			else if (ch >= '0' && ch <= '9' || ch == 'e')
			{//����,e��ʾ��Ȼ��e
				strTmp = "";
				for (; (ch >= '0' && ch <= '9' || ch == '.' || ch == 'e') && index < strExp.length();index++ )
				{
					ch = strExp[index];
					strTmp += ch;
				}
				
				strTmp = strTmp.substr(0,strTmp.length()-1);
				index --;
				m_vStrSplitExp.push_back(strTmp);
			}
			else//����
				ch = strExp[++index];
		}
	}
 
	int SmtCalculator::Isp( char ch )
	{//ջ��������
		int s=0;
		switch (ch)
		{
		case '=' : s = 0 ;break;
		case '(' : s = 1 ;break;
		case '#' : s = 8 ;break;
		case '^' : s = 7 ;break;
		case '*' : 
		case '/' : 
		case '%' : s = 5;break;
		case '+' : 
		case '-' : s = 3 ;break;
		case ')' : s = 9 ;break;
		}
		return s;
	}

	int SmtCalculator::Icp( char ch )
	{//ջ��������
		int s=0;
		switch (ch)
		{
		case '=' : s = 0 ;break;
		case '(' : s = 9 ;break;
		case '#' : s = 8 ;break;
		case '^' : s = 7 ;break;
		case '*' : 
		case '/' : 
		case '%' : s = 4;break;
		case '+' : 
		case '-' : s = 2 ;break;
		case ')' : s = 1 ;
			break;
		}
		return s;
	}

	void SmtCalculator::Postfix ()
	{//����׺���ʽת��Ϊ��׺���ʽ

		//����ջ�Ķ���s����Ԫ��Ϊ�ַ���
		stack <string,vector<string>> stkEle; 
		string strEle , strPopEle;

		//���
		while(!stkEle.empty())
			stkEle.pop();

		stkEle.push("=");//ջ�׷���һ�� =
		
		int i = 0;
		for (int i = 0; i < m_vStrSplitExp.size() && strEle != "=";i++)
		{
			strEle = m_vStrSplitExp[i];
			if (strEle == "+" || strEle == "-" || strEle == "*" || strEle == "/" ||strEle == "^" || strEle == "#" ||strEle == "(")
			{//������
				if (!stkEle.empty())
				{	
					strPopEle = stkEle.top();
					stkEle.pop();
					for ( ;!stkEle.empty() && Isp( strPopEle[0] ) > Icp (strEle[0]); strPopEle = stkEle.top(),stkEle.pop() ) 
						m_vStrPostfixSplitExp.push_back(strPopEle);//ispջ����������icp��ջ��������
				}
				
				stkEle.push( strPopEle ); 
				stkEle.push( strEle );//�ճ�ջ��y�ٽ�ջ
			}
			else if (strEle == ")")
			{// ( ���� )
				if (!stkEle.empty())
				{
					strPopEle = stkEle.top();
					stkEle.pop();
					for ( ;!stkEle.empty() &&strPopEle != "("; strPopEle = stkEle.top(),stkEle.pop() ) 
						m_vStrPostfixSplitExp.push_back(strPopEle);//�������strPopEle����ջ
				}
			}
			else if (strEle != "=")
			{//������
				m_vStrPostfixSplitExp.push_back(strEle);//�������strPopEle
			}
		}
		
		while ( !stkEle.empty() ) 
		{ 
			strPopEle = stkEle.top();
			stkEle.pop();
			m_vStrPostfixSplitExp.push_back(strPopEle);
		}
	}

	void SmtCalculator::AddOperand (SmtOperand &operVal )
	{//���������ջ
		m_gcOperands.push ( operVal );
	}

	//��ջ���˳�����������
	bool SmtCalculator::Get2Operands(SmtOperand &left,SmtOperand &right)
	{
		//���ջ�Ƿ��
		if ( m_gcOperands.empty() )
			return false; 
		
		right = m_gcOperands.top();//ȡ�Ҳ�����
		m_gcOperands.pop();

		left = m_gcOperands.top();//ȡ�������

		//���ջ�Ƿ��
		if (m_gcOperands.empty())
			return false; 

		m_gcOperands.pop();
		
		return true ;
	}

	void SmtCalculator::DoOperator(char op)
	{//�γ�����ָ���������
		SmtOperand left , right ;//ȡ����������
		if ( Get2Operands (left,right))
		{//��ȡ���ɹ�,��������
			switch (op)
			{
			case '+': 
				//��
				m_gcOperands.push (left + right); 
				break ;
			case '-':  
				//��
				m_gcOperands.push (left - right); 
				break ;
			case '*':
				//��
				m_gcOperands.push (left * right); 
				break ;
			case '/': 
				//��
				m_gcOperands.push (left / right); 
				break ;
			case '^': 
				//��
				m_gcOperands.push (left ^ right); 
				break ;
			case '#': 
				//����log
				{
					double * dbfPtr = right.GetRealsPtr();
					
					if (1 == right.GetRealNum())
					{
						double dbfValue = *dbfPtr;
						left.LogX(dbfValue);
					}
					else if (0 == right.GetRealNum())
					{//�ղ�������˵����ȡ��Ȼ����
						left.Ln();
					}
					
					m_gcOperands.push (left); 
					break ;
				}
			}
		}
		
		else 
			Clear ();//������0
	}
}