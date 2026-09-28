// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <utility>

#include "legacy/core/core.h"
#include "gis/model/feature/feature.h"
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
    expect(owned.set_field(0, "b") == SMT_ERR_NONE, "set field");
    expect(owned.geometry() != nullptr, "geometry");

    gis::SmtFeature alias(owned.release(), true);
    expect(!owned.owns_ogr() && owned.ogr() == nullptr, "released");
    alias.SetID(9);
    expect(alias.id() == 9, "legacy SetID");
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
