#include "plugin/legacy/module.h"
#include "plugin/legacy/module_manager.h"

namespace plugin
{
	SmtAuxModule::SmtAuxModule()
	{
		Register();
	}

	SmtAuxModule::~SmtAuxModule()
	{
		UnRegister();
	}

	int SmtAuxModule::Register()
	{
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
		return pAModuleMgr->register_a_module(this);
	}

	int SmtAuxModule::RegisterMsg()
	{
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::get_singleton_ptr();
		pAModuleMgr->register_a_module_msg(this);

		return SMT_ERR_NONE;
	}

	int SmtAuxModule::UnRegister()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::get_singleton_ptr();
		return pIAToolMgr->remove_a_module(this);
	}

	int SmtAuxModule::UnRegisterMsg()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::get_singleton_ptr();
		pIAToolMgr->unregister_a_module_msg(this);

		return SMT_ERR_NONE;
	}

	int SmtAuxModule::SetActive()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::get_singleton_ptr();
		pIAToolMgr->set_active_a_module(this);

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtAuxModule::Init()
	{
		return SMT_ERR_NONE;
	}

	int SmtAuxModule::Destroy()
	{
		return SMT_ERR_NONE;
	}
}