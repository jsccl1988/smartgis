// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_DIALOGS_DETAIL_OGR_FIELD_TYPE_H_
#define LEGACY_UI_DIALOGS_DETAIL_OGR_FIELD_TYPE_H_

#include <cstring>

#include "ogrsf_frmts.h"

namespace ui {
namespace detail {

// Display labels for the leftover att-struct editor (CMFCListCtrl type column).
inline constexpr const char* kOgrTypeLabels[] = {
    "Integer", "Integer64", "Real", "String", "Date", "Binary"};

inline constexpr int kOgrTypeLabelCount =
    static_cast<int>(sizeof(kOgrTypeLabels) / sizeof(kOgrTypeLabels[0]));

inline const char* ogr_field_type_label(OGRFieldType type) {
  switch (type) {
    case OFTInteger:
      return "Integer";
    case OFTInteger64:
      return "Integer64";
    case OFTReal:
      return "Real";
    case OFTString:
      return "String";
    case OFTDate:
    case OFTDateTime:
      return "Date";
    case OFTBinary:
      return "Binary";
    default:
      return "String";
  }
}

inline OGRFieldType ogr_field_type_from_label(const char* label) {
  if (!label) {
    return OFTString;
  }
  if (_stricmp(label, "Integer") == 0) {
    return OFTInteger;
  }
  if (_stricmp(label, "Integer64") == 0) {
    return OFTInteger64;
  }
  if (_stricmp(label, "Real") == 0 || _stricmp(label, "Double") == 0) {
    return OFTReal;
  }
  if (_stricmp(label, "Date") == 0 || _stricmp(label, "DateTime") == 0) {
    return OFTDate;
  }
  if (_stricmp(label, "Binary") == 0) {
    return OFTBinary;
  }
  return OFTString;
}

inline OGRFieldType ogr_field_type_cycle(OGRFieldType type) {
  const char* cur = ogr_field_type_label(type);
  int next = 0;
  for (int i = 0; i < kOgrTypeLabelCount; ++i) {
    if (_stricmp(cur, kOgrTypeLabels[i]) == 0) {
      next = (i + 1) % kOgrTypeLabelCount;
      break;
    }
  }
  return ogr_field_type_from_label(kOgrTypeLabels[next]);
}

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_DIALOGS_DETAIL_OGR_FIELD_TYPE_H_
