#include "base/core/msg.h"

using namespace base;

long smt_post_listener_msg(SmtListener *pListener,long lMsg,SmtListenerMsg &param)
{
	SmtListenerManager * pListenerMgr = SmtListenerManager::get_singleton_ptr();
	return pListenerMgr->notify(pListener,lMsg,param);
}