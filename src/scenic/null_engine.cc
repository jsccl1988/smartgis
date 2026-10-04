// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/engine.h"

namespace scenic {
namespace {

class NullEngine : public Engine {
 public:
  explicit NullEngine(Kind kind) : kind_(kind) {}

  bool initialize(const SessionDesc&) override { return true; }
  void shutdown() override {}
  bool present() override { return true; }
  Kind kind() const override { return kind_; }
  bool is_null() const override { return true; }

 private:
  Kind kind_;
};

}  // namespace

Engine* create_null_engine(Kind kind) { return new NullEngine(kind); }

}  // namespace scenic
