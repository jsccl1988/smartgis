// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/feature/feature.h"

#include <utility>

#include "ogrsf_frmts.h"

namespace gis {
namespace {

constexpr int k_ok = 0;
constexpr int k_fail = 1;

bool valid_field(const OGRFeature* ogr, int index) {
  return ogr != nullptr && index >= 0;
}

}  // namespace

Feature::Feature() = default;

Feature::Feature(OGRFeature* ogr, bool take_ownership)
    : ogr_(ogr), owns_ogr_(take_ownership && ogr != nullptr) {}

Feature::~Feature() {
  if (owns_ogr_ && ogr_) {
    OGRFeature::DestroyFeature(ogr_);
    ogr_ = nullptr;
  }
}

Feature::Feature(Feature&& other) noexcept { take_from(std::move(other)); }

Feature& Feature::operator=(Feature&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (owns_ogr_ && ogr_) {
    OGRFeature::DestroyFeature(ogr_);
  }
  take_from(std::move(other));
  return *this;
}

Feature Feature::borrow(OGRFeature* ogr) { return Feature(ogr, false); }

OGRFeature* Feature::release() {
  OGRFeature* out = ogr_;
  ogr_ = nullptr;
  owns_ogr_ = false;
  return out;
}

void Feature::reset_ogr(OGRFeature* ogr, bool take_ownership) {
  if (owns_ogr_ && ogr_ && ogr_ != ogr) {
    OGRFeature::DestroyFeature(ogr_);
  }
  ogr_ = ogr;
  owns_ogr_ = take_ownership && ogr != nullptr;
}

void Feature::take_from(Feature&& other) noexcept {
  ogr_ = other.ogr_;
  owns_ogr_ = other.owns_ogr_;
  other.ogr_ = nullptr;
  other.owns_ogr_ = false;
}

OGRwkbGeometryType Feature::geometry_type() const {
  const OGRGeometry* geom = geometry();
  return geom ? geom->getGeometryType() : wkbNone;
}

long Feature::id() const {
  return ogr_ ? static_cast<long>(ogr_->GetFID()) : 0;
}

void Feature::set_id(long id) {
  if (ogr_) {
    ogr_->SetFID(id);
  }
}

OGRGeometry* Feature::geometry() {
  return ogr_ ? ogr_->GetGeometryRef() : nullptr;
}

const OGRGeometry* Feature::geometry() const {
  return ogr_ ? ogr_->GetGeometryRef() : nullptr;
}

void Feature::set_geometry(OGRGeometry* geom) {
  if (ogr_) {
    ogr_->SetGeometry(geom);
  }
}

void Feature::set_geometry_directly(OGRGeometry* geom) {
  if (ogr_) {
    ogr_->SetGeometryDirectly(geom);
  }
}

int Feature::field_index(const char* name) const {
  if (!ogr_ || !name) {
    return -1;
  }
  return ogr_->GetFieldIndex(name);
}

int Feature::field_count() const {
  if (!ogr_ || !ogr_->GetDefnRef()) {
    return 0;
  }
  return ogr_->GetDefnRef()->GetFieldCount();
}

OGRFieldType Feature::field_type(int index) const {
  if (!valid_field(ogr_, index) || !ogr_->GetDefnRef()) {
    return OFTInteger;
  }
  OGRFieldDefn* defn = ogr_->GetDefnRef()->GetFieldDefn(index);
  return defn ? defn->GetType() : OFTInteger;
}

bool Feature::is_field_set(int index) const {
  return valid_field(ogr_, index) && ogr_->IsFieldSet(index);
}

bool Feature::is_field_null(int index) const {
  return valid_field(ogr_, index) && ogr_->IsFieldNull(index);
}

int Feature::get_field_as_integer(int index) const {
  return valid_field(ogr_, index) ? ogr_->GetFieldAsInteger(index) : 0;
}

GIntBig Feature::get_field_as_integer64(int index) const {
  return valid_field(ogr_, index) ? ogr_->GetFieldAsInteger64(index) : 0;
}

double Feature::get_field_as_double(int index) const {
  return valid_field(ogr_, index) ? ogr_->GetFieldAsDouble(index) : 0.0;
}

const char* Feature::get_field_as_string(int index) const {
  return valid_field(ogr_, index) ? ogr_->GetFieldAsString(index) : "";
}

const int* Feature::get_field_as_integer_list(int index, int* count) const {
  if (count) {
    *count = 0;
  }
  return valid_field(ogr_, index) ? ogr_->GetFieldAsIntegerList(index, count)
                                  : nullptr;
}

const GIntBig* Feature::get_field_as_integer64_list(int index,
                                                    int* count) const {
  if (count) {
    *count = 0;
  }
  return valid_field(ogr_, index) ? ogr_->GetFieldAsInteger64List(index, count)
                                  : nullptr;
}

const double* Feature::get_field_as_real_list(int index, int* count) const {
  if (count) {
    *count = 0;
  }
  return valid_field(ogr_, index) ? ogr_->GetFieldAsDoubleList(index, count)
                                  : nullptr;
}

char** Feature::get_field_as_string_list(int index) const {
  return valid_field(ogr_, index) ? ogr_->GetFieldAsStringList(index)
                                  : nullptr;
}

GByte* Feature::get_field_as_binary(int index, int* byte_count) const {
  if (byte_count) {
    *byte_count = 0;
  }
  return valid_field(ogr_, index) ? ogr_->GetFieldAsBinary(index, byte_count)
                                  : nullptr;
}

bool Feature::get_field_as_date_time(int index, int* year, int* month, int* day,
                                     int* hour, int* minute, float* second,
                                     int* tzflag) const {
  if (!valid_field(ogr_, index)) {
    return false;
  }
  return ogr_->GetFieldAsDateTime(index, year, month, day, hour, minute, second,
                                  tzflag) != 0;
}

int Feature::set_field(int index, int value) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, value);
  return k_ok;
}

int Feature::set_field(int index, GIntBig value) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, value);
  return k_ok;
}

int Feature::set_field(int index, double value) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, value);
  return k_ok;
}

int Feature::set_field(int index, const char* value) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, value);
  return k_ok;
}

int Feature::set_field_integer_list(int index, int count, const int* values) {
  if (!valid_field(ogr_, index) || count < 0) {
    return k_fail;
  }
  ogr_->SetField(index, count, values);
  return k_ok;
}

int Feature::set_field_integer64_list(int index, int count,
                                      const GIntBig* values) {
  if (!valid_field(ogr_, index) || count < 0) {
    return k_fail;
  }
  ogr_->SetField(index, count, values);
  return k_ok;
}

int Feature::set_field_real_list(int index, int count, const double* values) {
  if (!valid_field(ogr_, index) || count < 0) {
    return k_fail;
  }
  ogr_->SetField(index, count, values);
  return k_ok;
}

int Feature::set_field_string_list(int index, CSLConstList values) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, values);
  return k_ok;
}

int Feature::set_field_binary(int index, int byte_count, const void* data) {
  if (!valid_field(ogr_, index) || byte_count < 0) {
    return k_fail;
  }
  ogr_->SetField(index, byte_count, data);
  return k_ok;
}

int Feature::set_field_date_time(int index, int year, int month, int day,
                                 int hour, int minute, float second,
                                 int tzflag) {
  if (!valid_field(ogr_, index)) {
    return k_fail;
  }
  ogr_->SetField(index, year, month, day, hour, minute, second, tzflag);
  return k_ok;
}

long append_cloned_feature(OGRLayer* dest, const OGRFeature* src) {
  if (!dest || !src) {
    return k_fail;
  }
  OGRFeature* out = OGRFeature::CreateFeature(dest->GetLayerDefn());
  if (!out) {
    return k_fail;
  }
  out->SetFrom(src);
  if (src->GetGeometryRef()) {
    out->SetGeometry(src->GetGeometryRef());
  }
  const OGRErr err = dest->CreateFeature(out);
  OGRFeature::DestroyFeature(out);
  return err == OGRERR_NONE ? k_ok : k_fail;
}

long copy_ogr_layer(OGRLayer* dest, OGRLayer* src) {
  if (!dest || !src) {
    return k_fail;
  }
  src->ResetReading();
  while (OGRFeature* feat = src->GetNextFeature()) {
    append_cloned_feature(dest, feat);
    OGRFeature::DestroyFeature(feat);
  }
  return k_ok;
}

}  // namespace gis
