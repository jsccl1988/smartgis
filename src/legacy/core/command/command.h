// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _SMT_COMMAND_H
#define _SMT_COMMAND_H

#include <algorithm>
#include <stack>
#include <vector>

#include "legacy/core/macros/macros.h"

namespace base {

class SmtCommandReceiver {
 public:
  SmtCommandReceiver() = default;
  virtual ~SmtCommandReceiver() = default;
  virtual bool action(bool bUndo) = 0;
};

class SmtCommand {
 public:
  SmtCommand(SmtCommandReceiver* pReceiver, bool bAutoDelete = true)
      : m_pReceiver(pReceiver), m_bAutoDeleteReceiver(bAutoDelete) {}
  virtual ~SmtCommand(void) {
    if (m_bAutoDeleteReceiver) {
      SMT_SAFE_DELETE(m_pReceiver);
    }
  }

  void set_receiver(SmtCommandReceiver* pReceiver, bool bAutoDelete = true) {
    m_pReceiver = pReceiver;
    m_bAutoDeleteReceiver = bAutoDelete;
  }

  virtual bool execute(void) {
    return m_pReceiver ? m_pReceiver->action(false) : false;
  }
  virtual bool unexecute() {
    return m_pReceiver ? m_pReceiver->action(true) : false;
  }

 protected:
  SmtCommand(const SmtCommand&) = delete;
  SmtCommand& operator=(const SmtCommand&) = delete;

  SmtCommandReceiver* m_pReceiver;
  bool m_bAutoDeleteReceiver;
};

class SmtMacroCommand : public SmtCommand {
 public:
  SmtMacroCommand() : SmtCommand(nullptr, false) {}
  ~SmtMacroCommand() override {
    for (SmtCommand* cmd : m_vecCommands) {
      SMT_SAFE_DELETE(cmd);
    }
    m_vecCommands.clear();
  }

  bool execute() override {
    for (SmtCommand* cmd : m_vecCommands) {
      if (!cmd->execute()) {
        return false;
      }
    }
    return true;
  }

  bool unexecute() override {
    for (SmtCommand* cmd : m_vecCommands) {
      if (!cmd->unexecute()) {
        return false;
      }
    }
    return true;
  }

  void add_command(SmtCommand* pCommand) {
    if (pCommand) {
      m_vecCommands.push_back(pCommand);
    }
  }

  void delete_command(SmtCommand* pCommand) {
    if (!pCommand) {
      return;
    }
    m_vecCommands.erase(
        std::remove(m_vecCommands.begin(), m_vecCommands.end(), pCommand),
        m_vecCommands.end());
  }

 private:
  SmtMacroCommand(const SmtMacroCommand&) = delete;
  SmtMacroCommand& operator=(const SmtMacroCommand&) = delete;
  std::vector<SmtCommand*> m_vecCommands;
};

class SmtCommandManager {
 public:
  SmtCommandManager(void) = default;
  virtual ~SmtCommandManager() {
    clear_all_commands();
  }

  virtual bool call_command(SmtCommand* pCommand) {
    if (!pCommand) {
      return false;
    }
    if (pCommand->execute()) {
      push_undo_command(pCommand);
      delete_redo_commands();
      return true;
    }
    delete pCommand;
    return false;
  }

  virtual void clear_all_commands() {
    delete_undo_commands();
    delete_redo_commands();
  }

  virtual void undo() {
    SmtCommand* pCommand = pop_undo_command();
    if (!pCommand) {
      return;
    }
    if (pCommand->unexecute()) {
      push_redo_command(pCommand);
    } else {
      delete pCommand;
    }
  }

  virtual void redo() {
    SmtCommand* pCommand = pop_redo_command();
    if (!pCommand) {
      return;
    }
    if (pCommand->execute()) {
      push_undo_command(pCommand);
    } else {
      delete pCommand;
    }
  }

  virtual bool can_undo() const { return !m_stackUndo.empty(); }
  virtual bool can_redo() const { return !m_stackRedo.empty(); }

  void push_undo_command(SmtCommand* pCommand) {
    if (pCommand) {
      m_stackUndo.push(pCommand);
    }
  }
  SmtCommand* pop_undo_command() {
    if (m_stackUndo.empty()) {
      return nullptr;
    }
    SmtCommand* pCommand = m_stackUndo.top();
    m_stackUndo.pop();
    return pCommand;
  }
  void push_redo_command(SmtCommand* pCommand) {
    if (pCommand) {
      m_stackRedo.push(pCommand);
    }
  }
  SmtCommand* pop_redo_command() {
    if (m_stackRedo.empty()) {
      return nullptr;
    }
    SmtCommand* pCommand = m_stackRedo.top();
    m_stackRedo.pop();
    return pCommand;
  }
  void delete_undo_commands() {
    while (!m_stackUndo.empty()) {
      delete m_stackUndo.top();
      m_stackUndo.pop();
    }
  }
  void delete_redo_commands() {
    while (!m_stackRedo.empty()) {
      delete m_stackRedo.top();
      m_stackRedo.pop();
    }
  }

 private:
  std::stack<SmtCommand*> m_stackUndo;
  std::stack<SmtCommand*> m_stackRedo;
};

}  // namespace base

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_SMT_COMMAND_H
