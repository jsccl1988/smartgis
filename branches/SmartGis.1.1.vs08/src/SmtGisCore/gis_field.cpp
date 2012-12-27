#include "gis_attribute.h"
#include "smt_api.h"

using namespace Smt_Core;

namespace Smt_GIS
{
	void	InitFieldNameToTypeMap(map<string,ushort> &fldNameToType)
	{
		fldNameToType.insert(map<string,ushort>::value_type("Integer",SmtInteger));
		fldNameToType.insert(map<string,ushort>::value_type("Real",SmtReal));
		fldNameToType.insert(map<string,ushort>::value_type("Bool",SmtBool));
		fldNameToType.insert(map<string,ushort>::value_type("Byte",SmtByte));
		fldNameToType.insert(map<string,ushort>::value_type("String",SmtString));
		fldNameToType.insert(map<string,ushort>::value_type("IntegerList",SmtIntegerList));
		fldNameToType.insert(map<string,ushort>::value_type("RealList",SmtRealList));
		fldNameToType.insert(map<string,ushort>::value_type("StringList",SmtStringList));
		fldNameToType.insert(map<string,ushort>::value_type("Binary",SmtBinary));
		fldNameToType.insert(map<string,ushort>::value_type("Date",SmtDate));
		fldNameToType.insert(map<string,ushort>::value_type("Time",SmtTime));
		fldNameToType.insert(map<string,ushort>::value_type("DateTime",SmtDateTime));
	}

	void	InitFieldTypeToNameMap(map<ushort,string> &fldTypeToName)
	{
		fldTypeToName.insert(map<ushort,string>::value_type(SmtInteger,"Integer"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtReal,"Real"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtBool,"Bool"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtByte,"Byte"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtString,"String"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtIntegerList,"IntegerList"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtRealList,"RealList"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtStringList,"StringList"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtBinary,"Binary"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtDate,"Date"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtTime,"Time"));
		fldTypeToName.insert(map<ushort,string>::value_type(SmtDateTime,"DateTime"));
	}

	//////////////////////////////////////////////////////////////////////////
	SmtField::SmtField()
	{
		m_fldName[0] = '\0';
		m_fldValue.Vt = SmtUnknown;
	}

	SmtField::~SmtField()
	{
		UnSetField();
	}

	//////////////////////////////////////////////////////////////////////////
	const char * SmtField:: GetTypeName(ushort type)
	{
		switch (type)
		{
		case   SmtInteger:
			return "Integer";

		case   SmtReal:	
			return "Real";

		case   SmtBool :	
			return "Bool";

		case   SmtByte :
			return "Byte";

		case   SmtString :
			return "String";

		case   SmtIntegerList:
			return "IntegerList";

		case   SmtRealList:
			return "RealList";

		case   SmtStringList:
			return "StringList";

		case   SmtBinary:
			return "Binary";

		case   SmtDate:
			return "Date";

		case   SmtTime:
			return "Time";

		case   SmtDateTime:
			return "DateTime";
		case   SmtUnknown://未知类型
		default:
			return "Unknown";	
		}
	}

	void SmtField::GetAllTypes(vector<int> &vFldTypes)
	{
		vFldTypes.clear();
		vFldTypes.push_back(SmtInteger);
		vFldTypes.push_back(SmtReal);
		vFldTypes.push_back(SmtBool);
		vFldTypes.push_back(SmtByte);
		vFldTypes.push_back(SmtString);
		vFldTypes.push_back(SmtIntegerList);
		vFldTypes.push_back(SmtRealList);
		vFldTypes.push_back(SmtStringList);
		vFldTypes.push_back(SmtBinary);
		vFldTypes.push_back(SmtDate);
		vFldTypes.push_back(SmtTime);
		vFldTypes.push_back(SmtDateTime);
	}

	void SmtField::GetAllTypeNames(vector<string> &vFldTypeNames)
	{
		vFldTypeNames.clear();
		vFldTypeNames.push_back("Integer");
		vFldTypeNames.push_back("Real");
		vFldTypeNames.push_back("Bool");
		vFldTypeNames.push_back("Byte");
		vFldTypeNames.push_back("String");
		vFldTypeNames.push_back("IntegerList");
		vFldTypeNames.push_back("RealList");
		vFldTypeNames.push_back("StringList");
		vFldTypeNames.push_back("Binary");
		vFldTypeNames.push_back("Date");
		vFldTypeNames.push_back("Time");
		vFldTypeNames.push_back("DateTime");
	}

	ushort SmtField::GetType(const char *szTypeName)
	{
		static map<string,ushort> fldNameToType;
		static bool bOnce = (InitFieldNameToTypeMap(fldNameToType),true);

		map<string,ushort>::iterator iter;
		iter = fldNameToType.find(szTypeName);
		if (iter == fldNameToType.end())
		{
			return SmtUnknown;
		}
		else
			return iter->second;
	}

	//////////////////////////////////////////////////////////////////////////
	int  SmtField::GetValueAsInteger( ) const 
	{
		if( !IsFieldSetted() )
			return 0;

		return VarToInteger(m_fldValue);
	}

	double SmtField::GetValueAsDouble( ) const 
	{
		if( !IsFieldSetted() )
			return 0;

		return VarToDouble(m_fldValue);
	}

	const char *SmtField::GetValueAsString( ) const 
	{
		if( !IsFieldSetted() )
			return "";

		return VarToString(m_fldValue);
	}

	const int *SmtField::GetValueAsIntegerList( int *pnCount ) const 
	{
		if( !IsFieldSetted() )
			return NULL;

		if( m_fldValue.Vt == SmtIntegerList )
		{
			if( pnCount != NULL )
				*pnCount = m_fldValue.iValList.nCount;

			return m_fldValue.iValList.paList;
		}
		else
		{
			if( pnCount != NULL )
				*pnCount = 0;

			return NULL;
		}
	}

	const double *SmtField::GetValueAsDoubleList(  int *pnCount ) const 
	{
		if( !IsFieldSetted() )
			return NULL;

		if( m_fldValue.Vt == SmtRealList )
		{
			if( pnCount != NULL )
				*pnCount = m_fldValue.dbfValList.nCount;

			return m_fldValue.dbfValList.paList;
		}
		else
		{
			if( pnCount != NULL )
				*pnCount = 0;

			return NULL;
		}
	}

	char ** SmtField::GetValueAsStringList( ) const
	{
	//	if( !IsFieldSetted() )
	//		return NULL;

		if( m_fldValue.Vt == SmtStringList)
		{
			return m_fldValue.bstrValList.paList;
		}
		else
		{
			return NULL;
		}
	}

	byte  *SmtField::GetValueAsBinary( int *pnCount ) const 
	{
		if( !IsFieldSetted() )
			return NULL;

		if( m_fldValue.Vt == SmtBinary )
		{
			if( pnCount != NULL )
				*pnCount = m_fldValue.blobVal.nCount;

			return m_fldValue.blobVal.paData;
		}
		else
		{
			if( pnCount != NULL )
				*pnCount = 0;

			return NULL;
		}
	}

	int  SmtField::GetValueAsDateTime( int *pnYear, int *pnMonth, int *pnDay,int *pnHour, int *pnMinute, int *pnSecond, int *pnTZFlag ) const 
	{
		if( !IsFieldSetted() )
			return SMT_ERR_FAILURE;

		if( m_fldValue.Vt == SmtDate
			|| m_fldValue.Vt == SmtTime
			|| m_fldValue.Vt == SmtDateTime )

		{
			if( pnYear )
				*pnYear = m_fldValue.dateVal.Year;
			if( pnMonth )
				*pnMonth = m_fldValue.dateVal.Month;
			if( pnDay )
				*pnDay = m_fldValue.dateVal.Day;
			if( pnHour )
				*pnHour = m_fldValue.dateVal.Hour;
			if( pnMinute )
				*pnMinute = m_fldValue.dateVal.Minute;
			if( pnSecond )
				*pnSecond = m_fldValue.dateVal.Second;
			if( pnTZFlag )
				*pnTZFlag = m_fldValue.dateVal.TZFlag;

			return SMT_ERR_NONE;
		}
		else
		{
			return SMT_ERR_FAILURE;
		}

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////

	void SmtField::UnSetField()
	{
		if (!IsFieldSetted())
			return;
		 
		switch (m_fldValue.Vt)
		{
		case   SmtString :
			if (m_fldValue.bstrVal != NULL)
			{
				free(m_fldValue.bstrVal);
			}
			break;

		case   SmtIntegerList:
			if (m_fldValue.iValList.paList != NULL)
			{
				SMT_SAFE_DELETE_A(m_fldValue.iValList.paList);
				m_fldValue.iValList.nCount = 0;
			}
			break;	

		case   SmtRealList:
			if (m_fldValue.dbfValList.paList != NULL)
			{
				SMT_SAFE_DELETE_A(m_fldValue.dbfValList.paList);
				m_fldValue.dbfValList.nCount = 0;
			}
			break;

		case   SmtStringList:
			{
				for (int i = 0; i < m_fldValue.bstrValList.nCount; i++ )
				{
					free(m_fldValue.bstrValList.paList[i]);
				}

				SMT_SAFE_DELETE_A(m_fldValue.bstrValList.paList);

			}
			break;

		case   SmtBinary:
			if (m_fldValue.blobVal.paData != NULL)
			{
				SMT_SAFE_DELETE_A(m_fldValue.blobVal.paData);
				m_fldValue.blobVal.nCount = 0;
			}
			break;

		default:
			break;
		}

		m_fldValue.usReserved1 = SmtUnsetMarker;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtField::SetName(const char * szName)
	{
		sprintf_s(m_fldName,SMT_FIELD_NAME_LENGTH,"%s",szName);
	}

	void SmtField::SetType(varType uVt)
	{
		m_fldValue.Vt = uVt;

		switch (m_fldValue.Vt)
		{
		case   SmtString :
			    m_fldValue.bstrVal = NULL;
			break;

		case   SmtIntegerList:
			    m_fldValue.iValList.paList = NULL;
				m_fldValue.iValList.nCount = 0;
			break;	

		case   SmtRealList:
			   m_fldValue.dbfValList.paList = NULL;
			   m_fldValue.dbfValList.nCount = 0;
			break;

		case   SmtStringList:
			{
				m_fldValue.bstrValList.paList = NULL;
				m_fldValue.bstrValList.nCount = 0;
			}
			break;

		case   SmtBinary:
			m_fldValue.blobVal.paData = NULL;
			m_fldValue.blobVal.nCount = 0;
			break;
		default:
			break;
		}
	}

	SmtField *SmtField::Clone() const
	{
		SmtField *pAttFld = new SmtField();

		pAttFld->SetName(m_fldName);
		pAttFld->SetType(m_fldValue.Vt);
		pAttFld->SetValue(m_fldValue);

		return pAttFld;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtField::SetValue(const SmtVariant &smtFld)
	{
		if(m_fldValue.Vt != smtFld.Vt)
			return SMT_ERR_FAILURE;

		UnSetField();

		switch (m_fldValue.Vt)
		{
		case   SmtInteger:
		case   SmtReal:	
		case   SmtBool :	
		case   SmtByte :
			m_fldValue = smtFld;
			break;

		case   SmtString :
			if (NULL == smtFld.bstrVal)
				return SMT_ERR_FAILURE;

			m_fldValue.bstrVal = strdup(smtFld.bstrVal);
			break;

		case   SmtIntegerList:
			{
				if (NULL == smtFld.iValList.paList)
					return SMT_ERR_FAILURE;

				m_fldValue.iValList.paList = new int[smtFld.iValList.nCount];	
				memcpy(m_fldValue.iValList.paList,smtFld.iValList.paList,sizeof(int)*smtFld.iValList.nCount);
				m_fldValue.iValList.nCount = smtFld.iValList.nCount;
			}
			break;	

		case   SmtRealList:
			{
				if (NULL == smtFld.dbfValList.paList)
					return SMT_ERR_FAILURE;

				m_fldValue.dbfValList.paList = new double[smtFld.dbfValList.nCount];
				memcpy(m_fldValue.dbfValList.paList,smtFld.dbfValList.paList,sizeof(double)*smtFld.dbfValList.nCount);
				m_fldValue.dbfValList.nCount = smtFld.dbfValList.nCount;
			}
			break;

		case   SmtStringList:
			{
				if (NULL == smtFld.bstrValList.paList)
					return SMT_ERR_FAILURE;

				m_fldValue.bstrValList.paList = new char*[smtFld.bstrValList.nCount+1];
				m_fldValue.bstrValList.paList = STR_Duplicate(smtFld.bstrValList.paList);
				m_fldValue.bstrValList.nCount = smtFld.bstrValList.nCount;

			}
			break;

		case   SmtBinary:
			{
				if (NULL == smtFld.blobVal.paData)
					return SMT_ERR_FAILURE;

				m_fldValue.blobVal.paData = new byte[smtFld.blobVal.nCount];		
				memcpy(m_fldValue.blobVal.paData,smtFld.blobVal.paData,sizeof(byte)*smtFld.blobVal.nCount);
				m_fldValue.blobVal.nCount = smtFld.blobVal.nCount;
			}
			break;

		case   SmtDate:
		case   SmtTime:
		case   SmtDateTime:
			memcpy(&m_fldValue.dateVal,&smtFld.dateVal,sizeof(smtFld.dateVal));
			break;

		case   SmtUnknown://未知类型
			break;

		default:
			break;
		}

		m_fldValue.usReserved1 = SmtSetMarker;

		return SMT_ERR_NONE;
	}

	int SmtField::SetValue( int nValue )
	{
		if( m_fldValue.Vt == SmtInteger )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.iVal = nValue;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( double dfValue )
	{
		if( m_fldValue.Vt == SmtReal )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.dbfVal = dfValue;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( byte bValue )
	{
		if( m_fldValue.Vt == SmtByte )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.byteVal = bValue;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( bool bValue )
	{
		if( m_fldValue.Vt == SmtBool )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.boolVal = bValue;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( const char * pszValue )
	{
		switch (m_fldValue.Vt)
		{
		case   SmtInteger:
			m_fldValue.iVal = atoi(pszValue);
			break;

		case   SmtReal:	
			m_fldValue.dbfVal = atof(pszValue);
			break;

		case   SmtBool:	
			{
				if( strcmp(pszValue,"true") == 0)
					m_fldValue.boolVal = true;
				else if (strcmp(pszValue,"false") == 0)
				{
					m_fldValue.boolVal = false;
				}
				else 
					m_fldValue.boolVal = false;		
			}	
			break;

		case   SmtString:
			m_fldValue.bstrVal = strdup(pszValue);
			break;

		default:
			return SMT_ERR_FAILURE;
		}

		m_fldValue.usReserved1 = SmtSetMarker;

		return SMT_ERR_NONE;
	}

	int SmtField::SetValue( int nCount, int * panValues )
	{
		if( m_fldValue.Vt == SmtIntegerList )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.iValList.nCount = nCount;
			uField.iValList.paList = panValues;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( int nCount, double * padfValues )
	{
		if( m_fldValue.Vt == SmtRealList )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.dbfValList.nCount = nCount;
			uField.dbfValList.paList = padfValues;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( char ** papszValues )
	{
		if( m_fldValue.Vt == SmtStringList)
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;

			uField.bstrValList.nCount = STR_Count(papszValues);
			uField.bstrValList.paList = papszValues;

			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( int nCount, byte * pabyBinary )
	{
		if( m_fldValue.Vt == SmtBinary )
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;
			uField.blobVal.nCount = nCount;
			uField.blobVal.paData = pabyBinary;
			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

	int SmtField::SetValue( int nYear, int nMonth, int nDay,int nHour, int nMinute, int nSecond, int nTZFlag )
	{

		if( m_fldValue.Vt == SmtDate || m_fldValue.Vt == SmtTime || m_fldValue.Vt == SmtDateTime)
		{
			SmtVariant        uField;
			uField.Vt = m_fldValue.Vt;

			uField.dateVal.Year = (short)nYear;
			uField.dateVal.Month = (byte)nMonth;
			uField.dateVal.Day = (byte)nDay;
			uField.dateVal.Hour = (byte)nHour;
			uField.dateVal.Minute = (byte)nMinute;
			uField.dateVal.Second = (byte)nSecond;
			uField.dateVal.TZFlag = (byte)nTZFlag;

			return SetValue(uField );
		}
		else
			return SMT_ERR_FAILURE;
	}

}