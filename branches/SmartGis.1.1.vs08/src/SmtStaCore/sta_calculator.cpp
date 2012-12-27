#include "sta_calculator.h"

namespace Smt_StaCore
{
	SmtCalculator::SmtCalculator(void):m_strExpression("")
	{

	}

	SmtCalculator::~SmtCalculator(void)
	{
		//清除
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

		SplitExpression();//划分表达式

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
	//检查表达式是否正确
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

		SplitExpression();//划分表达式

		Postfix();//将中缀表达式转化为后缀表达式

		string strEle ; 	
		int index = 0;

		while ( strEle = m_vStrPostfixSplitExp[index] , strEle != "=" )
		{
			if (strEle == "+" || strEle == "-" || strEle == "*" || strEle == "/"|| strEle == "^"|| strEle == "#")
			{//操作符，执行计算
				DoOperator (strEle[0]) ;
			}
			else
			{//操作数
				SmtOperand operVal;
				if (strEle.at(0) == '[')
				{//实数集操作数
					string strOperand;
					double *dbfValPtr = NULL;
					long   lNum = 0;
			
					if (SMT_ERR_NONE == ParseOperand(strOperand,strEle) && SMT_ERR_NONE == GetOperValsByName(dbfValPtr,lNum,strOperand.c_str()))
						operVal.Attach(dbfValPtr,lNum);
				}
				else if (strEle.at(0) == 'e')
				{//自然数e，目前说明是取ln操作，补的e，因此压一个空操作数入栈
					;
				}
				else
				{//实数操作数
					double *dbfValPtr = new double;
					*dbfValPtr = atof(strEle.c_str());
					m_vDbfPtrs.push_back(dbfValPtr);
					operVal.Attach(dbfValPtr,1);
				}

				AddOperand (operVal);//将操作数放入栈中
			}

			index++;
		}

		//输出结果
		SmtOperand operValRes = m_gcOperands.top();
		resVal.SetReals(operValRes.GetRealsPtr(),operValRes.GetRealNum());

		return SMT_ERR_NONE;
	}

	long SmtCalculator::Clear(void)
	{//置空
		while(!m_gcOperands.empty())
			m_gcOperands.pop();

		m_vStrPostfixSplitExp.clear();
		m_vStrSplitExp.clear();
		
		//释放内存
		vector<double*>::iterator iterDbfPtr = m_vDbfPtrs.begin();
		while (iterDbfPtr != m_vDbfPtrs.end())
		{	
			SMT_SAFE_DELETE(*iterDbfPtr);
			iterDbfPtr ++;
		}	
		m_vDbfPtrs.clear();

		//释放内存
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
	//添加操作数
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

	//获取操作数
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

	//移除操作数
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

	//删除操作数
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
			{//操作数名词
				strTmp = "";
				for (;ch != ']' && index < strExp.length();index++)
				{
					ch = strExp[index];
					strTmp += ch;
				}
			
				m_vStrSplitExp.push_back(strTmp);
			}
			else if (ch == '{')
			{//#替代log
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

					//补自然数e
					m_vStrSplitExp.push_back("e");
				}
			}
			else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '^'||ch == '#'||
				ch == '=' || ch == '(' ||ch == ')')
			{//符号 #代替取对数
				strTmp = "";
				strTmp += ch;
				m_vStrSplitExp.push_back(strTmp);
				index ++;
			}
			else if (ch >= '0' && ch <= '9' || ch == 'e')
			{//数字,e表示自然数e
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
			else//过滤
				ch = strExp[++index];
		}
	}
 
	int SmtCalculator::Isp( char ch )
	{//栈内优先数
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
	{//栈外优先数
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
	{//将中缀表达式转化为后缀表达式

		//定义栈的对象s，其元素为字符串
		stack <string,vector<string>> stkEle; 
		string strEle , strPopEle;

		//清除
		while(!stkEle.empty())
			stkEle.pop();

		stkEle.push("=");//栈底放了一个 =
		
		int i = 0;
		for (int i = 0; i < m_vStrSplitExp.size() && strEle != "=";i++)
		{
			strEle = m_vStrSplitExp[i];
			if (strEle == "+" || strEle == "-" || strEle == "*" || strEle == "/" ||strEle == "^" || strEle == "#" ||strEle == "(")
			{//操作符
				if (!stkEle.empty())
				{	
					strPopEle = stkEle.top();
					stkEle.pop();
					for ( ;!stkEle.empty() && Isp( strPopEle[0] ) > Icp (strEle[0]); strPopEle = stkEle.top(),stkEle.pop() ) 
						m_vStrPostfixSplitExp.push_back(strPopEle);//isp栈内优先数，icp是栈外优先数
				}
				
				stkEle.push( strPopEle ); 
				stkEle.push( strEle );//刚除栈的y再进栈
			}
			else if (strEle == ")")
			{// ( 或者 )
				if (!stkEle.empty())
				{
					strPopEle = stkEle.top();
					stkEle.pop();
					for ( ;!stkEle.empty() &&strPopEle != "("; strPopEle = stkEle.top(),stkEle.pop() ) 
						m_vStrPostfixSplitExp.push_back(strPopEle);//持续输出strPopEle，退栈
				}
			}
			else if (strEle != "=")
			{//操作数
				m_vStrPostfixSplitExp.push_back(strEle);//持续输出strPopEle
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
	{//进入操作数栈
		m_gcOperands.push ( operVal );
	}

	//从栈中退出两个操作数
	bool SmtCalculator::Get2Operands(SmtOperand &left,SmtOperand &right)
	{
		//检查栈是否空
		if ( m_gcOperands.empty() )
			return false; 
		
		right = m_gcOperands.top();//取右操作数
		m_gcOperands.pop();

		left = m_gcOperands.top();//取左操作数

		//检查栈是否空
		if (m_gcOperands.empty())
			return false; 

		m_gcOperands.pop();
		
		return true ;
	}

	void SmtCalculator::DoOperator(char op)
	{//形成运算指令，进行运算
		SmtOperand left , right ;//取两个操作数
		if ( Get2Operands (left,right))
		{//若取数成功,进行运算
			switch (op)
			{
			case '+': 
				//加
				m_gcOperands.push (left + right); 
				break ;
			case '-':  
				//减
				m_gcOperands.push (left - right); 
				break ;
			case '*':
				//乘
				m_gcOperands.push (left * right); 
				break ;
			case '/': 
				//除
				m_gcOperands.push (left / right); 
				break ;
			case '^': 
				//幂
				m_gcOperands.push (left ^ right); 
				break ;
			case '#': 
				//代替log
				{
					double * dbfPtr = right.GetRealsPtr();
					
					if (1 == right.GetRealNum())
					{
						double dbfValue = *dbfPtr;
						left.LogX(dbfValue);
					}
					else if (0 == right.GetRealNum())
					{//空操作数，说明是取自然对数
						left.Ln();
					}
					
					m_gcOperands.push (left); 
					break ;
				}
			}
		}
		
		else 
			Clear ();//出错清0
	}
}