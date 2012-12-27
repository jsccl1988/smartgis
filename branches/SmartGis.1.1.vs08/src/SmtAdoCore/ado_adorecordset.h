#ifndef _ADO_ADORECORDSET
#define _ADO_ADORECORDSET

#include "ado_adoconnection.h"
namespace Smt_Ado
{
	class SMT_EXPORT_CLASS SmtAdoRecordSet
	{
	public:
	public:
		SmtAdoRecordSet();
		SmtAdoRecordSet(SmtAdoConnection *pConnection);

		virtual ~SmtAdoRecordSet();
		//flag form end to first
		BOOL                     SeekToFieldValue(int fieldIndex, const string & Value ,BOOL flag);
	protected:
		void                     Release();

		// ���� ------------------------------------------------	
	public:
		// ��ǰ�༭״̬ ----------------------------
		EditModeEnum             GetEditMode();

		// ��ǰ״̬ --------------------------------
		BOOL                     IsEOF();
		BOOL                     IsBOF();
		BOOL                     IsOpen();
		long                     GetState();
		long                     GetStatus();

		// �����ص�����¼�� --------------------
		long                     GetMaxRecordCount();
		BOOL                     SetMaxRecordCount(long count);

		// ���λ�� --------------------------------
		CursorLocationEnum       GetCursorLocation();
		BOOL                     SetCursorLocation(CursorLocationEnum CursorLocation = adUseClient);

		// ������� --------------------------------
		CursorTypeEnum           GetCursorType();
		BOOL                     SetCursorType(CursorTypeEnum CursorType = adOpenStatic);

		// ��ǩ --------------------------------
		_variant_t               GetBookmark();
		BOOL                     SetBookmark(_variant_t varBookMark = _variant_t((long)adBookmarkFirst));

		// ��ǰ��¼λ�� ------------------------
		long                     GetAbsolutePosition();
		BOOL                     SetAbsolutePosition(int nPosition);

		long                     GetAbsolutePage();
		BOOL                     SetAbsolutePage(int nPage);

		// ÿҳ�ļ�¼�� ------------------------
		long                     GetPageSize();
		BOOL                     SetCacheSize(const long& lCacheSize);	

		// ҳ�� --------------------------------
		long                     GetPageCount();

		// ��¼�����ֶ��� ----------------------
		long                     GetRecordCount();
		long                     GetFieldsCount();

		// �ֶ�ֵ�ڼ�¼�����е�λ�� --------------------------	
		int                      GetFieldIndex(const char * filedName);

		// ����λ�� --------------------------	
		long                     GetCursorPos();

		// ��ѯ�ַ��� --------------------------
		string                   GetSQLText() {return m_strSQL;}
		void                     SetSQLText(LPCTSTR strSQL) {m_strSQL = strSQL;}

		// ���Ӷ��� ----------------------------
		SmtAdoConnection*        GetConnection() {return m_pConnection;}
		void                     SetAdoConnection(SmtAdoConnection *pConnection);

		// ��¼������ --------------------------
		_RecordsetPtr&           GetRecordset();

		const char *             GetLastError();

		// �ֶ����� ----------------------------------------------
	public:
		// �ֶμ� -------------------------------
		FieldsPtr                GetFields();

		// �ֶζ��� -----------------------------
		FieldPtr                 GetField(long lIndex);
		FieldPtr                 GetField(LPCTSTR lpszFieldName);

		// �ֶ��� -------------------------------
		const char *             GetFieldName(long lIndex);

		// �ֶ��������� -------------------------
		DataTypeEnum             GetFieldType(long lIndex);
		DataTypeEnum             GetFieldType(LPCTSTR lpszFieldName);

		// �ֶ����� -----------------------------
		long                     GetFieldAttributes(long lIndex);
		long                     GetFieldAttributes(LPCTSTR lpszFieldName);

		// �ֶζ��峤�� -------------------------
		long                     GetFieldDefineSize(long lIndex);
		long                     GetFieldDefineSize(LPCTSTR lpszFieldName);

		// �ֶ�ʵ�ʳ��� -------------------------
		long                     GetFieldActualSize(long lIndex);
		long                     GetFieldActualSize(LPCTSTR lpszFieldName);

		// �ֶ��Ƿ�ΪNULL -----------------------
		BOOL                     IsFieldNull(long index);
		BOOL                     IsFieldNull(LPCTSTR lpFieldName);

		// ��¼���� --------------------------------------------
	public:
		BOOL                     AddNew();
		BOOL                     Update();
		BOOL                     UpdateBatch(AffectEnum AffectRecords = adAffectAll); 
		BOOL                     CancelUpdate();
		BOOL                     CancelBatch(AffectEnum AffectRecords = adAffectAll);
		BOOL                     Delete(AffectEnum AffectRecords = adAffectCurrent);

		// ˢ�¼�¼���е����� ------------------
		BOOL                     Requery(long Options = adConnectUnspecified);
		BOOL                     Resync(AffectEnum AffectRecords = adAffectAll, ResyncEnum ResyncValues = adResyncAllValues);   

		BOOL                     RecordBinding(CADORecordBinding &pAdoRecordBinding);
		BOOL                     AddNew(CADORecordBinding &pAdoRecordBinding);

		// ��¼���������� --------------------------------------
	public:
		BOOL                     MoveFirst();
		BOOL                     MovePrevious();
		BOOL                     MoveNext();
		BOOL                     MoveLast();
		BOOL                     Move(long lRecords, _variant_t Start = _variant_t((long)adBookmarkFirst));

		// ����ָ���ļ�¼ ----------------------
		BOOL                     Find(LPCTSTR lpszFind, SearchDirectionEnum SearchDirection = adSearchForward);
		BOOL                     FindNext();

		// ��ѯ ------------------------------------------------
	public:
		BOOL                     Open(LPCTSTR strSQL, long lOption = adCmdText, CursorTypeEnum CursorType = adOpenStatic, LockTypeEnum LockType = adLockOptimistic);
		BOOL                     Cancel();
		void                     Close();

		// ����/����־����ļ� -----------------
		BOOL                     Save(LPCTSTR strFileName = "", PersistFormatEnum PersistFormat = adPersistXML);
		BOOL                     Load(LPCTSTR strFileName);

		// �ֶδ�ȡ --------------------------------------------
	public:
		BOOL                     PutCollect(long index, const _variant_t &value);
		BOOL                     PutCollect(long index, const string &value);
		BOOL                     PutCollect(long index, const double &value);
		BOOL                     PutCollect(long index, const float  &value);
		BOOL                     PutCollect(long index, const long   &value);
		BOOL                     PutCollect(long index, const DWORD  &value);
		BOOL                     PutCollect(long index, const int    &value);
		BOOL                     PutCollect(long index, const short  &value);
		BOOL                     PutCollect(long index, const BYTE   &value);
		BOOL                     PutCollect(long index, const bool   &value);
	
		BOOL                     PutCollect(LPCTSTR strFieldName, const _variant_t &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const string &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const double &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const float  &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const long   &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const DWORD  &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const int    &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const short  &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const BYTE   &value);
		BOOL                     PutCollect(LPCTSTR strFieldName, const bool   &value);
		// ---------------------------------------------------------

		BOOL                     GetCollect(long index, string &value);
		BOOL                     GetCollect(long index, double  &value);
		BOOL                     GetCollect(long index, float   &value);
		BOOL                     GetCollect(long index, long    &value);
		BOOL                     GetCollect(long index, DWORD   &value);
		BOOL                     GetCollect(long index, int     &value);
		BOOL                     GetCollect(long index, short   &value);
		BOOL                     GetCollect(long index, BYTE    &value);
		BOOL                     GetCollect(long index, bool   &value);

		BOOL                     GetCollect(LPCSTR strFieldName, char * pValue,int nBufSize);
		BOOL                     GetCollect(LPCSTR strFieldName, double &value);
		BOOL                     GetCollect(LPCSTR strFieldName, float  &value);
		BOOL                     GetCollect(LPCSTR strFieldName, long   &value);
		BOOL                     GetCollect(LPCSTR strFieldName, DWORD  &value);
		BOOL                     GetCollect(LPCSTR strFieldName, int    &value);
		BOOL                     GetCollect(LPCSTR strFieldName, short  &value);
		BOOL                     GetCollect(LPCSTR strFieldName, BYTE   &value);
		BOOL                     GetCollect(LPCSTR strFieldName, bool   &value);

		// BLOB ���ݴ�ȡ ------------------------------------------
		BOOL                     LoadOLEDataFromDB(const char * CreateFileName ,const char *  feildName);

		BOOL                     AppendChunk(FieldPtr pField, LPVOID lpData, UINT nBytes);
		BOOL                     AppendChunk(long index, LPVOID lpData, UINT nBytes);
		BOOL                     AppendChunk(LPCSTR strFieldName, LPVOID lpData, UINT nBytes);
		BOOL                     AppendChunk(long index, LPCTSTR lpszFileName);
		BOOL                     AppendChunk(LPCSTR strFieldName, LPCTSTR lpszFileName);

		BOOL                     GetChunk(FieldPtr pField, LPVOID lpData);
		BOOL                     GetChunk(long index, LPVOID lpData);
		BOOL                     GetChunk(LPCSTR strFieldName, LPVOID lpData);
	
		// �������� --------------------------------------------
	public:
		// ���˼�¼ ---------------------------------
		BOOL                     SetFilter(LPCTSTR lpszFilter);

		// ���� -------------------------------------
		BOOL                     SetSort(LPCTSTR lpszCriteria);

		// �����Ƿ�֧��ĳ���� -----------------------
		BOOL                     Supports(CursorOptionEnum CursorOptions = adAddNew);

		// ��¡ -------------------------------------
		BOOL                     Clone(SmtAdoRecordSet &pRecordSet);
		_RecordsetPtr operator = (_RecordsetPtr &pRecordSet);

		// ��ʽ�� _variant_t ����ֵ -----------------

		//��Ա���� --------------------------------------------
	protected:
		SmtAdoConnection         *m_pConnection;
		_RecordsetPtr		     m_pRecordset;
		string				     m_strSQL;
		string				     m_strFind;
		string				     m_strFileName;
		IADORecordBinding	     *m_pAdoRecordBinding;
		SearchDirectionEnum      m_SearchDirection;
	public:

		_variant_t			     m_varBookmark;
	};
}
#endif //_ADO_ADORECORDSET