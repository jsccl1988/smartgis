// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>
#include <utility>

#include "cpl_string.h"
#include "gis/feature/feature.h"
#include "ogrsf_frmts.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

OGRFeatureDefn* make_defn() {
  auto* defn = new OGRFeatureDefn("pts");
  defn->Reference();
  OGRFieldDefn name("name", OFTString);
  defn->AddFieldDefn(&name);
  OGRFieldDefn count("count", OFTInteger);
  defn->AddFieldDefn(&count);
  OGRFieldDefn big("big", OFTInteger64);
  defn->AddFieldDefn(&big);
  OGRFieldDefn mag("mag", OFTReal);
  defn->AddFieldDefn(&mag);
  OGRFieldDefn ints("ints", OFTIntegerList);
  defn->AddFieldDefn(&ints);
  OGRFieldDefn bigs("bigs", OFTInteger64List);
  defn->AddFieldDefn(&bigs);
  OGRFieldDefn reals("reals", OFTRealList);
  defn->AddFieldDefn(&reals);
  OGRFieldDefn tags("tags", OFTStringList);
  defn->AddFieldDefn(&tags);
  OGRFieldDefn blob("blob", OFTBinary);
  defn->AddFieldDefn(&blob);
  OGRFieldDefn when("when", OFTDateTime);
  defn->AddFieldDefn(&when);
  return defn;
}

}  // namespace

int main() {
  OGRFeatureDefn* defn = make_defn();

  OGRFeature* raw = OGRFeature::CreateFeature(defn);
  raw->SetFID(7);
  raw->SetField(0, "a");
  raw->SetGeometryDirectly(new OGRPoint(1.0, 2.0));

  {
    gis::Feature owned(raw, true);
    expect(owned.owns_ogr() && owned.ogr() == raw, "own ogr");
    expect(owned.id() == 7, "fid");
    expect(owned.field_index("name") == 0, "field index");
    expect(owned.field_count() == 10, "field count");
    expect(owned.field_type(0) == OFTString, "string type");
    expect(owned.set_field(0, "b") == 0, "set field string");
    expect(std::strcmp(owned.get_field_as_string(0), "b") == 0, "get string");
    expect(owned.set_field(1, 42) == 0, "set integer");
    expect(owned.get_field_as_integer(1) == 42, "get integer");
    expect(owned.set_field(2, static_cast<GIntBig>(1ll << 40)) == 0,
           "set integer64");
    expect(owned.get_field_as_integer64(2) == (1ll << 40), "get integer64");
    expect(owned.set_field(3, 3.5) == 0, "set real");
    expect(owned.get_field_as_double(3) == 3.5, "get real");

    const int ints[] = {1, 2, 3};
    expect(owned.set_field_integer_list(4, 3, ints) == 0, "set int list");
    int n = 0;
    const int* got_ints = owned.get_field_as_integer_list(4, &n);
    expect(n == 3 && got_ints && got_ints[2] == 3, "get int list");

    const GIntBig bigs[] = {9, 8};
    expect(owned.set_field_integer64_list(5, 2, bigs) == 0, "set i64 list");
    n = 0;
    const GIntBig* got_bigs = owned.get_field_as_integer64_list(5, &n);
    expect(n == 2 && got_bigs && got_bigs[0] == 9, "get i64 list");

    const double reals[] = {0.5, 1.5};
    expect(owned.set_field_real_list(6, 2, reals) == 0, "set real list");
    n = 0;
    const double* got_reals = owned.get_field_as_real_list(6, &n);
    expect(n == 2 && got_reals && got_reals[1] == 1.5, "get real list");

    char** tags = nullptr;
    tags = CSLAddString(tags, "x");
    tags = CSLAddString(tags, "y");
    expect(owned.set_field_string_list(7, tags) == 0, "set string list");
    CSLDestroy(tags);
    char** got_tags = owned.get_field_as_string_list(7);
    expect(got_tags && CSLCount(got_tags) == 2 &&
               std::strcmp(got_tags[1], "y") == 0,
           "get string list");

    const GByte blob[] = {0xab, 0xcd};
    expect(owned.set_field_binary(8, 2, blob) == 0, "set binary");
    int nbytes = 0;
    GByte* got_blob = owned.get_field_as_binary(8, &nbytes);
    expect(nbytes == 2 && got_blob && got_blob[0] == 0xab, "get binary");

    expect(owned.set_field_date_time(9, 2026, 10, 4, 2, 30, 0.0f, 0) == 0,
           "set datetime");
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, tz = 0;
    float sec = 0.0f;
    expect(owned.get_field_as_date_time(9, &y, &mo, &d, &h, &mi, &sec, &tz) &&
               y == 2026 && mo == 10 && d == 4 && h == 2 && mi == 30,
           "get datetime");
    expect(owned.geometry() != nullptr, "geometry");

    gis::Feature alias(owned.release(), true);
    expect(!owned.owns_ogr() && owned.ogr() == nullptr, "released");
    alias.set_id(9);
    expect(alias.id() == 9, "set_id");
    gis::Feature moved(std::move(alias));
    expect(moved.owns_ogr() && moved.id() == 9, "move owns");
    expect(alias.ogr() == nullptr && !alias.owns_ogr(), "moved-from empty");
  }

  OGRFeature* borrowed_raw = OGRFeature::CreateFeature(defn);
  borrowed_raw->SetFID(4);
  {
    gis::Feature borrowed = gis::Feature::borrow(borrowed_raw);
    expect(!borrowed.owns_ogr() && borrowed.id() == 4, "borrow");
  }
  expect(borrowed_raw->GetFID() == 4, "borrow does not destroy");
  OGRFeature::DestroyFeature(borrowed_raw);

  defn->Release();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("feature_test ok\n");
  return 0;
}
