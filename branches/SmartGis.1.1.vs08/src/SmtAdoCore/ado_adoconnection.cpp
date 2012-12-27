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
	//	TRACE(_T("Warning: δ����� _variant_t ����ֵ; �ļ�: %s; ��: %d\n"), __FILE__, __LINE__);
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
	//	TRACE(_T("Warning: δ����� _variant_t ����ֵ; �ļ�: %s; ��: %d\n"), __FILE__, __LINE__);
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
	//	TRACE(_T("Warning: δ����� _variant_t ����ֵ; �ļ�: %s; ��: %d\n"), __FILE__, __LINE__);
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
	case VT_BSTR://�ַ���
	case VT_LPSTR://�ַ���
	case VT_LPWSTR://�ַ���
		value = atol((LPCTSTR)(_bstr_t)var);
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		;
		//TRACE(_T("Warning: δ����� _variant_t ����ֵ; �ļ�: %s; ��: %d\n"), __FILE__, __LINE__);
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
	case VT_BSTR://�ַ���
	case VT_LPSTR://�ַ���
	case VT_LPWSTR://�ַ���
		value = atof((LPCTSTR)(_bstr_t)var);
		break;
	case VT_NULL:
	case VT_EMPTY:
		value = 0;
		break;
	default:
		value = 0;
	//	TRACE(_T("Warning: δ����� _variant_t ����ֵ; �ļ�: %s; ��: %d\n"), __FILE__, __LINE__);
	}
	return value;
}

const char * vartostr(const _variant_t &var)
{
	static char buf[TEMP_BUFFER_SIZE];
	memset(buf,'\0',TEMP_BUFFER_SIZE);

	switch (var.vt)
	{
	case VT_BSTR://�ַ���
	case VT_LPSTR://�ַ���
	case VT_LPWSTR://�ַ���
		sprintf(buf,(LPCTSTR)(_bstr_t)var);
		break;
	case VT_I1:
	case VT_UI1:
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.bVal);	 
		break;
	case VT_I2://������
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.iVal);
		break;
	case VT_UI2://�޷��Ŷ�����
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.uiVal);
		break;
	case VT_INT://����
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.intVal);
		break;
	case VT_I4: //����
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.lVal);
		break;
	case VT_I8: //������
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.lVal);
		break;
	case VT_UINT://�޷�������
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.uintVal);
		break;
	case VT_UI4: //�޷�������
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.ulVal);
		break;
	case VT_UI8: //�޷��ų�����
		snprintf( buf, TEMP_BUFFER_SIZE, "%d", var.ulVal);
		break;
	case VT_VOID:
		snprintf( buf, TEMP_BUFFER_SIZE, "%8x", var.byref);
		break;
	case VT_R4://������
		snprintf( buf, TEMP_BUFFER_SIZE, "%.4f", var.fltVal);
		break;
	case VT_R8://˫������
		snprintf( buf, TEMP_BUFFER_SIZE, "%.8f", var.dblVal);
		break;
	case VT_DECIMAL: //С��
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
	case VT_BOOL://������
        sprintf(buf,var.boolVal?"TRUE" : "FALSE");
		break;
	case VT_DATE: //������
		{
		 
		}
		break;
	case VT_NULL://NULLֵ
		 sprintf(buf,"");
		break;
	case VT_EMPTY://��
		 sprintf(buf,"");
		break;
	case VT_UNKNOWN://δ֪����
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
		//���� Connection ����---------------------------
		m_pConnection.CreateInstance("ADODB.Connection");
#ifdef _DEBUG
		if (m_pConnection == NULL)
		{
		//	AfxMessageBox("Connection ���󴴽�ʧ��! ��ȷ���Ƿ��ʼ����COM����\r\n");
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
	Name:		���ӵ�����Դ.
	-----------------------------------------------------
	Params:		[lpszConnect]: �����ַ���, ����������Ϣ.
	[lOptions]:	��ѡ. �����÷�������ͬ�������첽�ķ�ʽ��������
	Դ. ����������ĳ������:
	[����]					[˵��] 
	----------------------------------
	adConnectUnspecified	(Ĭ��)ͬ����ʽ������. 
	adAsyncConnect			�첽��ʽ������. Ado�� ConnectComplete ��
	����֪ͨ�Ѿ��������. 
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
			// �������ݿ� ---------------------------------------------
			if (pLog)
				pLog->LogMessage("����:%s",lpszConnect);
			return (m_pConnection->Open(_bstr_t(lpszConnect), "", "", lOptions) == S_OK);
		}
		catch (_com_error e)
		{
			if (pLog)
				pLog->LogMessage("Warning: �������ݿⷢ���쳣. ������Ϣ: %s; ",e.ErrorMessage());
		} 
		catch (...)
		{
			if (pLog)
				pLog->LogMessage("Warning: �������ݿ�ʱ����δ֪����:");
		}


		return FALSE;
	}

	/*========================================================================
	Name:	���� Access ���ݿ�. 
	-----------------------------------------------------
	Params:	
	[dbpath]:	Access·��.
	[username]:		�û���.
	[pass]:		����.
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
	Name:	���� DSN. 
	-----------------------------------------------------
	Params:	
	[dsn]:		DSN����
	[username]:		�û���.
	[pass]:		����.
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
	Name:	���� SQL SERVER ���ݿ�. 
	-----------------------------------------------------
	Params:		
	[dbsrc]:	SQL SERVER ��������.
	[dbname]:	Ĭ�ϵ����ݿ���.
	[username]:	�û���.
	[pass]:		����.
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
			//TRACE(_T("Warning: �ر����ݿⷢ���쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
		} 
	}

	/*========================================================================
	Name:	�ر����Ӳ��ͷŶ���.
	-----------------------------------------------------
	Remarks: �ر����Ӳ��ͷ�connection����.
	==========================================================================*/
	void SmtAdoConnection::Release()
	{
		if (IsOpen()) Close();
		m_pConnection.Release();
	}

	/*========================================================================
	Name:		ִ��ָ���Ĳ�ѯ��SQL ��䡢�洢���̵�.
	----------------------------------------------------------
	Remarks:	��ο� CAdoRecordSet ���Open����. ���ص� Recordset ����ʼ
	��Ϊֻ��������ǰ���α�.
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
			//TRACE(_T("Warning: Execute ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Remarks:	��ο� CAdoRecordSet �� Cancel ����.
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
		//	TRACE(_T("Warning: Cancel ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		ȡ�� Connection �����ṩ�ߵ�����.
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
		//	TRACE(_T("Warning: GetProviderName ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return "";
		} 
	}

	/*========================================================================
	Name:		ȡ�� ADO �İ汾��
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
			//TRACE(_T("Warning: GetVersion ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return "";
		} 
	}

	/*========================================================================
	Name:		ȡ�ö����״̬(ͬ Recordset ����� GetState ����).
	-----------------------------------------------------
	returns:	�������г���֮һ�ĳ�����ֵ.
	[����]				[˵��] 
	----------------------------------
	adStateClosed		ָʾ�����ǹرյ�. 
	adStateOpen			ָʾ�����Ǵ򿪵�. 
	-----------------------------------------------------
	Remarks:		������ʱʹ�� State ����ȡ��ָ������ĵ�ǰ״̬.
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
			//TRACE(_T("Warning: GetState �����쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	/*========================================================================
	Name:	������Ӷ����Ƿ�Ϊ��״̬.
	==========================================================================*/
	BOOL SmtAdoConnection::IsOpen()
	{
		try
		{
			return (m_pConnection != NULL && (m_pConnection->State & adStateOpen));
		}
		catch (_com_error e)
		{
		//	TRACE(_T("Warning: IsOpen ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:	ȡ��������Ĵ�����Ϣ.
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
			//TRACE(_T("Warning: GetLastError ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return strErrors;
		} 
#ifdef _DEBUG
		return strErrors;
#else
		return "���ݲ�������";
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
		//	TRACE(_T("Warning: GetErrors ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
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
		//	TRACE(_T("Warning: GetError ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}


	/*========================================================================
	Name:		ȡ���� Connection �������޸����ݵĿ���Ȩ��.
	----------------------------------------------------------
	returns:	��������ĳ�� ConnectModeEnum ��ֵ.
	[����]				 [˵��] 
	----------------------------------
	adModeUnknown		 Ĭ��ֵ. ����Ȩ����δ���û��޷�ȷ��. 
	adModeRead			 ����Ȩ��Ϊֻ��. 
	adModeWrite			 ����Ȩ��Ϊֻд. 
	adModeReadWrite		 ����Ȩ��Ϊ��/д. 
	adModeShareDenyRead  ��ֹ�����û�ʹ�ö�Ȩ�޴�����. 
	adModeShareDenyWrite ��ֹ�����û�ʹ��дȨ�޴�����. 
	adModeShareExclusive ��ֹ�����û�������. 
	adModeShareDenyNone  ��ֹ�����û�ʹ���κ�Ȩ�޴�����.
	----------------------------------------------------------
	Remarks: ʹ�� Mode ���Կ����û򷵻ص�ǰ�������ṩ������ʹ�õķ���Ȩ��.
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
		//	TRACE(_T("Warning: GetMode �����쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return adModeUnknown;
		} 
		return adModeUnknown;
	}

	/*========================================================================
	Name:		������ Connection ���޸����ݵĿ���Ȩ��. ��ο� GetMode ����.
	----------------------------------------------------------
	Remarks:	ֻ���ڹر� Connection ����ʱ�������� Mode ����.
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
		//	TRACE(_T("Warning: SetMode �����쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return TRUE;
	}

	/*========================================================================
	Name:		������Դ��ȡ���ݿ���Ϣ.
	-----------------------------------------------------
	Params:		QueryType  ��Ҫ���е�ģʽ��ѯ����,����Ϊ�������ⳣ��.
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
	adSchemaProviderSpecific	 �μ�˵�� 
	adSchemaProviderTypes		DATA_TYPE
	BEST_MATCH 
	adSchemaReferentialConstraints CONSTRAINT_CATALOG
	CONSTRAINT_SCHEMA
	CONSTRAINT_NAME 
	adSchemaSchemata			CATALOG_NAME
	SCHEMA_NAME
	SCHEMA_OWNER 
	adSchemaSQLLanguages		<��> 
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
	returns:	���ذ������ݿ���Ϣ�� Recordset ����. Recordset ����ֻ������̬
	�α��.
	-----------------------------------------------------
	Remarks:	OpenSchema��������������Դ�йص���Ϣ��������ڷ������ϵı�
	�Լ����е��е���Ϣ, �������ݽ����ο�, �Ӿ��������Դ���ܻ��в�ͬ��
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
			//TRACE(_T("Warning: OpenSchema���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return NULL;
		} 
		return NULL;
	}

	/*########################################################################
	------------------------------------------------
	������
	------------------------------------------------
	########################################################################*/

	/*========================================================================
	Name:		��ʼ������.
	-----------------------------------------------------
	returns:	����֧��Ƕ����������ݿ���˵, ���Ѵ򿪵������е��� BeginTrans 
	��������ʼ�µ�Ƕ������. ����ֵ��ָʾǶ�ײ��: ����ֵΪ 1 ��ʾ�Ѵ򿪶���
	���� (�����񲻱���һ��������Ƕ��), ����ֵΪ 2 ��ʾ�Ѵ򿪵ڶ�������(Ƕ��
	�ڶ��������е�����), ��������.
	-----------------------------------------------------
	Remarks:	һ�������� BeginTrans ����, �ڵ��� CommitTrans �� RollbackTrans
	��������֮ǰ, ���ݿ⽫���������ύ�������κθ���.

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
			//TRACE(_T("Warning: BeginTrans ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return -1;
		} 
		return -1;
	}

	/*########################################################################
	------------------------------------------------
	������
	------------------------------------------------
	BeginTrans		- ��ʼ������
	CommitTrans		- �����κθ��Ĳ�������ǰ������Ҳ��������������
	RollbackTrans	- ȡ����ǰ�������������κθ��Ĳ�����������Ҳ��������
	������

	һ�������� BeginTrans �������ڵ��� CommitTrans �� RollbackTrans ��
	������֮ǰ�����ݿ⽫���������ύ�������κθ��ġ�
	����֧��Ƕ����������ݿ���˵�����Ѵ򿪵������е��� BeginTrans ����
	����ʼ�µ�Ƕ�����񡣷���ֵ��ָʾǶ�ײ�Σ�����ֵΪ 1 ��ʾ�Ѵ򿪶�������
	(�����񲻱���һ��������Ƕ��)������ֵΪ 2 ��ʾ�Ѵ򿪵ڶ�������(Ƕ��
	�ڶ��������е�����)���������ơ����� CommitTrans �� RollbackTrans ֻӰ
	�����´򿪵������ڴ����κθ��߲�����֮ǰ����رջ�ؾ�ǰ����
	���� CommitTrans ���������������ϴ򿪵������������ĸ��Ĳ���������
	���� RollbackTrans������ԭ�������������ĸ��Ĳ�����������δ������
	ʱ���������κ�һ�ַ���������������
	ȡ���� Connection ����� Attributes ���ԣ����� CommitTrans �� 
	RollbackTrans �����������Զ�������������� Attributes ��������Ϊ 
	adXactCommitRetaining�����ݿ��� CommitTrans ���ú���Զ�����������
	��� Attributes ��������Ϊ adXactAbortRetaining�����ݿ��ڵ��� 
	RollbackTrans ֮���Զ�����������
	########################################################################*/
	/*========================================================================
	Name:		�����κθ��Ĳ�������ǰ����.
	-----------------------------------------------------
	Remarks:	���� CommitTrans �� RollbackTrans ֻӰ�����´򿪵�����; ��
	�����κθ��߲�����֮ǰ����رջ�ؾ�ǰ����.
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
		//	TRACE(_T("Warning: CommitTrans ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}

	/*========================================================================
	Name:		ȡ����ǰ�������������κθ��Ĳ���������.
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
			//TRACE(_T("Warning: RollbackTrans ���������쳣. ������Ϣ: %s; �ļ�: %s; ��: %d\n"), e.ErrorMessage(), __FILE__, __LINE__);
			return FALSE;
		} 
		return FALSE;
	}
}