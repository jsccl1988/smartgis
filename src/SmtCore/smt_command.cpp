#include "smt_command.h"
#include <algorithm>

namespace Smt_Core
{
	//////////////////////////////////////////////////////////////////////////
	//SmtCommandReceiver

	//////////////////////////////////////////////////////////////////////////
	//SmtCommand
	SmtCommand::SmtCommand(SmtCommandReceiver * pReceiver, bool bAutoDelete):m_pReceiver(pReceiver)
		,m_bAutoDeleteReceiver(bAutoDelete)
	{

	}

	SmtCommand::~SmtCommand(void)
	{
		if (m_bAutoDeleteReceiver)
		{
			SMT_SAFE_DELETE(m_pReceiver);
		}
	}

	void SmtCommand::SetReceiver(SmtCommandReceiver * pReceiver, bool bAutoDelete)
	{
		m_pReceiver = pReceiver;
		m_bAutoDeleteReceiver = bAutoDelete;
	}

	bool SmtCommand::Execute(void) 
	{
		if (m_pReceiver)
		{
			return m_pReceiver->Action(false);
		}

		return false;
	}

	bool SmtCommand::Unexecute()
	{
		if (m_pReceiver)
		{
			return m_pReceiver->Action(true);
		}

		return false;
	}


	SmtCommand::SmtCommand(const SmtCommand& rhs)
	{

	}

	SmtCommand& SmtCommand::operator=(const SmtCommand& rhs)
	{
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	//SmtCommand
	SmtMacroCommand::SmtMacroCommand():SmtCommand(NULL,false)
	{
	}

	SmtMacroCommand::~SmtMacroCommand()
	{
		vector<SmtCommand *>::iterator iter = m_vecCommands.begin() ;

		while (iter != m_vecCommands.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}

		m_vecCommands.clear();
	}

	bool SmtMacroCommand::Execute()
	{
		vector<SmtCommand *>::iterator iter = m_vecCommands.begin() ;

		while (iter != m_vecCommands.end())
		{
			if (!(*iter)->Execute())
			{
				return false;
			}
			
			++iter;
		}

		return true;
	}

	bool SmtMacroCommand::Unexecute()
	{
		vector<SmtCommand *>::iterator iter = m_vecCommands.begin() ;

		while (iter != m_vecCommands.end())
		{
			if (!(*iter)->Unexecute())
			{
				return false;
			}

			++iter;
		}

		return true;
	}

	void SmtMacroCommand::AddCommand(SmtCommand * pCommand)
	{
		if (pCommand)
			m_vecCommands.push_back(pCommand);
	}

	void SmtMacroCommand::DeleteCommand(SmtCommand * pCommand)
	{
		if (pCommand)
			m_vecCommands.erase(std::remove(m_vecCommands.begin(), m_vecCommands.end(), pCommand));
	}


	//////////////////////////////////////////////////////////////////////////
	//SmtCommandManager
	SmtCommandManager::SmtCommandManager(void) 
	{

	}

	SmtCommandManager::~SmtCommandManager() 
	{

	}

	bool SmtCommandManager::CallCommand(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			if (pCommand->Execute())
			{
				PushUndoCommand(pCommand);
				DeleteRedoCommands();

				return true;
			}
			else
			{
				delete pCommand;
			}
		}

		return false;
	}

	void SmtCommandManager::ClearAllCommands()
	{
		DeleteUndoCommands();
		DeleteRedoCommands();
	}

	void SmtCommandManager::Undo()
	{
		SmtCommand * pCommand = PopUndoCommand();
		if (pCommand)
		{
			if (pCommand->Unexecute())
			{
				PushRedoCommand(pCommand);
			}
			else
			{
				delete pCommand;
			}
		}
	}

	void SmtCommandManager::Redo()
	{
		SmtCommand * pCommand = PopRedoCommand();
		if (pCommand)
		{
			if (pCommand->Execute())
			{
				PushUndoCommand(pCommand);
			}
			else
			{
				delete pCommand;
			}
		}
	}

	bool SmtCommandManager::CanUndo() const
	{
		return !m_stackUndo.empty();
	}

	bool SmtCommandManager::CanRedo() const
	{
		return !m_stackRedo.empty();
	}

	void SmtCommandManager::PushUndoCommand(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			m_stackUndo.push(pCommand);
		}
	}

	SmtCommand * SmtCommandManager::PopUndoCommand()
	{
		SmtCommand * pCommand = NULL;
		if (!m_stackUndo.empty())
		{
			pCommand = m_stackUndo.top();
			m_stackUndo.pop();
		}
		return pCommand;
	}

	void SmtCommandManager::PushRedoCommand(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			m_stackRedo.push(pCommand);
		}
	}

	SmtCommand * SmtCommandManager::PopRedoCommand()
	{
		SmtCommand * pCommand = NULL;
		if (!m_stackRedo.empty())
		{
			pCommand = m_stackRedo.top();
			m_stackRedo.pop();
		}

		return pCommand;
	}

	void SmtCommandManager::DeleteUndoCommands()
	{
		while (!m_stackUndo.empty())
		{
			delete m_stackUndo.top();
			m_stackUndo.pop();
		}
	}

	void SmtCommandManager::DeleteRedoCommands()
	{
		while (!m_stackRedo.empty())
		{
			delete m_stackRedo.top();
			m_stackRedo.pop();
		}
	}
}