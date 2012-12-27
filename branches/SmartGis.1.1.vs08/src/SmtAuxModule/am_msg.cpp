#include "am_msg.h"

long SmtPostAMMsg(SmtAuxModule *pAMoudule,long lMsg,SmtListenerMsg &param)
{
	SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
	return pAModuleMgr->Notify(pAMoudule,lMsg,param);
}