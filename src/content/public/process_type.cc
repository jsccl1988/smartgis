// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/process_type.h"

#include <cstddef>
#include <cwchar>

namespace content {
namespace {

constexpr wchar_t kTypeSwitchPrefix[] = L"--type=";

ProcessType process_type_from_switch_value(const wchar_t* value) {
  if (!value || value[0] == L'\0' || wcscmp(value, L"browser") == 0) {
    return ProcessType::kBrowser;
  }
  if (wcscmp(value, L"renderer") == 0) {
    return ProcessType::kRenderer;
  }
  if (wcscmp(value, L"gpu") == 0) {
    return ProcessType::kGpu;
  }
  if (wcscmp(value, L"utility") == 0) {
    return ProcessType::kUtility;
  }
  return ProcessType::kBrowser;
}

}  // namespace

ProcessType ProcessTypeFromCommandLine(int argc, wchar_t** argv) {
  if (!argv || argc <= 0) {
    return ProcessType::kBrowser;
  }

  const size_t prefix_len = wcslen(kTypeSwitchPrefix);
  for (int i = 0; i < argc; ++i) {
    const wchar_t* arg = argv[i];
    if (!arg) {
      continue;
    }
    // Prefix form: --type=renderer (value is the suffix after --type=).
    if (wcsncmp(arg, kTypeSwitchPrefix, prefix_len) == 0) {
      return process_type_from_switch_value(arg + prefix_len);
    }
  }
  return ProcessType::kBrowser;
}

const wchar_t* ProcessTypeSwitchValue(ProcessType t) {
  switch (t) {
    case ProcessType::kRenderer:
      return L"renderer";
    case ProcessType::kGpu:
      return L"gpu";
    case ProcessType::kUtility:
      return L"utility";
    case ProcessType::kBrowser:
    default:
      return L"browser";
  }
}

}  // namespace content
