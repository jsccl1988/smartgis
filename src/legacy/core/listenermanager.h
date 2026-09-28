// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_LISTENER_MGR_H
#define _SMT_LISTENER_MGR_H

#include <map>
#include <mutex>

#include "base/core/export.h"
#include "legacy/core/core.h"
#include "legacy/core/listener.h"

namespace base {
typedef map<string, SmtListener> SmtListenerMap;

class BASE_EXPORT SmtListenerManager {
 public:
  virtual ~SmtListenerManager(void);

 public:
  static SmtListenerManager* get_singleton_ptr(void);
  static void destroy_instance(void);

 public:
  long notify(SmtListener* pListener, long lMsg, SmtListenerMsg& param);

  long register_listener(SmtListener* pListener);
  long remove_listener(SmtListener* pListener);
  long remove_all_listener(void);

  void set_active_listener(SmtListener* pListener) {
    m_pActiveListener = pListener;
  }
  SmtListener* get_active_listener(void) { return m_pActiveListener; }
  const SmtListener* get_active_listener(void) const {
    return m_pActiveListener;
  }

  int get_listener_count(void) const { return m_vListenerPtrs.size(); }
  SmtListener* get_listener(int index);
  const SmtListener* get_listener(int index) const;

  long register_listener_msg(SmtListener* pListener);
  long unregister_listener_msg(SmtListener* pListener);

 protected:
#ifdef SMT_THREAD_SAFE
  std::mutex m_cslock;
#endif
  vSmtListenerPtrs m_vListenerPtrs;
  SmtListener* m_pActiveListener;
  mapMsgToPtr m_mapMsgToListeners;

 private:
  SmtListenerManager(void);
  static SmtListenerManager* m_pSingleton;
};
}  // namespace base

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_SMT_LISTENER_MGR_H