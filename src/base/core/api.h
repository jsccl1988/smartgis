/*
File:    smt_api.h

Desc:    SmartGis����API

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_API_H
#define _SMT_API_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"
#include "base/core/listener.h"

static const double dEPSILON	= 1E-5;
static const double dPI			= 3.1415926;

//////////////////////////////////////////////////////////////////////////
//Smt Variant Functions
bool		CORE_EXPORT		var_to_bool(const base::SmtVariant& var);
byte		CORE_EXPORT		var_to_byte(const base::SmtVariant& var);
short		CORE_EXPORT		var_to_integer(const base::SmtVariant& var);
long		CORE_EXPORT		var_to_long(const base::SmtVariant& var);
double		CORE_EXPORT		var_to_double(const base::SmtVariant& var);
const char	CORE_EXPORT     *var_to_string(const base::SmtVariant& var);

int			CORE_EXPORT		integer_list_to_string(int nCount,int *pInteger,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);
int			CORE_EXPORT		string_list_to_string(int nCount,char**pStrings,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);
int			CORE_EXPORT		real_list_to_string(int nCount,double *pReal,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);

//////////////////////////////////////////////////////////////////////////
//Smt String Functions
int			CORE_EXPORT		str_count(char **papszStrList);
char		CORE_EXPORT		**str_duplicate(char **papszStrList);
uint		CORE_EXPORT		str_tokenize(const string& str, vector<string>& tokens, const string& delimiters);
//////////////////////////////////////////////////////////////////////////
//Smt Real Functions
bool		CORE_EXPORT		is_equal(double a, double b, double eps);

//////////////////////////////////////////////////////////////////////////
//Smt Aux  Functions
void		CORE_EXPORT		split_file_name(const char*fullname,char* path,char* fileName,char* title,char* ext);
void		CORE_EXPORT		get_parent_directory(const char*szCurDir,char*szParentDir,int iParent = 1);
string		CORE_EXPORT		get_app_path(void);
string		CORE_EXPORT		get_app_temp_path(void);
inline string				GetAppPath(void) { return get_app_path(); }
inline string				GetAppTempPath(void) { return get_app_temp_path(); }

long		CORE_EXPORT		create_all_path_directory(string strDirPath);
long		CORE_EXPORT		delete_directory(string strDirName);				//ɾ��Ŀ¼���������ļ�����Ŀ¼
long		CORE_EXPORT		get_temp_name(string &strName);					//��ȡ��ʱ����

//////////////////////////////////////////////////////////////////////////
void		CORE_EXPORT		l_rect_to_f_rect(base::fRect &frect,base::lRect lrect);
void		CORE_EXPORT		f_rect_to_l_rect(base::lRect &lrect,base::fRect frect);

void		CORE_EXPORT		adjust_l_rect(base::lRect &lrect);
void		CORE_EXPORT		adjust_f_rect(base::fRect &frect);

bool		CORE_EXPORT		is_in_f_rect(float x,float y,base::fRect &frect);
bool		CORE_EXPORT		is_in_l_rect(long x,long y,base::lRect &lrect);

//////////////////////////////////////////////////////////////////////////
long		CORE_EXPORT		get_interp_color(long index,long internum,long r1,long g1,long b1,long r2,long g2,long b2);
long		CORE_EXPORT		get_random_color(void);

//////////////////////////////////////////////////////////////////////////
HMENU		CORE_EXPORT		create_listener_menu(base::SmtListener*pListener,base::SmtFuncItemStyle style);
void		CORE_EXPORT		append_listener_menu(HMENU hOwnwerMenu ,base::SmtListener*pListener,\
												   base::SmtFuncItemStyle style,bool bInsertSeprator = true);

//ximage type
long		CORE_EXPORT		get_image_type_by_file_ext(const char *szFileName);

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_API_H