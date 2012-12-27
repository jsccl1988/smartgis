#ifndef _ADO_ADOCONNECTION_H
#define _ADO_ADOCONNECTION_H

#include "smt_core.h"

#import "C:\program files\common files\System\ado\msado15.dll" no_namespace \
	rename("EOF","adoEOF") \
	rename("BOF","adoBOF") 

#include <icrsint.h>
#include <comdef.h> 

namespace Smt_Ado
{
	class SmtAdoConnection;
}
#include "ado_adorecordset.h"

bool SMT_EXPORT_API vartobool(const _variant_t& var);
BYTE SMT_EXPORT_API vartoby(const _variant_t& var);
short SMT_EXPORT_API vartoi(const _variant_t& var);
long SMT_EXPORT_API vartol(const _variant_t& var);
double SMT_EXPORT_API vartof(const _variant_t& var);
const char SMT_EXPORT_API *vartostr(const _variant_t& var);

namespace Smt_Ado
{
	const	string		C_STR_ADO_LOG = "SmtAdo";

	class SMT_EXPORT_CLASS SmtAdoConnection
	{
	public:
		SmtAdoConnection();
		virtual ~SmtAdoConnection();

	protected:
		void                     Release();

	public:
		// ���Ӷ��� ----------------------------------
		_ConnectionPtr&          GetConnection() {return m_pConnection;};
		// �����ִ� ----------------------------------
		const char *             GetConnectionText() {return m_strConnect.c_str();}
		// ������Ϣ ----------------------------------
		const char *             GetProviderName();
		const char *             GetVersion();
		const char *             GetDefaultDatabase();

		// �쳣��Ϣ ----------------------------------
        const char *             GetLastErrorText();
		ErrorsPtr                GetErrors();
		ErrorPtr                 GetError(long index);
    public:
		// ����״̬ ----------------------------------
		BOOL                     IsOpen();
		long                     GetState();

		// ����ģʽ ----------------------------------
		ConnectModeEnum          GetMode();
		BOOL                     SetMode(ConnectModeEnum mode);

    public:
		// ����Դ��Ϣ -------------------------------
		_RecordsetPtr            OpenSchema(SchemaEnum QueryType);

	public:
		// ���ݿ����� --------------------------------
		BOOL                     Open(const char * lpszConnect ="" , long lOptions = adConnectUnspecified);
		BOOL                     ConnectDSN(const char * dsn,const char * username = "",const char * pass = "", long lOptions = adConnectUnspecified);
		BOOL                     ConnectAccess(const char * dbpath,const char * username = "" , const char * pass = "", long lOptions = adConnectUnspecified);
		BOOL                     ConnectSqlServer(const char * dbsrc,const char * dbname, const char * username = "",const char * pass = "", long lOptions = adConnectUnspecified);
		void                     Close();
	public:
		// ������ ----------------------------------
		long                     BeginTrans();
		BOOL                     RollbackTrans();
		BOOL                     CommitTrans();

	public:
		// ִ�� SQL ��� ------------------------------
		_RecordsetPtr            Execute(const char * szSQL, long lOptions = adCmdText);
		BOOL                     Cancel();

	protected:
		string			         m_strConnect;
		_ConnectionPtr	         m_pConnection;
	};
}

#if !defined(Export_SmtAdoCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtAdoCoreD.lib")
#       else
#          pragma comment(lib,"SmtAdoCore.lib")
#	    endif  
#endif

#endif //_ADO_ADOCONNECTION_H