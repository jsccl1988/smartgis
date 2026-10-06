// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_PUMP_PUMP_H_
#define APP_VIEWS_HARNESS_COMMON_PUMP_PUMP_H_

#include <windows.h>

namespace app {
namespace detail {

// Pumps the thread message queue for up to |ms| milliseconds.
void pump_messages(DWORD ms);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_PUMP_PUMP_H_
