// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PUMP_H_
#define IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PUMP_H_

#include <windows.h>

namespace app {
namespace detail {

// Pumps the thread message queue for up to |ms| milliseconds.
void pump_messages(DWORD ms);

}  // namespace detail

void pump_views_messages(DWORD ms);

}  // namespace app

#endif  // IL_RUNTIME_CAPABILITY_HORIZON_ATOM_PUMP_H_
