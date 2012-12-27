#include "am_amodule.h"
#include "am_amodulemanager.h"

namespace Smt_AM
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
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		return pAModuleMgr->RegisterAModule(this);
	}

	int SmtAuxModule::RegisterMsg()
	{
		SmtAModuleManager * pAModuleMgr = SmtAModuleManager::GetSingletonPtr();
		pAModuleMgr->RegisterAModuleMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtAuxModule::UnRegister()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::GetSingletonPtr();
		return pIAToolMgr->RemoveAModule(this);
	}

	int SmtAuxModule::UnRegisterMsg()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::GetSingletonPtr();
		pIAToolMgr->UnRegisterAModuleMsg(this);

		return SMT_ERR_NONE;
	}

	int SmtAuxModule::SetActive()
	{
		SmtAModuleManager * pIAToolMgr = SmtAModuleManager::GetSingletonPtr();
		pIAToolMgr->SetActiveAModule(this);

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