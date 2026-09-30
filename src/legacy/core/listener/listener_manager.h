// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef _SMT_LISTENER_MGR_H
#define _SMT_LISTENER_MGR_H

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>

#include "legacy/core/macros/macros.h"
#include "legacy/core/msg/msg_def.h"
#include "legacy/core/types/types.h"

namespace base {

// Leftover listener base. Header-only: do not mark BASE_EXPORT
// (avoids dllimport of inline members).
class SmtListener {
 public:
  SmtListener(void) { m_szListenerName[0] = '\0'; }
  virtual ~SmtListener(void) = default;

  virtual int register_(void);
  virtual int register_msg(void);
  virtual int unregister(void);
  virtual int unregister_msg(void);
  virtual int set_active();

  virtual int notify(long lMsg, SmtListenerMsg& param) = 0;

  const char* get_name() const { return m_szListenerName; }
  void set_name(const char* szName) {
    if (szName == nullptr) {
      m_szListenerName[0] = '\0';
      return;
    }
    std::strncpy(m_szListenerName, szName, SMT_GROUP_NAME_LENGTH - 1);
    m_szListenerName[SMT_GROUP_NAME_LENGTH - 1] = '\0';
  }

  bool append_func_items(const char* szFunc, long lFuncMsg,
                         long lStyle = FIM_2DVIEW | FIM_3DVIEW) {
    bool ok = true;
    if (lStyle & FIM_2DVIEW) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v2DViewFuncItems);
    }
    if (lStyle & FIM_3DVIEW) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v3DViewFuncItems);
    }
    if (lStyle & FIM_3DEXVIEW) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v3DExViewFuncItems);
    }
    if (lStyle & FIM_MAPDOCCATALOG) {
      ok &= append_func_items(szFunc, lFuncMsg, m_vMDCatalogFuncItems);
    }
    if (lStyle & FIM_2DMFTOOLBAR) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v2DToolBarFuncItems);
    }
    if (lStyle & FIM_3DMFTOOLBAR) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v3DToolBarFuncItems);
    }
    if (lStyle & FIM_2DMFMENU) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v2DMMenuFuncItems);
    }
    if (lStyle & FIM_3DMFMENU) {
      ok &= append_func_items(szFunc, lFuncMsg, m_v3DMMenuFuncItems);
    }
    if (lStyle & FIM_AUXMODULEBOX) {
      ok &= append_func_items(szFunc, lFuncMsg, m_vAMBoxFuncItems);
    }
    if (lStyle & FIM_AUXMODULETREE) {
      ok &= append_func_items(szFunc, lFuncMsg, m_vAMTreeFuncItems);
    }
    return ok;
  }

  vSmtFuncItems get_func_items(SmtFuncItemStyle style) {
    switch (style) {
      case FIM_2DVIEW:
        return m_v2DViewFuncItems;
      case FIM_3DVIEW:
        return m_v3DViewFuncItems;
      case FIM_3DEXVIEW:
        return m_v3DExViewFuncItems;
      case FIM_MAPDOCCATALOG:
        return m_vMDCatalogFuncItems;
      case FIM_2DMFTOOLBAR:
        return m_v2DToolBarFuncItems;
      case FIM_3DMFTOOLBAR:
        return m_v3DToolBarFuncItems;
      case FIM_2DMFMENU:
        return m_v2DMMenuFuncItems;
      case FIM_3DMFMENU:
        return m_v3DMMenuFuncItems;
      case FIM_AUXMODULEBOX:
        return m_vAMBoxFuncItems;
      case FIM_AUXMODULETREE:
        return m_vAMTreeFuncItems;
      default:
        return {};
    }
  }

  vSmtMsgs get_msgs(void) { return m_vMsgs; }

 protected:
  bool append_func_items(const char* szFunc, long lFuncMsg,
                         vSmtFuncItems& vFuncItems) {
    for (const auto& item : vFuncItems) {
      if (item.lMsg == lFuncMsg) {
        return false;
      }
    }
    SmtFuncItem funcItem = {};
    if (szFunc) {
      std::strncpy(funcItem.szName, szFunc, SMT_FUNC_NAME_LENGTH - 1);
    }
    funcItem.lMsg = lFuncMsg;
    vFuncItems.push_back(funcItem);
    return true;
  }

  bool append_msg(long lFuncMsg) {
    if (std::find(m_vMsgs.begin(), m_vMsgs.end(), lFuncMsg) != m_vMsgs.end()) {
      return false;
    }
    m_vMsgs.push_back(lFuncMsg);
    return true;
  }

  char m_szListenerName[SMT_GROUP_NAME_LENGTH];
  vSmt2DViewFuncItems m_v2DViewFuncItems;
  vSmt3DViewFuncItems m_v3DViewFuncItems;
  vSmt3DExViewFuncItems m_v3DExViewFuncItems;
  vSmtMapDocCatalogFuncItems m_vMDCatalogFuncItems;
  vSmt2DToolBarFuncItems m_v2DToolBarFuncItems;
  vSmt3DToolBarFuncItems m_v3DToolBarFuncItems;
  vSmt2DMenuFuncItems m_v2DMMenuFuncItems;
  vSmt3DMenuFuncItems m_v3DMMenuFuncItems;
  vSmtAuxModuleBoxFuncItems m_vAMBoxFuncItems;
  vSmtAuxModuleTreeFuncItems m_vAMTreeFuncItems;
  vSmtMsgs m_vMsgs;
};

typedef vector<SmtListener*> vSmtListenerPtrs;

typedef map<string, SmtListener> SmtListenerMap;

// Singleton registry for leftover SmtListener instances and msg routing.
class SmtListenerManager {
 public:
  ~SmtListenerManager(void) { remove_all_listener(); }

  static SmtListenerManager*& holder() {
    static SmtListenerManager* p = nullptr;
    return p;
  }

  static SmtListenerManager* get_singleton_ptr(void) {
    static std::mutex lock;
    std::lock_guard<std::mutex> scope(lock);
    if (holder() == nullptr) {
      holder() = new SmtListenerManager();
    }
    return holder();
  }

  static void destroy_instance(void) {
    static std::mutex lock;
    std::lock_guard<std::mutex> scope(lock);
    delete holder();
    holder() = nullptr;
  }

  long notify(SmtListener* pListener, long lMsg, SmtListenerMsg& param) {
    if (pListener == SMT_LISTENER_MSG_INVALID) {
      return SMT_ERR_INVALID_PARAM;
    }
    if (pListener == SMT_LISTENER_MSG_BROADCAST) {
      auto mapIter = m_mapMsgToListeners.find(lMsg);
      if (mapIter != m_mapMsgToListeners.end()) {
        auto* found = static_cast<SmtListener*>(mapIter->second);
        if (found) {
          found->notify(lMsg, param);
        }
      } else {
        vSmtListenerPtrs done;
        auto it = m_vListenerPtrs.begin();
        while (it != m_vListenerPtrs.end()) {
          if (std::find(done.begin(), done.end(), *it) == done.end()) {
            param.bModify = false;
            done.push_back(*it);
            (*it++)->notify(lMsg, param);
            if (param.bModify) {
              it = m_vListenerPtrs.begin();
              continue;
            }
          } else {
            ++it;
          }
        }
      }
    } else {
      pListener->notify(lMsg, param);
    }
    return SMT_ERR_NONE;
  }

  long register_listener(SmtListener* pListener) {
    if (std::find(m_vListenerPtrs.begin(), m_vListenerPtrs.end(), pListener) !=
        m_vListenerPtrs.end()) {
      return false;
    }
    m_vListenerPtrs.push_back(pListener);
    return SMT_ERR_NONE;
  }

  long remove_listener(SmtListener* pListener) {
    auto it =
        std::find(m_vListenerPtrs.begin(), m_vListenerPtrs.end(), pListener);
    if (it != m_vListenerPtrs.end()) {
      m_vListenerPtrs.erase(it);
    }
    return true;
  }

  long remove_all_listener(void) {
    m_vListenerPtrs.clear();
    return SMT_ERR_NONE;
  }

  void set_active_listener(SmtListener* pListener) {
    m_pActiveListener = pListener;
  }
  SmtListener* get_active_listener(void) { return m_pActiveListener; }
  const SmtListener* get_active_listener(void) const {
    return m_pActiveListener;
  }

  int get_listener_count(void) const {
    return static_cast<int>(m_vListenerPtrs.size());
  }
  SmtListener* get_listener(int index) {
    if (index < 0 || index >= static_cast<int>(m_vListenerPtrs.size())) {
      return nullptr;
    }
    return m_vListenerPtrs.at(static_cast<size_t>(index));
  }
  const SmtListener* get_listener(int index) const {
    if (index < 0 || index >= static_cast<int>(m_vListenerPtrs.size())) {
      return nullptr;
    }
    return m_vListenerPtrs.at(static_cast<size_t>(index));
  }

  long register_listener_msg(SmtListener* pListener) {
    if (pListener == nullptr) {
      return SMT_ERR_INVALID_PARAM;
    }
    for (long msg : pListener->get_msgs()) {
      if (m_mapMsgToListeners.find(msg) == m_mapMsgToListeners.end()) {
        m_mapMsgToListeners.insert(pairMsgToPtr(msg, pListener));
      }
    }
    return SMT_ERR_NONE;
  }

  long unregister_listener_msg(SmtListener* pListener) {
    if (pListener == nullptr) {
      return SMT_ERR_INVALID_PARAM;
    }
    for (long msg : pListener->get_msgs()) {
      auto it = m_mapMsgToListeners.find(msg);
      if (it != m_mapMsgToListeners.end()) {
        m_mapMsgToListeners.erase(it);
      }
    }
    return SMT_ERR_NONE;
  }

 private:
  SmtListenerManager(void) : m_pActiveListener(nullptr) {}

  vSmtListenerPtrs m_vListenerPtrs;
  SmtListener* m_pActiveListener;
  mapMsgToPtr m_mapMsgToListeners;
};

inline int SmtListener::register_(void) {
  return SmtListenerManager::get_singleton_ptr()->register_listener(this);
}

inline int SmtListener::register_msg(void) {
  SmtListenerManager::get_singleton_ptr()->register_listener_msg(this);
  return SMT_ERR_NONE;
}

inline int SmtListener::unregister(void) {
  return SmtListenerManager::get_singleton_ptr()->remove_listener(this);
}

inline int SmtListener::unregister_msg(void) {
  SmtListenerManager::get_singleton_ptr()->unregister_listener_msg(this);
  return SMT_ERR_NONE;
}

inline int SmtListener::set_active() {
  SmtListenerManager::get_singleton_ptr()->set_active_listener(this);
  return SMT_ERR_NONE;
}

}  // namespace base

#define SMT_POST_LISTENER_MSG(pListener, lMsg, param)  \
  {                                                    \
    base::SmtListenerManager *pListenerMgr =           \
        base::SmtListenerManager::get_singleton_ptr(); \
    pListenerMgr->notify(pListener, lMsg, param);      \
  }

inline long smt_post_listener_msg(base::SmtListener* pListener, long lMsg,
                                  base::SmtListenerMsg& param) {
  return base::SmtListenerManager::get_singleton_ptr()->notify(pListener, lMsg,
                                                               param);
}

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_SMT_LISTENER_MGR_H
