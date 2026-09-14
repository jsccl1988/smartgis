/*
File:    smt_command.h

Desc:    SmartGis ,����ģʽ ʵ��Undo/Redo

Version: Version 1.0

Writter:  �´���

Date:    2012.12.26

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_COMMAND_H
#define _SMT_COMMAND_H

#include "base/core/core.h"
#include <stack>

namespace base
{
	class CORE_EXPORT SmtCommandReceiver
	{
	public:
		SmtCommandReceiver() {}
		virtual ~SmtCommandReceiver() {}

	public:
		virtual bool		action(bool bUndo) = 0;
	};

	class CORE_EXPORT SmtCommand  
	{
	public:
		SmtCommand(SmtCommandReceiver * pReceiver, bool bAutoDelete = true);
		virtual ~SmtCommand(void);

	public:
		void				set_receiver(SmtCommandReceiver * pReceiver, bool bAutoDelete = true);
	public:
		virtual bool		execute(void);
		virtual bool		unexecute();

	protected:
		SmtCommand(const SmtCommand& rhs);
		SmtCommand& operator=(const SmtCommand& rhs);

	protected:
		SmtCommandReceiver	*m_pReceiver;
		bool				m_bAutoDeleteReceiver;
	};

	class CORE_EXPORT SmtMacroCommand : public SmtCommand
	{
	public:
		SmtMacroCommand();
		~SmtMacroCommand();

		virtual bool		execute();
		virtual bool		unexecute();

		void				add_command(SmtCommand * pCommand);
		void				delete_command(SmtCommand * pCommand);

	private:
		SmtMacroCommand(const SmtMacroCommand& rhs);
		SmtMacroCommand& operator=(const SmtMacroCommand& rhs);

	private:
		std::vector<SmtCommand *> m_vecCommands;
	};

	
	class CORE_EXPORT SmtCommandManager
	{
	public:
		SmtCommandManager(void);
		virtual ~SmtCommandManager();

	public:
		virtual bool		call_command(SmtCommand * pCommand);
		virtual void		clear_all_commands();

		virtual void		undo();
		virtual void		redo();

		virtual bool		can_undo() const;
		virtual bool		can_redo() const;

	public:
		void				push_undo_command(SmtCommand * pCommand);
		SmtCommand			*pop_undo_command();
		void				push_redo_command(SmtCommand * pCommand);
		SmtCommand			*pop_redo_command();
		void				delete_undo_commands();
		void				delete_redo_commands();
		
	private:
		std::stack<SmtCommand *> m_stackUndo;
		std::stack<SmtCommand *> m_stackRedo;
	};
}

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"platform_d.lib")
#       else
#          pragma comment(lib,"platform.lib")
#	    endif  
#endif

#endif //_SMT_COMMAND_H
