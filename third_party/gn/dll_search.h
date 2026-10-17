// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef THIRD_PARTY_GN_DLL_SEARCH_H_
#define THIRD_PARTY_GN_DLL_SEARCH_H_

// Point the Windows DLL search at <exe_dir>/../third_party/bin when that
// directory exists (SDK layout). The exe directory is still searched first,
// so a flat gen root (DLLs beside the exe) keeps working.
void prepare_dll_search_path();

#endif  // THIRD_PARTY_GN_DLL_SEARCH_H_
