#include "base/carto/stylemanager.h"

namespace base {
SmtStyleManager *SmtStyleManager::m_pSingleton = NULL;

SmtStyleManager *SmtStyleManager::get_singleton_ptr(void) {
  if (m_pSingleton == NULL) {
    m_pSingleton = new SmtStyleManager();
  }
  return m_pSingleton;
}

void SmtStyleManager::destroy_instance(void) { SMT_SAFE_DELETE(m_pSingleton); }

//////////////////////////////////////////////////////////////////////////
SmtStyleManager::SmtStyleManager(void) {
  m_pDefaultStyle = NULL;

  destroy_all_style();
}

SmtStyleManager::~SmtStyleManager(void) { destroy_all_style(); }

//////////////////////////////////////////////////////////////////////////

void SmtStyleManager::set_default_style(const char *defName,
                                        SmtPenDesc &penDesc,
                                        SmtBrushDesc &brushDesc,
                                        SmtAnnotationDesc &annoDesc,
                                        SmtSymbolDesc &symbolDesc) {
  m_pDefaultStyle =
      new SmtStyle(defName, penDesc, brushDesc, annoDesc, symbolDesc);
  m_StylePtrList.push_back(m_pDefaultStyle);
}

void SmtStyleManager::destroy_all_style() {
  StylePtrList::iterator i = m_StylePtrList.begin();

  while (i != m_StylePtrList.end()) {
    SMT_SAFE_DELETE(*i);
    ++i;
  }
  m_StylePtrList.clear();
}

SmtStyle *SmtStyleManager::create_style(const char *defName,
                                        SmtPenDesc &penDesc,
                                        SmtBrushDesc &brushDesc,
                                        SmtAnnotationDesc &annoDesc,
                                        SmtSymbolDesc &symbolDesc) {
  SmtStyle *pStyle = NULL;
  pStyle = get_style(defName);
  if (!pStyle) {
    pStyle = new SmtStyle(defName, penDesc, brushDesc, annoDesc, symbolDesc);
    m_StylePtrList.push_back(pStyle);
  }

  return pStyle;
}

void SmtStyleManager::destroy_style(const char *name) {
  SmtStyle *pStyle = NULL;
  StylePtrList::iterator i = m_StylePtrList.begin();

  while (i != m_StylePtrList.end()) {
    if (strcmp((**i).get_style_name(), name) == 0) {
      SMT_SAFE_DELETE(*i);
      m_StylePtrList.erase(i);
      break;
    }
    ++i;
  }
}

void SmtStyleManager::destroy_style(SmtStyle *pStyle) {
  destroy_style(pStyle->get_style_name());
}

SmtStyle *SmtStyleManager::get_style(const char *name) {
  SmtStyle *pStyle = NULL;
  StylePtrList::iterator i = m_StylePtrList.begin();

  while (i != m_StylePtrList.end()) {
    if (strcmp((**i).get_style_name(), name) == 0) {
      pStyle = *i;
      break;
    }
    ++i;
  }
  return pStyle;
}

SmtStyle *SmtStyleManager::get_default_style(void) {
  if (m_pDefaultStyle)
    return m_pDefaultStyle;
  else
    return NULL;
}
}  // namespace base