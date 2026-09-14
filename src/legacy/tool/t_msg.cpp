#include "legacy/tool/t_msg.h"

long post_ia_tool_msg(SmtIATool *pIATool,long lMsg,SmtListenerMsg &param)
{
	SmtIAToolManager * pToolMgr = SmtIAToolManager::get_singleton_ptr();
	return pToolMgr->notify(pIATool,lMsg,param);
}