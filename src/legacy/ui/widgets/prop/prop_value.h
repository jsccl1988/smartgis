// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_WIDGETS_PROP_VALUE_H_
#define LEGACY_UI_WIDGETS_PROP_VALUE_H_

// Feature Pack GetValue() is const COleVariant&; historical BCG allowed
// numeric C-casts. Coerce through a mutable temporary for leftover docks.

namespace legacy_ui {

inline long prop_as_long(const COleVariant& v) {
  COleVariant tmp(v);
  tmp.ChangeType(VT_I4);
  return tmp.lVal;
}

inline float prop_as_float(const COleVariant& v) {
  COleVariant tmp(v);
  tmp.ChangeType(VT_R4);
  return tmp.fltVal;
}

}  // namespace legacy_ui

#endif  // LEGACY_UI_WIDGETS_PROP_VALUE_H_
