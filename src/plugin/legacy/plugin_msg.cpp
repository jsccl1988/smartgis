#include "plugin/legacy/plugin_msg.h"

long SmtPostAMMsg(SmtAuxModule *pAMoudule,long lMsg,SmtListenerMsg &param)
{
	SmtAModuleManager * pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
	return pAModuleMgr->notify(pAMoudule,lMsg,param);
}