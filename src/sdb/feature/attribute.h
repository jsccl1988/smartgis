/*
File:    gis_attribute.h

Desc:    Leftover attribute schema types (not product field storage).

Version: Version 1.0

Writter:  谢达斌

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_ATTRIBUTE_H
#define _GIS_ATTRIBUTE_H

// SmtAttribute / SmtField left //src/sdb/map:gis. Product fields live on
// OGRFeature / OGRFeatureDefn (sdb::Feature, MapLayer::ogr()).
// MFC leftover only: link //src/sdb/feature:leftover_attr and define
// SMT_ALLOW_LEFTOVER_ATTRIBUTE before including this header.
#if !defined(SMT_ALLOW_LEFTOVER_ATTRIBUTE)
#error \
    "SmtAttribute/SmtField left //src/sdb/map:gis. Use OGR field APIs. " \
    "Leftover: define SMT_ALLOW_LEFTOVER_ATTRIBUTE and link " \
    "//src/sdb/feature:leftover_attr."
#endif

#include "base/core/core.h"
#include "base/core/bas_struct.h"
#include "sdb/feature/leftover_attr_export.h"

using namespace base;

#define SMT_FIELD_NAME_LENGTH 255
namespace sdb {
class LEFTOVER_ATTR_EXPORT SmtField {
  friend class SmtAttribute;

 public:
  SmtField();
  virtual ~SmtField();

 public:
  const char* GetName(void) const { return m_fldName; }
  varType GetType(void) const { return m_fldValue.Vt; }
  SmtVariant* GetValuePtr(void) { return &m_fldValue; }
  const SmtVariant* GetValuePtr(void) const { return &m_fldValue; }

  int GetValueAsInteger() const;
  double GetValueAsDouble() const;
  const char* GetValueAsString() const;
  const int* GetValueAsIntegerList(int* pnCount) const;
  const double* GetValueAsDoubleList(int* pnCount) const;
  char** GetValueAsStringList() const;
  byte* GetValueAsBinary(int* pnCount) const;
  int GetValueAsDateTime(int* pnYear,
                         int* pnMonth,
                         int* pnDay,
                         int* pnHour,
                         int* pnMinute,
                         int* pnSecond,
                         int* pnTZFlag) const;

  SmtField* clone() const;

  bool IsFieldSetted() const {
    return (m_fldValue.usReserved1 == SmtSetMarker);
  }

  void UnSetField();

  void SetName(const char* szName);
  void SetType(varType uVt);

  int SetValue(const SmtVariant& smtFld);
  int SetValue(int nValue);
  int SetValue(double dfValue);
  int SetValue(byte bValue);
  int SetValue(bool bValue);
  int SetValue(const char* pszValue);
  int SetValue(int nCount, int* panValues);
  int SetValue(int nCount, double* padfValues);
  int SetValue(char** papszValues);
  int SetValue(int nCount, byte* pabyBinary);
  int SetValue(int nYear,
               int nMonth,
               int nDay,
               int nHour = 0,
               int nMinute = 0,
               int nSecond = 0,
               int nTZFlag = 0);

 public:
  static void GetAllTypes(vector<int>& vFldTypes);
  static void GetAllTypeNames(vector<string>& vFldTypeNames);

  static const char* GetTypeName(ushort type);
  static ushort GetType(const char* szTypeName);

 protected:
  char m_fldName[SMT_FIELD_NAME_LENGTH];
  SmtVariant m_fldValue;
};

class LEFTOVER_ATTR_EXPORT SmtAttribute {
 public:
  SmtAttribute(void);
  virtual ~SmtAttribute(void);

  SmtAttribute* clone(void) const;

  void AddField(SmtField& fld);
  void RemoveField(const char* fldname);

  inline long GetFieldCount(void) const { return m_lFieldCount; }

  inline SmtField** GetFieldPtrsRef(void) {
    return (m_pFieldPtrs != NULL) ? m_pFieldPtrs : NULL;
  }
  inline SmtField** const GetFieldPtrsRef(void) const {
    return (m_pFieldPtrs != NULL) ? m_pFieldPtrs : NULL;
  }

  inline SmtField* GetFieldPtr(int index) {
    if (index > -1 && index < m_lFieldCount)
      return m_pFieldPtrs[index];
    else
      return NULL;
  }
  inline const SmtField* GetFieldPtr(int index) const {
    if (index > -1 && index < m_lFieldCount)
      return m_pFieldPtrs[index];
    else
      return NULL;
  }
  int GetFieldIndex(const char* fldname) const;

  bool IsFieldExsit(const char* fldname) const;

 protected:
  long m_lFieldCount;
  SmtField** m_pFieldPtrs;
};
}  // namespace sdb

#endif  // _GIS_ATTRIBUTE_H
