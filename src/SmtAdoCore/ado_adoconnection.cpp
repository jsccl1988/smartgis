#include "ado_adoconnection.h"
#include "smt_bas_struct.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

bool vartobool(const _variant_t& var)
{
	bool value = false;
	switch (var.vt)
	{
	case VT_BOOL:
		value = var.boolVal ? true : false;
	case VT_EMPTY:
	case VT_NULL:
		break;
	default:
		;
	//	TRACE(_T("Warning: 未处理的 _variant_t 类型值; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
	}
	return value;
}

BYTE vartoby(const _variant_t& var)
{
	BYTE value = 0;
	switch (var.vt)
	{
	case VT_I1:
	case VT_UI1:
		value = var.bVal;
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		;
	//	TRACE(_T("Warning: 未处理的 _variant_t 类型值; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
	}	
	return value;
}

short vartoi(const _variant_t& var)
{
	short value = 0;
	switch (var.vt)
	{
	case VT_BOOL:
		value = var.boolVal;
		break;
	case VT_UI1:
	case VT_I1:
		value = var.bVal;
		break;
	case VT_I2:
	case VT_UI2:
		value = var.iVal;
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		;
	//	TRACE(_T("Warning: 未处理的 _variant_t 类型值; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
	}	
	return value;
}

long vartol(const _variant_t& var)
{
	long value = 0;
	switch (var.vt)
	{
	case VT_BOOL:
		value = var.boolVal;
		break;
	case VT_UI1:
	case VT_I1:
		value = var.bVal;
		break;
	case VT_UI2:
	case VT_I2:
		value = var.iVal;
		break;
	case VT_I4:
	case VT_UI4:
		value = var.lVal;
		break;
	case VT_INT:
		value = var.intVal;
		break;
	case VT_R4:
		value = (long)(var.fltVal + 0.5);
		break;
	case VT_R8:
		value = (long)(var.dblVal + 0.5);
		break;
	case VT_DECIMAL:
		value = (long)var;
		break;
	case VT_CY:
		value = (long)var;
		break;
	case VT_BSTR://字符串
	case VT_LPSTR://字符串
	case VT_LPWSTR://字符串
		value = atol((LPCTSTR)(_bstr_t)var);
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		;
		//TRACE(_T("Warning: 未处理的 _variant_t 类型值; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
	}
	return value;
}

double vartof(const _variant_t& var)
{
	double value = 0;
	switch (var.vt)
	{
	case VT_R4:
		value = var.fltVal;
		break;
	case VT_R8:
		value = var.dblVal;
		break;
	case VT_DECIMAL:
		value = (double)var;
		break;
	case VT_CY:
		value = (double)var;
		break;
	case VT_BOOL:
		value = var.boolVal;
		break;
	case VT_UI1:
	case VT_I1:
		value = var.bVal;
		break;
	case VT_UI2:
	case VT_I2:
		value = var.iVal;
		break;
	case VT_UI4:
	case VT_I4:
		value = var.lVal;
		break;
	case VT_INT:
		value = var.intVal;
		break;
	case VT_BSTR://字符串
	case VT_LPSTR://字符串
	case VT_LPWSTR://字符串
		value = atof((LPCTSTR)(_bstr_t)var);
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		value = 0;
	//	TRACE(_T("Warning: 未处理的 _variant_t 类型值; 文件: %s; 行: %d\n"), __FILE__, __LINE__);
	}
	return value;
}

const char * vartostr(const _variant_t &var)
{
	static char buf[TEMP_BUFFER_SIZE];
	memset(buf,'\0',TEMP_BUFFER_SIZE);

	switch (var.vt)
	{
	case VT_BSTR://字符串
	case VT_LPSTR://字符串
	case VT_LPWSTR://字符串
		sprintf(buf,(LPCTSTR)(_bstr_t)var);
		break;
	case VT_I1:
	case VT_UI1:
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.bVal);	 
		break;
	case VT_I2://短整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.iVal);
		break;
	case VT_UI2://无符号短整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.uiVal);
		break;
	case VT_INT://整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.intVal);
		break;
	case VT_I4: //整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.lVal);
		break;
	case VT_I8: //长整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.lVal);
		break;
	case VT_UINT://无符号整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.uintVal);
		break;
	case VT_UI4: //无符号整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.ulVal);
		break;
	case VT_UI8: //无符号长整型
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.ulVal);
		break;
	case VT_VOID:
		snprintf( buf, TEMP_BUFFER_SIZE, "%8x", var.byref);
		break;
	case VT_R4://浮点型
		snprintf( buf, TEMP_BUFFER_SIZE, "%.4f", var.fltVal);
		break;
	case VT_R8://双精度型
		snprintf( buf, TEMP_BUFFER_SIZE, "%.8f", var.dblVal);
		break;
	case VT_DECIMAL: //小数
		snprintf( buf, TEMP_BUFFER_SIZE, "%.8f", (double)var);
		break;
	case VT_CY:
		{
			 
		}
		break;
	case VT_BLOB:
	case VT_BLOB_OBJECT:
	case 0x2011:
		sprintf(buf,"[BLOB]");
		break;
	case VT_BOOL://布尔型
        sprintf(buf,var.boolVal?"TRUE" : "FALSE");
		break;
	case VT_DATE: //日期型
		{
		 
		}
		break;
	case VT_NULL://NULL值
		 sprintf(buf,"");
		break;
	case VT_EMPTY://空
		 sprintf(buf,"");
		break;
	case VT_UNKNOWN://未知类型
	default:
		 sprintf(buf,"UN_KNOW");
		break;
	}
	return buf;
}


namespace Smt_Ado
{
	SmtAdoConnection::SmtAdoConnection()
	{
		m_strConnect = "N";
		//创建 Connection 对象---------------------------
		m_pConnection.CreateInstance("ADODB.Connection");
#ifdef _DEBUG
		if (m_pConnection == NULL)
		{
		//	AfxMessageBox("Connection 对象创建失败! 请确认是否初始化了COM环境\r\n");
		}
#endif
		assert(m_pConnection != NULL);

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_ADO_LOG);
		if (NULL == pLog)
		{
			pLogMgr->CreateLog(C_STR_ADO_LOG.c_str());
		}
	}

	SmtAdoConnection::~SmtAdoConnection()
	{
		if (m_pConnection != NULL)
		{
			Release();
		}
	}

	/*========================================================================
	Name:		连接到数据源.
	-----------------------------------------------------
	Params:		[lpszConnect]: 连接字符串, 包含连接信息.
	[lOptions]:	可选. 决定该方法是以同步还是异步的方式连接数据
	源. 可以是如下某个常量:
	[常量]					[说明] 
	----------------------------------
	adConnectUnspecified	(默认)同步方式打开连接. 
	adAsyncConnect			异步方式打开连接. Ado用 ConnectComplete 事
	件来通知已经完成连接. 
	==========================================================================*/
	BOOL SmtAdoConnection::Open(const char * lpszConnect, long lOptions)
	{
		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_ADO_LOG.c_str());

		assert(m_pConnection != NULL);
		if (strcmp(lpszConnect,"") == 0)
			return FALSE;
		 
		if (m_strConnect.length() == 0)
		{
			assert(FALSE);
			return FALSE;
		}

		if (IsOpen()) 
			Close();

		try
		{
			// 连接数据库 ---------------------------------------------
			if (pLog)
				pLog->LogMessage("连接:%s",lpszConnect);
			return (m_pConnection->Open(_bstr_t(lpszConnect), "", "", lOptions) == S_OK);
		}
		catch (_com_error e)
		{
			if (pLog)
				pLog->LogMessage("Warning: 连接数据库发生异常. 错误信息: %s; ",e.ErrorMessage());
		} 
		catch (...)
		{
			if (pLog)
				pLog->LogMessage("Warning: 连接数据库时发生未知错误:");
		}


		return FALSE;
	}

	/*========================================================================
	Name:	连接 Access 数据库. 
	-----------------------------------------------------
	Params:	
	[dbpath]:	Access路径.
	[username]:		用户名.
	[pass]:		密码.
	==========================================================================*/
	BOOL SmtAdoConnection::ConnectAccess(const char * dbpath, const char * username ,const char * pass, long lOptions)
	{
		string strConnect = "Provider=Microsoft.Jet.OLEDB.4.0";
		strConnect +=";Data Source =";
		strConnect += dbpath;
		if (strcmp(username, "") != 0)
		{
			strConnect += "Jet OLEDB:Database UserID=" ;
			strConnect += username;
		}
		if (strcmp(pass, "") != 0) 
		{
			strConnect += ";Jet OLEDB:Database Password=" ;
			strConnect += pass;
		}
		return Open(strConnect.c_str(), lOptions);
	}

	/*========================================================================
	Name:	连接 DSN. 
	-----------------------------------------------------
	Params:	
	[dsn]:		DSN名称
	[username]:		用户名.
	[pass]:		密码.
	==========================================================================*/
	BOOL SmtAdoConnection::ConnectDSN(const char * dsn,const char * username ,const char * pass, long lOptions)
	{
		string strConnect = "DSN=";
		strConnect += dsn;
		if (strcmp(username, "") != 0)
		{
			strConnect += ";uid=" ;
			strConnect += username;
		}
		if (strcmp(pass, "") != 0) 
		{
			strConnect += ";pwd=" ;
			strConnect += pass;
		}
	
		return Open(strConnect.c_str(), lOptions);
	}

	/*========================================================================
	Name:	连接 SQL SERVER 数据库. 
	-----------------------------------------------------
	Params:		
	[dbsrc]:	SQL SERVER 服务器名.
	[dbname]:	默认的数据库名.
	[username]:	用户名.
	[pass]:		密码.
	==========================================================================*/
	BOOL SmtAdoConnection::ConnectSqlServer(const char * dbsrc,const char * dbname, const char * username ,const char * pass, long lOptions)
	{
		string strConnect = "Provider=SQLOLEDB";
		strConnect +=";Data Source =";
		strConnect +=dbsrc;

		strConnect +=";Initial Catalog = ";
		strConnect +=dbname;

		if (strcmp(username, "") == 0 && strcmp(pass, "") == 0)
		{
			strConnect += ";Integrated Security=SSPI";
		}
		else if (strcmp(username, "") != 0)
		{
			strConnect += ";uid=" ;
			strConnect += username;
		}
		if (strcmp(pass, "") != 0) 
		{
			strConnect += ";pwd=" ;
			strConnect += pass;
		}

		return Open(strConnect.c_str(), lOptions);
	}

	void SmtAdoConnection::Close()
	{
		try
		{
			if (m_pConnection != NULL && IsOpen()) 
			{
				m_pConnection->Close();
			}
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: 关闭数据库发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		} 
	}

	/*========================================================================
	Name:	关闭连接并释放对象.
	-----------------------------------------------------
	Remarks: 关闭连接并释放connection对象.
	==========================================================================*/
	void SmtAdoConnection::Release()
	{
		if (IsOpen()) Close();
		m_pConnection.Release();
	}

	/*========================================================================
	Name:		执行指定的查询、SQL 语句、存储过程等.
	----------------------------------------------------------
	Remarks:	请参考 CAdoRecordSet 类的Open方法. 返回的 Recordset 对象始
	终为只读、仅向前的游标.
	==========================================================================*/
	_RecordsetPtr SmtAdoConnection::Execute(const char * lpszSQL, long lOptions)
	{
		assert(m_pConnection != NULL);
		if (strcmp(lpszSQL,"") == 0)
			return FALSE;

		try
		{
			return m_pConnection->Execute(_bstr_t(lpszSQL), NULL, lOptions);
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: Execute 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Remarks:	请参考 CAdoRecordSet 类 Cancel 方法.
	==========================================================================*/
	BOOL SmtAdoConnection::Cancel()
	{
		assert(m_pConnection != NULL);
		try
		{
			return m_pConnection->Cancel();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: Cancel 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		取得 Connection 对象提供者的名称.
	==========================================================================*/
	const char * SmtAdoConnection::GetProviderName()
	{
		assert(m_pConnection != NULL);
		try
		{
			return LPCTSTR(_bstr_t(m_pConnection->GetProvider()));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetProviderName 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return "";
		} 
	}

	/*========================================================================
	Name:		取得 ADO 的版本号
	==========================================================================*/
	const char * SmtAdoConnection::GetVersion()
	{
		assert(m_pConnection != NULL);
		try
		{
			return LPCTSTR(_bstr_t(m_pConnection->GetVersion()));
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: GetVersion 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return "";
		} 
	}

	/*========================================================================
	Name:		取得对象的状态(同 Recordset 对象的 GetState 方法).
	-----------------------------------------------------
	returns:	返回下列常量之一的长整型值.
	[常量]				[说明] 
	----------------------------------
	adStateClosed		指示对象是关闭的. 
	adStateOpen			指示对象是打开的. 
	-----------------------------------------------------
	Remarks:		可以随时使用 State 属性取得指定对象的当前状态.
	==========================================================================*/
	long SmtAdoConnection::GetState()
	{
		assert(m_pConnection != NULL);
		try
		{
			return m_pConnection->GetState();
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: GetState 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	/*========================================================================
	Name:	检测连接对象是否为打开状态.
	==========================================================================*/
	BOOL SmtAdoConnection::IsOpen()
	{
		try
		{
			return (m_pConnection != NULL && (m_pConnection->State & adStateOpen));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsOpen 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:	取得最后发生的错误信息.
	==========================================================================*/
	const char * SmtAdoConnection::GetLastErrorText()
	{
		assert(m_pConnection != NULL);
		static char strErrors[TEMP_BUFFER_SIZE];
		strErrors[0] = '\0';
		try
		{
			if (m_pConnection != NULL)
			{
				ErrorsPtr pErrors = m_pConnection->Errors;
				char strError[TEMP_BUFFER_SIZE];
				for (long n = 0; n < pErrors->Count; n++)
				{
					ErrorPtr pError = pErrors->GetItem(n);
					snprintf(strError,TEMP_BUFFER_SIZE,"Description: %s\r\nState: %s, Native: %d, Source: %s\r\n",
						(LPCTSTR)pError->Description,
						(LPCTSTR)pError->SQLState,
						pError->NativeError,
						(LPCTSTR)pError->Source);
					strcat(strErrors,strError);
					 
				}
			}
			return strErrors;
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: GetLastError 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return strErrors;
		} 
#ifdef _DEBUG
		return strErrors;
#else
		return "数据操作错误";
#endif

	}

	ErrorsPtr SmtAdoConnection::GetErrors()
	{
		assert(m_pConnection != NULL);
		try
		{
			if (m_pConnection != NULL)
			{
				return m_pConnection->Errors;
			}
			return NULL;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetErrors 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}

	ErrorPtr SmtAdoConnection::GetError(long index)
	{
		assert(m_pConnection != NULL);
		try
		{
			if (m_pConnection != NULL)
			{
				ErrorsPtr pErrors =  m_pConnection->Errors;
				if (index >= 0 && index < pErrors->Count)
				{
					return pErrors->GetItem(_variant_t(index));
				}
			}
			return NULL;
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetError 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}


	/*========================================================================
	Name:		取得在 Connection 对象中修改数据的可用权限.
	----------------------------------------------------------
	returns:	返回以下某个 ConnectModeEnum 的值.
	[常量]				 [说明] 
	----------------------------------
	adModeUnknown		 默认值. 表明权限尚未设置或无法确定. 
	adModeRead			 表明权限为只读. 
	adModeWrite			 表明权限为只写. 
	adModeReadWrite		 表明权限为读/写. 
	adModeShareDenyRead  防止其他用户使用读权限打开连接. 
	adModeShareDenyWrite 防止其他用户使用写权限打开连接. 
	adModeShareExclusive 防止其他用户打开连接. 
	adModeShareDenyNone  防止其他用户使用任何权限打开连接.
	----------------------------------------------------------
	Remarks: 使用 Mode 属性可设置或返回当前连接上提供者正在使用的访问权限.
	==========================================================================*/
	ConnectModeEnum SmtAdoConnection::GetMode()
	{
		assert(m_pConnection != NULL);
		try
		{
			return m_pConnection->GetMode();
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: GetMode 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adModeUnknown;
		} 
		return adModeUnknown;
	}

	/*========================================================================
	Name:		设置在 Connection 中修改数据的可用权限. 请参考 GetMode 方法.
	----------------------------------------------------------
	Remarks:	只能在关闭 Connection 对象时方可设置 Mode 属性.
	==========================================================================*/
	BOOL SmtAdoConnection::SetMode(ConnectModeEnum mode)
	{
		assert(m_pConnection != NULL);
		assert(!IsOpen());

		try
		{
			m_pConnection->PutMode(mode);
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: SetMode 发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return TRUE;
	}

	/*========================================================================
	Name:		从数据源获取数据库信息.
	-----------------------------------------------------
	Params:		QueryType  所要运行的模式查询类型,可以为下列任意常量.
	adSchemaAsserts				CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME 
	adSchemaCatalogs			CATALOG_NAME 
	adSchemaCharacterSets		CHARACTER_SET_CATALOG
	CHARACTER_SET_SCHEMA
	CHARACTER_SET_NAME 
	adSchemaCheckConstraints	CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME 
	adSchemaCollations			 COLLATION_CATALOG
	COLLATION_SCHEMA
	COLLATION_NAME 
	adSchemaColumnDomainUsage	DOMAIN_CATALOG
	DOMAIN_SCHEMA
	DOMAIN_NAME
	COLUMN_NAME 
	adSchemaColumnPrivileges	TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	COLUMN_NAME
	GRANTOR
	GRANTEE 
	adSchemaColumns				TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	COLUMN_NAME 
	adSchemaConstraintColumnUsage TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	COLUMN_NAME 
	adSchemaConstraintTableUsage TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME 
	adSchemaForeignKeys			PK_TABLE_CATALOG
	PK_TABLE_SCHEMA
	PK_TABLE_NAME
	FK_TABLE_CATALOG
	FK_TABLE_SCHEMA
	FK_TABLE_NAME 
	adSchemaIndexes				TABLE_CATALOG
	TABLE_SCHEMA
	INDEX_NAME
	TYPE
	TABLE_NAME 
	adSchemaKeyColumnUsage		CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME
	TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	COLUMN_NAME 
	adSchemaPrimaryKeys			PK_TABLE_CATALOG
	PK_TABLE_SCHEMA
	PK_TABLE_NAME 
	adSchemaProcedureColumns	PROCEDURE_CATALOG
	PROCEDURE_SCHEMA
	PROCEDURE_NAME
	COLUMN_NAME 
	adSchemaProcedureParameters PROCEDURE_CATALOG
	PROCEDURE_SCHEMA
	PROCEDURE_NAME
	PARAMTER_NAME 
	adSchemaProcedures			PROCEDURE_CATALOG
	PROCEDURE_SCHEMA
	PROCEDURE_NAME
	PROCEDURE_TYPE 
	adSchemaProviderSpecific	 参见说明 
	adSchemaProviderTypes		DATA_TYPE
	BEST_MATCH 
	adSchemaReferentialConstraints CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME 
	adSchemaSchemata			CATALOG_NAME
	SCHEMA_NAME
	SCHEMA_OWNER 
	adSchemaSQLLanguages		<无> 
	adSchemaStatistics			TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME 
	adSchemaTableConstraints	CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME
	TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	CONSTRAINT_TYPE 
	adSchemaTablePrivileges		TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	GRANTOR
	GRANTEE 
	adSchemaTables				TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME
	TABLE_TYPE 
	adSchemaTranslations		TRANSLATION_CATALOG
	TRANSLATION_SCHEMA
	TRANSLATION_NAME 
	adSchemaUsagePrivileges		OBJECT_CATALOG
	OBJECT_SCHEMA
	OBJECT_NAME
	OBJECT_TYPE
	GRANTOR
	GRANTEE 
	adSchemaViewColumnUsage		VIEW_CATALOG
	VIEW_SCHEMA
	VIEW_NAME 
	adSchemaViewTableUsage		VIEW_CATALOG
	VIEW_SCHEMA
	VIEW_NAME 
	adSchemaViews				TABLE_CATALOG
	TABLE_SCHEMA
	TABLE_NAME 
	-----------------------------------------------------
	returns:	返回包含数据库信息的 Recordset 对象. Recordset 将以只读、静态
	游标打开.
	-----------------------------------------------------
	Remarks:	OpenSchema方法返回与数据源有关的信息，例如关于服务器上的表
	以及表中的列等信息, 上述数据仅供参考, 视具体的数据源可能会有不同。
	==========================================================================*/
	_RecordsetPtr SmtAdoConnection::OpenSchema(SchemaEnum QueryType)
	{
		assert(m_pConnection != NULL);
		try
		{
			return m_pConnection->OpenSchema(QueryType, vtMissing, vtMissing);
		}
		catch(_com_error e)
		{
			//TRACE(_T("Warning: OpenSchema方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}

	/*########################################################################
	------------------------------------------------
	事务处理
	------------------------------------------------
	########################################################################*/

	/*========================================================================
	Name:		开始新事务.
	-----------------------------------------------------
	returns:	对于支持嵌套事务的数据库来说, 在已打开的事务中调用 BeginTrans 
	方法将开始新的嵌套事务. 返回值将指示嵌套层次: 返回值为 1 表示已打开顶层
	事务 (即事务不被另一个事务所嵌套), 返回值为 2 表示已打开第二层事务(嵌套
	在顶层事务中的事务), 依次类推.
	-----------------------------------------------------
	Remarks:	一旦调用了 BeginTrans 方法, 在调用 CommitTrans 或 RollbackTrans
	结束事务之前, 数据库将不再立即提交所作的任何更改.

	==========================================================================*/
	long SmtAdoConnection::BeginTrans()
	{
		assert(m_pConnection != NULL);
		try
		{
			return m_pConnection->BeginTrans();
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: BeginTrans 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	/*########################################################################
	------------------------------------------------
	事务处理
	------------------------------------------------
	BeginTrans		- 开始新事务。
	CommitTrans		- 保存任何更改并结束当前事务。它也可能启动新事务。
	RollbackTrans	- 取消当前事务中所作的任何更改并结束事务。它也可能启动
	新事务。

	一旦调用了 BeginTrans 方法，在调用 CommitTrans 或 RollbackTrans 结
	束事务之前，数据库将不再立即提交所作的任何更改。
	对于支持嵌套事务的数据库来说，在已打开的事务中调用 BeginTrans 方法
	将开始新的嵌套事务。返回值将指示嵌套层次：返回值为 1 表示已打开顶层事务
	(即事务不被另一个事务所嵌套)，返回值为 2 表示已打开第二层事务(嵌套
	在顶层事务中的事务)，依次类推。调用 CommitTrans 或 RollbackTrans 只影
	响最新打开的事务；在处理任何更高层事务之前必须关闭或回卷当前事务。
	调用 CommitTrans 方法将保存连接上打开的事务中所做的更改并结束事务。
	调用 RollbackTrans方法还原打开事务中所做的更改并结束事务。在未打开事务
	时调用其中任何一种方法都将引发错误。
	取决于 Connection 对象的 Attributes 属性，调用 CommitTrans 或 
	RollbackTrans 方法都可以自动启动新事务。如果 Attributes 属性设置为 
	adXactCommitRetaining，数据库在 CommitTrans 调用后会自动启动新事务。
	如果 Attributes 属性设置为 adXactAbortRetaining，数据库在调用 
	RollbackTrans 之后将自动启动新事务。
	########################################################################*/
	/*========================================================================
	Name:		保存任何更改并结束当前事务.
	-----------------------------------------------------
	Remarks:	调用 CommitTrans 或 RollbackTrans 只影响最新打开的事务; 在
	处理任何更高层事务之前必须关闭或回卷当前事务.
	==========================================================================*/
	BOOL SmtAdoConnection::CommitTrans()
	{
		assert(m_pConnection != NULL);
		try
		{
			return SUCCEEDED(m_pConnection->CommitTrans());
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: CommitTrans 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		取消当前事务中所作的任何更改并结束事务.
	==========================================================================*/
	BOOL SmtAdoConnection::RollbackTrans()
	{
		assert(m_pConnection != NULL);
		try
		{
			return SUCCEEDED(m_pConnection->RollbackTrans());
		}
		catch (_com_error e)
		{
			//TRACE(_T("Warning: RollbackTrans 方法发生异常. 错误信息: %s; 文件: %s; 行: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}
}