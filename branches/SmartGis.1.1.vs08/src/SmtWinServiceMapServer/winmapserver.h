/*
File:    winmapservice.h

Desc:    windows 服务，地图服务

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.211

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _WINMAPSERVER1_H
#define _WINMAPSERVER1_H

#define	SMT_WINSERVICE_NAME_LENGTH		50
#define	SMT_WINSERVICE_LOGNAME_LENGTH	50


#include "smt_winservice.h"
using namespace Smt_Core;

class SmtWinMapService:public SmtWinService
{
public:
	//继承类必须重写下面函数
	static long					New(SmtWinService *&pWinService,const char *szWinServiceName,const char *szWinServiceLog);			
	static long					Delete(SmtWinService *&pWinService);			

protected:
	SmtWinMapService(const char *szWinServiceName,const char *szWinServiceLog);
	virtual ~SmtWinMapService();

public:
	virtual long				UserRun();						//用户自行完成循环控制
	virtual long				UserCtrl(uchar dwOpcode);		//用户自行完成循环控制

};


#endif //_WINMAPSERVER1_H