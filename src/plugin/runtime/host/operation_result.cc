// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/host/operation_result.h"

#include <mutex>
#include <utility>

namespace plugin {
namespace {

std::mutex g_mu;
std::string g_message;

}  // namespace

void set_operation_result(std::string message) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_message = std::move(message);
}

std::string operation_result() {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_message;
}

}  // namespace plugin
