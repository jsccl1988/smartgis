#include "base/core/command.h"
#include <algorithm>

namespace base
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

	void SmtCommand::set_receiver(SmtCommandReceiver * pReceiver, bool bAutoDelete)
	{
		m_pReceiver = pReceiver;
		m_bAutoDeleteReceiver = bAutoDelete;
	}

	bool SmtCommand::execute(void) 
	{
		if (m_pReceiver)
		{
			return m_pReceiver->action(false);
		}

		return false;
	}

	bool SmtCommand::unexecute()
	{
		if (m_pReceiver)
		{
			return m_pReceiver->action(true);
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

	bool SmtMacroCommand::execute()
	{
		vector<SmtCommand *>::iterator iter = m_vecCommands.begin() ;

		while (iter != m_vecCommands.end())
		{
			if (!(*iter)->execute())
			{
				return false;
			}
			
			++iter;
		}

		return true;
	}

	bool SmtMacroCommand::unexecute()
	{
		vector<SmtCommand *>::iterator iter = m_vecCommands.begin() ;

		while (iter != m_vecCommands.end())
		{
			if (!(*iter)->unexecute())
			{
				return false;
			}

			++iter;
		}

		return true;
	}

	void SmtMacroCommand::add_command(SmtCommand * pCommand)
	{
		if (pCommand)
			m_vecCommands.push_back(pCommand);
	}

	void SmtMacroCommand::delete_command(SmtCommand * pCommand)
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

	bool SmtCommandManager::call_command(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			if (pCommand->execute())
			{
				push_undo_command(pCommand);
				delete_redo_commands();

				return true;
			}
			else
			{
				delete pCommand;
			}
		}

		return false;
	}

	void SmtCommandManager::clear_all_commands()
	{
		delete_undo_commands();
		delete_redo_commands();
	}

	void SmtCommandManager::undo()
	{
		SmtCommand * pCommand = pop_undo_command();
		if (pCommand)
		{
			if (pCommand->unexecute())
			{
				push_redo_command(pCommand);
			}
			else
			{
				delete pCommand;
			}
		}
	}

	void SmtCommandManager::redo()
	{
		SmtCommand * pCommand = pop_redo_command();
		if (pCommand)
		{
			if (pCommand->execute())
			{
				push_undo_command(pCommand);
			}
			else
			{
				delete pCommand;
			}
		}
	}

	bool SmtCommandManager::can_undo() const
	{
		return !m_stackUndo.empty();
	}

	bool SmtCommandManager::can_redo() const
	{
		return !m_stackRedo.empty();
	}

	void SmtCommandManager::push_undo_command(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			m_stackUndo.push(pCommand);
		}
	}

	SmtCommand * SmtCommandManager::pop_undo_command()
	{
		SmtCommand * pCommand = NULL;
		if (!m_stackUndo.empty())
		{
			pCommand = m_stackUndo.top();
			m_stackUndo.pop();
		}
		return pCommand;
	}

	void SmtCommandManager::push_redo_command(SmtCommand * pCommand)
	{
		if (pCommand)
		{
			m_stackRedo.push(pCommand);
		}
	}

	SmtCommand * SmtCommandManager::pop_redo_command()
	{
		SmtCommand * pCommand = NULL;
		if (!m_stackRedo.empty())
		{
			pCommand = m_stackRedo.top();
			m_stackRedo.pop();
		}

		return pCommand;
	}

	void SmtCommandManager::delete_undo_commands()
	{
		while (!m_stackUndo.empty())
		{
			delete m_stackUndo.top();
			m_stackUndo.pop();
		}
	}

	void SmtCommandManager::delete_redo_commands()
	{
		while (!m_stackRedo.empty())
		{
			delete m_stackRedo.top();
			m_stackRedo.pop();
		}
	}
}