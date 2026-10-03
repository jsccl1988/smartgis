// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_SHELL_AMBOX_TITLE_H_
#define LEGACY_UI_SHELL_AMBOX_TITLE_H_

#if defined(XAMBOX_EXPORTS)
#define XAMBOX_EXPORT __declspec(dllexport)
#else
#define XAMBOX_EXPORT __declspec(dllimport)
#endif

namespace ui {

// Decode leftover CP936 or UTF-8 AM titles into a CString for the process ACP.
XAMBOX_EXPORT CString ambox_title_for_display(const char* name);

// ASCII-only Outlook / tree-root caption (Outlook faces paint CJK as '?').
XAMBOX_EXPORT CString ambox_outlook_caption(const char* name);

}  // namespace ui

#endif  // LEGACY_UI_SHELL_AMBOX_TITLE_H_
