/*
File:    smt_api.h

Desc:    SmartGis基础API

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_API_H
#define _SMT_API_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "smt_listener.h"

static const double dEPSILON	= 1E-5;
static const double dPI			= 3.1415926;

//////////////////////////////////////////////////////////////////////////
//Smt Variant Functions
bool		SMT_EXPORT_API		VarToBool(const Smt_Core::SmtVariant& var);
byte		SMT_EXPORT_API		VarToByte(const Smt_Core::SmtVariant& var);
short		SMT_EXPORT_API		VarToInteger(const Smt_Core::SmtVariant& var);
long		SMT_EXPORT_API		VarToLong(const Smt_Core::SmtVariant& var);
double		SMT_EXPORT_API		VarToDouble(const Smt_Core::SmtVariant& var);
const char	SMT_EXPORT_API     *VarToString(const Smt_Core::SmtVariant& var);

int			SMT_EXPORT_API		IntegerListToString(int nCount,int *pInteger,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);
int			SMT_EXPORT_API		StringListToString(int nCount,char**pStrings,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);
int			SMT_EXPORT_API		RealListToString(int nCount,double *pReal,int nStingBufLength = TEMP_BUFFER_SIZE,char * buf = NULL);

//////////////////////////////////////////////////////////////////////////
//Smt String Functions
int			SMT_EXPORT_API		STR_Count(char **papszStrList);
char		SMT_EXPORT_API		**STR_Duplicate(char **papszStrList);
uint		SMT_EXPORT_API		STR_Tokenize(const string& str, vector<string>& tokens, const string& delimiters);
//////////////////////////////////////////////////////////////////////////
//Smt Real Functions
bool		SMT_EXPORT_API		IsEqual(double a, double b, double eps);

//////////////////////////////////////////////////////////////////////////
//Smt Aux  Functions
void		SMT_EXPORT_API		SplitFileName(const char*fullname,char* path,char* fileName,char* title,char* ext);
void		SMT_EXPORT_API		GetParentDictory(const char*szCurDir,char*szParentDir,int iParent = 1);
string		SMT_EXPORT_API		GetAppPath(void);
string		SMT_EXPORT_API		GetAppTempPath(void);

long		SMT_EXPORT_API		CreateAllPathDirectory(string strDirPath);
long		SMT_EXPORT_API		DeleteDirectory(string strDirName);				//删除目录及其所有文件、子目录
long		SMT_EXPORT_API		GetTempName(string &strName);					//获取临时名称

//////////////////////////////////////////////////////////////////////////
void		SMT_EXPORT_API		lRectTofRect(Smt_Core::fRect &frect,Smt_Core::lRect lrect);
void		SMT_EXPORT_API		fRectTolRect(Smt_Core::lRect &lrect,Smt_Core::fRect frect);

void		SMT_EXPORT_API		AjustlRect(Smt_Core::lRect &lrect);
void		SMT_EXPORT_API		AjustfRect(Smt_Core::fRect &frect);

bool		SMT_EXPORT_API		IsInfRect(float x,float y,Smt_Core::fRect &frect);
bool		SMT_EXPORT_API		IsInlRect(long x,long y,Smt_Core::lRect &lrect);

//////////////////////////////////////////////////////////////////////////
long		SMT_EXPORT_API		GetInterpColor(long index,long internum,long r1,long g1,long b1,long r2,long g2,long b2);
long		SMT_EXPORT_API		GetRandomColor(void);

//////////////////////////////////////////////////////////////////////////
HMENU		SMT_EXPORT_API		CreateListenerMenu(Smt_Core::SmtListener*pListener,Smt_Core::SmtFuncItemStyle style);
void		SMT_EXPORT_API		AppendListenerMenu(HMENU hOwnwerMenu ,Smt_Core::SmtListener*pListener,\
												   Smt_Core::SmtFuncItemStyle style,bool bInsertSeprator = true);

//ximage type
long		SMT_EXPORT_API		GetImageTypeByFileExt(const char *szFileName);

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_API_H