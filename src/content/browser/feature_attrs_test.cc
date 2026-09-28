// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/feature_attrs.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  content::FeatureId id{};
  id.len = 4;
  id.bytes[0] = 0x0a;
  id.bytes[1] = 0x1b;
  id.bytes[2] = 0x2c;
  id.bytes[3] = 0x3d;

  const std::string token = content::encode_feature_token(id);
  expect(token == "fid:0a1b2c3d", "encode_feature_token hex");

  const content::FeatureId decoded = content::decode_feature_token(token);
  expect(decoded.len == 4, "decode len");
  expect(std::memcmp(decoded.bytes, id.bytes, 4) == 0, "decode bytes");

  const content::FeatureId raw =
      content::decode_feature_token("0a1b2c3d");
  expect(raw.len == 4 && raw.bytes[0] == 0x0a, "decode without prefix");

  std::vector<content::NamedField> fields = {{"name", "a"}, {"type", "point"}};
  expect(content::apply_named_field(&fields, "name", "b"), "update existing");
  expect(fields[0].value == "b", "name updated");
  expect(content::apply_named_field(&fields, "anno", "label"), "append field");
  expect(fields.size() == 3 && fields[2].name == "anno", "appended");
  expect(!content::apply_named_field(&fields, "", "x"), "reject empty name");
  expect(!content::apply_named_field(nullptr, "name", "x"), "reject null");

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("content_feature_attrs_test: ok\n");
  return 0;
}
