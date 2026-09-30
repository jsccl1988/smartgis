// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_PRESENT_CARTO_STYLEMANAGER_H_
#define GIS_PRESENT_CARTO_STYLEMANAGER_H_

#include "gis/gis_export.h"
#include "legacy/gis/present/carto/style.h"

namespace base {
// Singleton registry of named SmtStyle instances for leftover paint paths.
class GIS_EXPORT SmtStyleManager {
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

#endif  // GIS_PRESENT_CARTO_STYLEMANAGER_H_
