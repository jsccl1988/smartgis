#include <math.h>

#include "base/core/api.h"
#include "ximage.h"

using namespace base;

//////////////////////////////////////////////////////////////////////////
bool var_to_bool(const SmtVariant& var)
{
	bool value = false;

	switch (var.Vt)
	{
	case SmtBool:
		value = var.boolVal ? true : false;
		break;

	default:
		break;
	}

	return value;
}

byte var_to_byte(const SmtVariant& var)
{
	byte value = 0;

	switch (var.Vt)
	{
	case SmtByte:
		value = var.byteVal;
		break;

	default:
		break;
	}	

	return value;
}

short var_to_integer(const SmtVariant& var)
{
	short value = 0;

	switch (var.Vt)
	{
	case   SmtInteger:
		value = var.iVal;
		break;

	case   SmtReal:
		value = (short)(var.dbfVal + 0.5);
		break;

	case   SmtUnknown://δ֪����
		value = 0;
		break;

	default:
		value = 0;
		break;
	}

	return value;
}

long var_to_long(const SmtVariant& var)
{
	long value = 0;

	switch (var.Vt)
	{
	case   SmtInteger:
		value = var.iVal;
		break;

	case   SmtReal:
		value = (long)(var.dbfVal + 0.5);
		break;

	case   SmtUnknown://δ֪����
		value = 0;
		break;

	default:
		value = 0;
		break;
	}

	return value;
}

double var_to_double(const SmtVariant& var)
{
	double value = 0;

	switch (var.Vt)
	{
		case   SmtInteger:
			value = double(var.iVal);
			break;

		case   SmtReal:
			value = var.dbfVal;
			break;

		case   SmtString:
			value = atof(var.bstrVal);
			break;

		case   SmtUnknown://δ֪����
			value = 0;
			break;

		default:
			value = 0;
			break;
	}

	return value;
}

const char * var_to_string(const SmtVariant &var)
{
	static char szBuf[TEMP_BUFFER_SIZE];
	memset(szBuf,'\0',TEMP_BUFFER_SIZE);

	switch (var.Vt)
	{
	case   SmtInteger:
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%d", var.iVal);
		break;

	case   SmtReal:
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%.8f", var.dbfVal);
		break;

	case   SmtBool :
		sprintf(szBuf,var.boolVal?"true":"false");
		break;

	case   SmtByte :
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%d", var.byteVal);
		break;

	case   SmtString :
		sprintf(szBuf,var.bstrVal);
		break;

	case   SmtIntegerList:
		integer_list_to_string(var.iValList.nCount,var.iValList.paList,TEMP_BUFFER_SIZE,szBuf);	
		break;	

	case   SmtRealList:
		real_list_to_string(var.dbfValList.nCount,var.dbfValList.paList,TEMP_BUFFER_SIZE,szBuf);	
		break;

	case   SmtStringList:
		string_list_to_string(var.bstrValList.nCount,var.bstrValList.paList,TEMP_BUFFER_SIZE,szBuf);	
		break;

	case   SmtBinary:
		strcpy(szBuf,"binary");
		break;

	case   SmtDate:
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%04d/%02d/%02d", var.dateVal.Year,var.dateVal.Month,var.dateVal.Day);
		break;

	case   SmtTime:
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%2d:%02d:%02d", var.dateVal.Hour,var.dateVal.Minute,var.dateVal.Second);
		break;

	case   SmtDateTime:
		snprintf( szBuf, TEMP_BUFFER_SIZE, "%04d/%02d/%02d %2d:%02d:%02d", var.dateVal.Year,var.dateVal.Month,var.dateVal.Day,var.dateVal.Hour,var.dateVal.Minute,var.dateVal.Second);
		break;

	case   SmtUnknown://δ֪����
		break;

	default:
		strcpy(szBuf,"unknown");
		break;
	}

	return szBuf;
}

int integer_list_to_string(int nCount,int *pInteger,int nStingBufLength,char * szBuf)
{
	if (NULL == szBuf)
		return SMT_ERR_INVALID_PARAM;

	char    szItem[40];
//	char    szFormat[64];
	int     i;

	snprintf( szBuf, nStingBufLength, "(%d:", nCount );
	for( i = 0; i < nCount; i++ )
	{
		snprintf( szItem, sizeof(szItem), "%d",pInteger[i] );
		if( strlen(szBuf) + strlen(szItem) + 6 >= sizeof(szBuf) )
		{
			break;
		}

		if( i > 0 )
			strcat(szBuf, ",");

		strcat(szBuf, szItem);
	}

	if( i < nCount )
		strcat(szBuf,",...)");
	else
		strcat(szBuf,")");

	return SMT_ERR_NONE;
}

int string_list_to_string(int nCount,char**pStrings,int nStingBufLength,char * szBuf)
{
	if (NULL == szBuf)
		return SMT_ERR_INVALID_PARAM;

	int i;
	snprintf( szBuf, nStingBufLength, "(%d:", nCount );

	for(i = 0; i < nCount; i++ )
	{
		const char  *pszItem = pStrings[i];

		if( strlen(szBuf) + strlen(pszItem)  + 6 >= sizeof(szBuf) )
		{
			break;
		}

		if( i > 0 )
			strcat( szBuf, "," );

		strcat( szBuf, pszItem );
	}

	if( i < nCount )
		strcat( szBuf, ",...)" );
	else
		strcat( szBuf, ")" );

	return SMT_ERR_NONE;
}

int real_list_to_string(int nCount,double *pReal,int nStingBufLength,char * szBuf)
{
	if (NULL == szBuf)
		return SMT_ERR_INVALID_PARAM;

	char    szItem[40];
//	char    szFormat[64];
	int     i;

	snprintf( szBuf, nStingBufLength, "(%d:", nCount );
	for( i = 0; i < nCount; i++ )
	{
		snprintf( szItem, sizeof(szItem), "%.8f",pReal[i] );
		if( strlen(szBuf) + strlen(szItem) + 6 >= sizeof(szBuf) )
		{
			break;
		}

		if(i > 0)
			strcat(szBuf,",");

		strcat(szBuf,szItem);
	}

	if(i < nCount)
		strcat(szBuf,",...)");
	else
		strcat(szBuf,")");

	return SMT_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////
int str_count(char **papszStrList)
{
	int nItems=0;

	if (papszStrList)
	{
		while(*papszStrList != NULL)
		{
			nItems++;
			papszStrList++;
		}
	}

	return nItems;
}

char **str_duplicate(char **papszStrList)
{
	char **papszNewList, **papszSrc, **papszDst;
	int  nLines;

	nLines = str_count(papszStrList);

	if (nLines == 0)
		return NULL;

	papszNewList = new char*[nLines+1];;
	papszSrc = papszStrList;
	papszDst = papszNewList;

	while(*papszSrc != NULL)
	{
		*papszDst = strdup(*papszSrc);

		papszSrc++;
		papszDst++;
	}
	*papszDst = NULL;

	return papszNewList;
}

uint str_tokenize(const string& str, vector<string>& tokens, const string& delimiters)
{
	// Skip delimiters at beginning.
	string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	string::size_type pos     = str.find_first_of(delimiters, lastPos);

	while (string::npos != pos || string::npos != lastPos)
	{
		// Found a token, add it to the vector.
		string tmp = str.substr(lastPos, pos - lastPos);
		if (tmp.size() > 0  && tmp[0] !=' ')
			tokens.push_back(tmp);
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}

	return tokens.size();
}

//////////////////////////////////////////////////////////////////////////
bool  is_equal(double a, double b, double eps) 
{
	return (fabs(a - b) < eps);
}


//////////////////////////////////////////////////////////////////////////
void  l_rect_to_f_rect(fRect &frect,lRect lrect)
{
	frect.lb.x = lrect.lb.x;
	frect.lb.y = lrect.lb.y;

	frect.rt.x = lrect.rt.x;
	frect.rt.y = lrect.rt.y;
}

void  f_rect_to_l_rect(lRect &lrect,fRect frect)
{
	lrect.lb.x = long(frect.lb.x+0.5);
	lrect.lb.y = long(frect.lb.y+0.5);

	lrect.rt.x = long(frect.rt.x+0.5);
	lrect.rt.y = long(frect.rt.y+0.5);
}

void adjust_l_rect(lRect &lrect)
{
	lRect tmpRect = lrect;

	lrect.lb.x = min(tmpRect.lb.x,tmpRect.rt.x);
	lrect.lb.y = min(tmpRect.lb.y,tmpRect.rt.y);

	lrect.rt.x = max(tmpRect.lb.x,tmpRect.rt.x);
	lrect.rt.y = max(tmpRect.lb.y,tmpRect.rt.y);
}

void adjust_f_rect(fRect &frect)
{
	fRect tmpRect = frect;

	frect.lb.x = min(tmpRect.lb.x,tmpRect.rt.x);
	frect.lb.y = min(tmpRect.lb.y,tmpRect.rt.y);

	frect.rt.x = max(tmpRect.lb.x,tmpRect.rt.x);
	frect.rt.y = max(tmpRect.lb.y,tmpRect.rt.y);
}

bool is_in_f_rect(float x,float y,fRect &frect)
{
	return frect.lb.x <= x && frect.lb.y <= y &&
		frect.rt.x >= x && frect.rt.y >= y;
}

bool is_in_l_rect(long x,long y,lRect &lrect)
{
	return lrect.lb.x <= x && lrect.lb.y <= y &&
		lrect.rt.x >= x && lrect.rt.y >= y;
}

//////////////////////////////////////////////////////////////////////////
void  split_file_name(const char*fullname,char* path,char* fileName,char* title,char* ext)
{
	char szDir [_MAX_DIR];
	char szDrive [_MAX_DRIVE];
	_splitpath (fullname, szDrive,szDir,title,ext);
	if(path !=NULL)
	{
		strcpy(path,szDrive);
		strcat(path,szDir);
	}
	if(fileName !=NULL)
	{
		strcpy(fileName,title);
		strcat(fileName,ext);
	}
}

void	get_parent_directory(const char*szCurDir,char*szParentDir,int iParent)
{
	if (iParent  < 1)
	{
		strcpy(szParentDir,szCurDir);
	}
	 
	string strPath = szCurDir;
	int i = iParent+1;
	while(i > 0 && !strPath.empty())
	{
		strPath = strPath.substr(0,strPath.rfind('\\'));
		i--;
	}
	sprintf(szParentDir,"%s\\",strPath.c_str());
}

string	get_app_path(void)
{
	char szModulePath[MAX_PATH];
	string strAppPath0;
	string strAppPath1;


	GetModuleFileName(NULL,szModulePath, MAX_PATH);

	strAppPath0 = szModulePath;

	int nPos = strAppPath0.rfind('\\');
	if (nPos != string::npos)
	{
		strAppPath1 = strAppPath0.substr(0,nPos+1);
	}
	else
	{
		strAppPath1 = "";
	}

	return strAppPath1;
}

long create_all_path_directory(string strDirPath)
{//��\����
	bool bFlag = true;
	int  nLength=strDirPath.length();
	if (nLength<=3)
		bFlag =  false;

	string strCurPath="";
	strCurPath+=strDirPath[0];
	strCurPath+=strDirPath[1];
	strCurPath+=strDirPath[2];

	for (int i=3;i<nLength;i++)
	{
		string str;
		str = strDirPath[i];
		if (str == "\\" || bFlag)
		{
			bFlag = CreateDirectory(strCurPath.c_str(),NULL);
		}

		strCurPath+=str;
	}

	//bFlag = CreateDirectory(strCurPath.c_str(),NULL);

	return bFlag?SMT_ERR_NONE:SMT_ERR_FAILURE;
}

long delete_directory(string strDirName)//ɾ��Ŀ¼���������ļ�����Ŀ¼
{//��\����
	char tempFileFind[MAX_PATH];
	sprintf(tempFileFind,"%s*.*",strDirName.c_str());

	WIN32_FIND_DATA fileFindData;
	HANDLE hFind = ::FindFirstFile(tempFileFind, &fileFindData);            //�ҵ���һ��

	if (hFind == INVALID_HANDLE_VALUE)       
	{ //���û���ҵ���ص��ļ���Ϣ������false
		return SMT_ERR_FAILURE;
	}

	do 
	{//����֮�������һ����ֱ��������
		if (fileFindData.cFileName[0] == '.')
		{//��Ϊ�ļ��п�ʼ��"."��".."����Ŀ¼��Ҫ���˵�
			continue;            
		}

		if((fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			char tempDir[MAX_PATH];
			sprintf(tempDir,"%s%s",strDirName.c_str(),fileFindData.cFileName);
			delete_directory(tempDir);
		}
		else
		{
			char tempFileName[MAX_PATH];
			sprintf(tempFileName,"%s%s",strDirName.c_str(),fileFindData.cFileName);
			DeleteFile(tempFileName);
		}

	}while (::FindNextFile(hFind, &fileFindData)); 

	FindClose(hFind);                //�رղ��Ҿ��

	strDirName = strDirName.substr(0,strDirName.length()-1);

	if(!RemoveDirectory(strDirName.c_str()))
	{
		return SMT_ERR_FAILURE;
	}

	return SMT_ERR_NONE;
}


string	get_app_temp_path(void)
{
	static char szModulePath[MAX_PATH];
	string strAppPath = get_app_path();

	strAppPath += "temp\\";
	
	return strAppPath;
}

long	get_temp_name(string &strName)
{
	static char szBuf[MAX_NAME_LENGTH];

	SYSTEMTIME time;
	::GetSystemTime(&time);
	sprintf_s(szBuf, MAX_NAME_LENGTH, _T("%s_%d%d%d%d"),
		strName.c_str(), time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	strName = szBuf;

	return SMT_ERR_NONE;

}
///////////////////////////////////////////////////////////////////////////////////////
long get_interp_color(long index,long internum,long r1,long g1,long b1,long r2,long g2,long b2)
{
	index = index%internum;

	long lclr = 0;
	double r = r2-r1,g = g2-g1,b= b2-b1;
	double dr  = r/internum,dg = g/internum,db = b/internum;
	
	lclr = RGB(dr*index+r1,dg*index+g1,db*index+b1);

	return lclr;
}

long get_random_color(void)
{
	int red = 1 + ( int )( 255.0 * rand() / ( RAND_MAX + 1.0 ) );
	int green = 1 + ( int )( 255.0 * rand() / ( RAND_MAX + 1.0 ) );
	int blue = 1 + ( int )( 255.0 * rand() / ( RAND_MAX + 1.0 ) );
	return RGB(red,green,blue);
}

//////////////////////////////////////////////////////////////////////////
HMENU	create_listener_menu(SmtListener*pListener,SmtFuncItemStyle style)
{
	if (NULL == pListener)
		return NULL;

	HMENU hMenu = ::CreatePopupMenu();
	if (NULL == hMenu)
		return NULL;

	vSmtFuncItems vFuncItems = pListener->get_func_items(style);
	vSmtFuncItems::iterator i = vFuncItems.begin();	
	while (i != vFuncItems.end())
	{
		// szName is char[]; leftover TUs are MBCS. Never use the TCHAR
		// AppendMenu macro — UNICODE leftover mixes produce mojibake/AV.
		::AppendMenuA(hMenu,MF_STRING,static_cast<UINT_PTR>((*i).lMsg),(*i).szName);
		++i;
	}

	return hMenu;
}

void	append_listener_menu(HMENU hOwnwerMenu ,SmtListener*pListener,SmtFuncItemStyle style,bool bInsertSeprator)
{
	if (NULL == hOwnwerMenu || NULL == pListener)
	{
		return;
	}

	if (bInsertSeprator)
	{
		::AppendMenuA(hOwnwerMenu,MF_SEPARATOR,0,NULL);
	}
	
	vSmtFuncItems vFuncItems = pListener->get_func_items(style);
	vSmtFuncItems::iterator i = vFuncItems.begin();	
	while (i != vFuncItems.end())
	{		
		::AppendMenuA(hOwnwerMenu,MF_STRING,static_cast<UINT_PTR>((*i).lMsg),(*i).szName);
		++i;
	}
}

bool	append_popup_menu(HMENU owner, HMENU popup, const char* name)
{
	if (NULL == owner || NULL == popup || NULL == name)
		return false;
	return ::AppendMenuA(owner, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), name) != FALSE;
}

bool	insert_popup_menu(HMENU owner, UINT position, HMENU popup,
			const char* name, UINT extra_flags)
{
	if (NULL == owner || NULL == popup || NULL == name)
		return false;
	return ::InsertMenuA(owner, position, MF_POPUP | extra_flags,
			     reinterpret_cast<UINT_PTR>(popup), name) != FALSE;
}

bool	attach_listener_popup(HMENU owner, SmtListener* listener,
			SmtFuncItemStyle style, const char* name,
			int insert_at, UINT extra_flags)
{
	HMENU popup = create_listener_menu(listener, style);
	if (NULL == popup)
		return false;
	if (::GetMenuItemCount(popup) <= 0)
	{
		::DestroyMenu(popup);
		return false;
	}
	const char* caption = (name && name[0]) ? name :
			      (listener ? listener->get_name() : NULL);
	if (NULL == caption || caption[0] == 0)
		caption = "";
	const bool ok = (insert_at < 0)
			    ? append_popup_menu(owner, popup, caption)
			    : insert_popup_menu(owner, static_cast<UINT>(insert_at),
						popup, caption, extra_flags);
	if (!ok)
		::DestroyMenu(popup);
	return ok;
}

//////////////////////////////////////////////////////////////////////////
long	get_image_type_by_file_ext(const char *szFileName)
{
	long nImageTyle = CXIMAGE_FORMAT_UNKNOWN;

	string strFileName = strlwr((char*)szFileName);
	int nPos = strFileName.rfind('.');
	if (nPos == string::npos)
		return CXIMAGE_FORMAT_UNKNOWN;

	string strExt = strFileName.substr(nPos+1,strFileName.size());
	
	if (stricmp(strExt.c_str(),"bmp") == 0)
		nImageTyle = CXIMAGE_FORMAT_BMP;
	else if (stricmp(strExt.c_str(),"gif") == 0)
		nImageTyle = CXIMAGE_FORMAT_GIF;
#if CXIMAGE_SUPPORT_JPG
	else if (stricmp(strExt.c_str(),"jpg") == 0)
		nImageTyle = CXIMAGE_FORMAT_JPG;
#endif
#if CXIMAGE_SUPPORT_PNG
	else if (stricmp(strExt.c_str(),"png") == 0)
		nImageTyle = CXIMAGE_FORMAT_PNG;
#endif
#if CXIMAGE_SUPPORT_MNG
	else if (stricmp(strExt.c_str(),"mng") == 0)
		nImageTyle = CXIMAGE_FORMAT_MNG;
#endif
#if CXIMAGE_SUPPORT_ICO
	else if (stricmp(strExt.c_str(),"ico") == 0)
		nImageTyle = CXIMAGE_FORMAT_ICO;
#endif
#if CXIMAGE_SUPPORT_TIF
	else if (stricmp(strExt.c_str(),"tif") == 0)
		nImageTyle = CXIMAGE_FORMAT_TIF;
#endif
#if CXIMAGE_SUPPORT_TGA
	else if (stricmp(strExt.c_str(),"tga") == 0)
		nImageTyle = CXIMAGE_FORMAT_TGA;
#endif
#if CXIMAGE_SUPPORT_PCX
	else if (stricmp(strExt.c_str(),"pcx") == 0)
		nImageTyle = CXIMAGE_FORMAT_PCX;
#endif
#if CXIMAGE_SUPPORT_WBMP
	else if (stricmp(strExt.c_str(),"wbmp") == 0)
		nImageTyle = CXIMAGE_FORMAT_WBMP;
#endif
#if CXIMAGE_SUPPORT_WMF
	else if (stricmp(strExt.c_str(),"wmf") == 0)
		nImageTyle = CXIMAGE_FORMAT_WMF;
#endif
#if CXIMAGE_SUPPORT_JPC
	else if (stricmp(strExt.c_str(),"jpc") == 0)
		nImageTyle = CXIMAGE_FORMAT_JPC;
#endif
#if CXIMAGE_SUPPORT_JP2
	else if (stricmp(strExt.c_str(),"jp2") == 0)
		nImageTyle = CXIMAGE_FORMAT_JP2;
#endif
#if CXIMAGE_SUPPORT_PGX
	else if (stricmp(strExt.c_str(),"pgx") == 0)
		nImageTyle = CXIMAGE_FORMAT_PGX;
#endif
#if CXIMAGE_SUPPORT_PNM
	else if (stricmp(strExt.c_str(),"pnm") == 0)
		nImageTyle = CXIMAGE_FORMAT_PNM;
#endif
#if CXIMAGE_SUPPORT_RAS
	else if (stricmp(strExt.c_str(),"ras") == 0)
		nImageTyle = CXIMAGE_FORMAT_RAS;
#endif

	return nImageTyle;
}
