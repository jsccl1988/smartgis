// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_PROCESS_TYPE_H
#define CONTENT_PUBLIC_PROCESS_TYPE_H

// Multiprocess role for one chrome PE relaunched with --type=.
// Browser is the default when the switch is omitted.
namespace content {

enum class ProcessType { kBrowser, kRenderer, kGpu, kUtility };

// First --type= argv token wins. Missing, empty, or "browser" is kBrowser.
ProcessType ProcessTypeFromCommandLine(int argc, wchar_t** argv);

// Switch value only (no "--type=" prefix): L"browser", L"renderer", ...
const wchar_t* ProcessTypeSwitchValue(ProcessType t);

}  // namespace content

#endif  // CONTENT_PUBLIC_PROCESS_TYPE_H
