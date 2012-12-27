#include "t_msg.h"

long SmtPostIAToolMsg(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param)
{
	SmtIAToolManager * pToolMgr = SmtIAToolManager::GetSingletonPtr();
	return pToolMgr->Notify(pIATool,lMsg,param);
}