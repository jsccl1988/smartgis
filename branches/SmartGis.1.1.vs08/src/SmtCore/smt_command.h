/*
File:    smt_command.h

Desc:    SmartGis ,命令模式 实现Undo/Redo

Version: Version 1.0

Writter:  陈春亮

Date:    2012.12.26

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_COMMAND_H
#define _SMT_COMMAND_H

#include "smt_core.h"
#include <stack>

namespace Smt_Core
{
	class SMT_EXPORT_CLASS SmtCommandReceiver
	{
	public:
		SmtCommandReceiver() {}
		virtual ~SmtCommandReceiver() {}

	public:
		virtual bool		Action(bool bUndo) = 0;
	};

	class SMT_EXPORT_CLASS SmtCommand  
	{
	public:
		SmtCommand(SmtCommandReceiver * pReceiver, bool bAutoDelete = true);
		virtual ~SmtCommand(void);

	public:
		void				SetReceiver(SmtCommandReceiver * pReceiver, bool bAutoDelete = true);
	public:
		virtual bool		Execute(void);
		virtual bool		Unexecute();

	protected:
		SmtCommand(const SmtCommand& rhs);
		SmtCommand& operator=(const SmtCommand& rhs);

	protected:
		SmtCommandReceiver	*m_pReceiver;
		bool				m_bAutoDeleteReceiver;
	};

	class SMT_EXPORT_CLASS SmtMacroCommand : public SmtCommand
	{
	public:
		SmtMacroCommand();
		~SmtMacroCommand();

		virtual bool		Execute();
		virtual bool		Unexecute();

		void				AddCommand(SmtCommand * pCommand);
		void				DeleteCommand(SmtCommand * pCommand);

	private:
		SmtMacroCommand(const SmtMacroCommand& rhs);
		SmtMacroCommand& operator=(const SmtMacroCommand& rhs);

	private:
		std::vector<SmtCommand *> m_vecCommands;
	};

	
	class SMT_EXPORT_CLASS SmtCommandManager
	{
	public:
		SmtCommandManager(void);
		virtual ~SmtCommandManager();

	public:
		virtual bool		CallCommand(SmtCommand * pCommand);
		virtual void		ClearAllCommands();

		virtual void		Undo();
		virtual void		Redo();

		virtual bool		CanUndo() const;
		virtual bool		CanRedo() const;

	public:
		void				PushUndoCommand(SmtCommand * pCommand);
		SmtCommand			*PopUndoCommand();
		void				PushRedoCommand(SmtCommand * pCommand);
		SmtCommand			*PopRedoCommand();
		void				DeleteUndoCommands();
		void				DeleteRedoCommands();
		
	private:
		std::stack<SmtCommand *> m_stackUndo;
		std::stack<SmtCommand *> m_stackRedo;
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_COMMAND_H
