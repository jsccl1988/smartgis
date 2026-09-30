// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_MSG_DEF_H
#define _SMT_MSG_DEF_H

#define SMT_FUNC_NAME_LENGTH 50
#define SMT_GROUP_NAME_LENGTH 50

#include <map>

#include "legacy/core/macros/macros.h"
// #include <hash_map>
namespace base {
struct SmtFuncItem {
  char szName[SMT_FUNC_NAME_LENGTH];
  long lMsg;
};

enum SmtFuncItemStyle {
  FIM_2DVIEW = 1 << 0,
  FIM_3DVIEW = 1 << 1,
  FIM_3DEXVIEW = 1 << 2,
  FIM_MAPDOCCATALOG = 1 << 3,
  FIM_2DMFTOOLBAR = 1 << 4,
  FIM_3DMFTOOLBAR = 1 << 5,
  FIM_2DMFMENU = 1 << 6,
  FIM_3DMFMENU = 1 << 7,
  FIM_AUXMODULEBOX = 1 << 8,
  FIM_AUXMODULETREE = 1 << 9,
};

struct SmtListenerMsg {
  HWND hSrcWnd;
  WPARAM wParam;
  LPARAM lParam;
  void *pToFollow;
  bool bModify;

  SmtListenerMsg() {
    hSrcWnd = NULL;
    wParam = NULL;
    lParam = NULL;
    pToFollow = NULL;
    bModify = false;
  }

  operator LPARAM() { return LPARAM(this); }

  operator WPARAM() { return WPARAM(this); }
};

#define SMT_MSG_KEY(lMsg, hWnd) (MAKELONG(lMsg, hWnd))
#define SMT_MSG_KEY_HWND(msgKey) (HIWORD(msgKey))
#define SMT_MSG_KEY_LMSG(msgKey) (LOWORD(msgKey))

typedef vector<SmtFuncItem> vSmtFuncItems;
typedef vSmtFuncItems vSmt2DViewFuncItems;
typedef vSmtFuncItems vSmt3DViewFuncItems;
typedef vSmtFuncItems vSmt3DExViewFuncItems;
typedef vSmtFuncItems vSmtMapDocCatalogFuncItems;
typedef vSmtFuncItems vSmt2DToolBarFuncItems;
typedef vSmtFuncItems vSmt3DToolBarFuncItems;
typedef vSmtFuncItems vSmt2DMenuFuncItems;
typedef vSmtFuncItems vSmt3DMenuFuncItems;
typedef vSmtFuncItems vSmtAuxModuleBoxFuncItems;
typedef vSmtFuncItems vSmtAuxModuleTreeFuncItems;

typedef vector<long> vSmtMsgs;
typedef map<long, void *> mapMsgToPtr;
typedef pair<long, void *> pairMsgToPtr;
// typedef hash_map<long,void*>				hashmapMsgToPtr;

class SmtListener;

}  // namespace base

#define SMT_LISTENER_MSG_BROADCAST ((base::SmtListener *)0xFFFF)
#define SMT_LISTENER_MSG_INVALID ((base::SmtListener *)0x0000)

// Message id ranges (leaf constants; keep out of listener_manager).
#define SMT_MSG_INVALID (-1)
#define SMT_MSG_SYS_MSG (0x0000)
#define SMT_MSG_2D_TOOL (0x1000)
#define SMT_MSG_3D_TOOL (0x2000)
#define SMT_MSG_CMD_BEGIN (0x3001)
#define SMT_MSG_CMD_END (0x4000)
#define SMT_MSG_USER_BEGIN (0x4001)
#define SMT_MSG_USER_END (0x8000)

#define SMT_MSG_GET_SYS_2DVIEW (SMT_MSG_SYS_MSG + 1)
#define SMT_MSG_GET_SYS_2DEDITVIEW (SMT_MSG_SYS_MSG + 2)
#define SMT_MSG_GET_SYS_3DVIEW (SMT_MSG_SYS_MSG + 3)

#endif  //_SMT_MSG_DEF_H