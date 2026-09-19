/*
File:    bl_stylemanager.h

Desc:    SmtStyleManager

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_STYLEMANAGER_H
#define _BL_STYLEMANAGER_H

#include "base/carto/style.h"

namespace base {
class STYLE_EXPORT SmtStyleManager {
 private:
  SmtStyleManager(void);

 public:
  virtual ~SmtStyleManager(void);

  void set_default_style(const char *defName, SmtPenDesc &penDesc,
                         SmtBrushDesc &brushDesc, SmtAnnotationDesc &annoDesc,
                         SmtSymbolDesc &symbolDesc);
  SmtStyle *get_default_style(void);

 public:
  SmtStyle *create_style(const char *defName, SmtPenDesc &penDesc,
                         SmtBrushDesc &brushDesc, SmtAnnotationDesc &annoDesc,
                         SmtSymbolDesc &symbolDesc);

  void destroy_style(const char *name);

  void destroy_style(SmtStyle *pStyle);

  void destroy_all_style();

  SmtStyle *get_style(const char *name);

 public:
  static SmtStyleManager *get_singleton_ptr(void);

  static void destroy_instance(void);

 private:
  StylePtrList m_StylePtrList;
  SmtStyle *m_pDefaultStyle;

  static SmtStyleManager *m_pSingleton;
};
}  // namespace base

#if !defined(STYLE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "platform_d.lib")
#else
#pragma comment(lib, "platform.lib")
#endif
#endif

#endif  //_BL_STYLEMANAGER_H