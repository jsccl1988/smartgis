#include "smt_msg.h"

using namespace Smt_Core;

long SmtPostListenerMsg(SmtListener *pListener,long lMsg,SmtListenerMsg &param)
{
	SmtListenerManager * pListenerMgr = SmtListenerManager::GetSingletonPtr();
	return pListenerMgr->Notify(pListener,lMsg,param);
}