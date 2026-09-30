// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_TYPES_VARIANT_H
#define SMT_LEGACY_CORE_TYPES_VARIANT_H

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "legacy/core/macros/macros.h"
#include "legacy/core/types/scalars.h"

namespace base {

enum SmtVarType {
  SmtInteger = 0,
  SmtIntegerList = 1,
  SmtBool = 2,
  SmtReal = 3,
  SmtRealList = 4,
  SmtByte = 5,
  SmtString = 6,
  SmtStringList = 7,
  SmtBinary = 10,
  SmtDate = 11,
  SmtTime = 12,
  SmtDateTime = 13,
  SmtUnknown = 14
};

// Calendar / clock payload shared by SmtDate / SmtTime / SmtDateTime.
struct SmtDateVal {
  ::ushort Year = 0;
  ::byte Month = 0;
  ::byte Day = 0;
  ::byte Hour = 0;
  ::byte Minute = 0;
  ::byte Second = 0;
  // 0=unknown, 1=localtime(ambiguous), 100=GMT, 104=GMT+1, 80=GMT-5, etc.
  ::byte TZFlag = 0;
};

// Tagged value: std::variant storage + SmtVarType discriminator (Date/Time/
// DateTime share SmtDateVal; Vt selects which calendar view applies).
struct SmtVariant {
  using IntegerList = std::vector<int>;
  using RealList = std::vector<double>;
  using StringList = std::vector<std::string>;
  using Binary = std::vector<::byte>;

  using Storage =
      std::variant<std::monostate, int, IntegerList, bool, double, RealList,
                   ::byte, std::string, StringList, Binary, SmtDateVal>;

  SmtVarType Vt = SmtUnknown;
  ushort usReserved1 = 0;
  ushort usReserved2 = 0;
  ushort usReserved3 = 0;

  Storage data;

  void clear() {
    Vt = SmtUnknown;
    data = std::monostate{};
  }

  void set_integer(int v) {
    Vt = SmtInteger;
    data = v;
  }
  void set_integer_list(IntegerList v) {
    Vt = SmtIntegerList;
    data = std::move(v);
  }
  void set_bool(bool v) {
    Vt = SmtBool;
    data = v;
  }
  void set_real(double v) {
    Vt = SmtReal;
    data = v;
  }
  void set_real_list(RealList v) {
    Vt = SmtRealList;
    data = std::move(v);
  }
  void set_byte(::byte v) {
    Vt = SmtByte;
    data = v;
  }
  void set_string(std::string v) {
    Vt = SmtString;
    data = std::move(v);
  }
  void set_string_list(StringList v) {
    Vt = SmtStringList;
    data = std::move(v);
  }
  void set_binary(Binary v) {
    Vt = SmtBinary;
    data = std::move(v);
  }
  void set_date(SmtDateVal v) {
    Vt = SmtDate;
    data = std::move(v);
  }
  void set_time(SmtDateVal v) {
    Vt = SmtTime;
    data = std::move(v);
  }
  void set_date_time(SmtDateVal v) {
    Vt = SmtDateTime;
    data = std::move(v);
  }

  int* try_integer() { return std::get_if<int>(&data); }
  const int* try_integer() const { return std::get_if<int>(&data); }

  IntegerList* try_integer_list() { return std::get_if<IntegerList>(&data); }
  const IntegerList* try_integer_list() const {
    return std::get_if<IntegerList>(&data);
  }

  bool* try_bool() { return std::get_if<bool>(&data); }
  const bool* try_bool() const { return std::get_if<bool>(&data); }

  double* try_real() { return std::get_if<double>(&data); }
  const double* try_real() const { return std::get_if<double>(&data); }

  RealList* try_real_list() { return std::get_if<RealList>(&data); }
  const RealList* try_real_list() const {
    return std::get_if<RealList>(&data);
  }

  ::byte* try_byte() { return std::get_if<::byte>(&data); }
  const ::byte* try_byte() const { return std::get_if<::byte>(&data); }

  std::string* try_string() { return std::get_if<std::string>(&data); }
  const std::string* try_string() const {
    return std::get_if<std::string>(&data);
  }

  StringList* try_string_list() { return std::get_if<StringList>(&data); }
  const StringList* try_string_list() const {
    return std::get_if<StringList>(&data);
  }

  Binary* try_binary() { return std::get_if<Binary>(&data); }
  const Binary* try_binary() const { return std::get_if<Binary>(&data); }

  SmtDateVal* try_date_val() { return std::get_if<SmtDateVal>(&data); }
  const SmtDateVal* try_date_val() const {
    return std::get_if<SmtDateVal>(&data);
  }

  int i_val() const {
    const int* p = try_integer();
    return p ? *p : 0;
  }
  double dbf_val() const {
    const double* p = try_real();
    return p ? *p : 0.0;
  }
  bool bool_val() const {
    const bool* p = try_bool();
    return p ? *p : false;
  }
  ::byte byte_val() const {
    const ::byte* p = try_byte();
    return p ? *p : static_cast<::byte>(0);
  }
  const char* bstr_val() const {
    const std::string* p = try_string();
    return p ? p->c_str() : nullptr;
  }
};

template <typename V>
struct variant_traits {
  using storage_type = typename V::Storage;
  using integer_list = typename V::IntegerList;
  using real_list = typename V::RealList;
  using string_list = typename V::StringList;
  using binary = typename V::Binary;
  using date_val = SmtDateVal;

  static SmtVarType type(const V& v) { return v.Vt; }
  static const storage_type& storage(const V& v) { return v.data; }
  static storage_type& storage(V& v) { return v.data; }
};

}  // namespace base

#endif  // SMT_LEGACY_CORE_TYPES_VARIANT_H
