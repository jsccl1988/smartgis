// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_ERR_H_
#define SCENIC_DETAIL_ERR_H_

#include <cmath>
#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Scenic-owned error / safe-delete helpers. Replaces legacy/core/macros for
// scenic_copy TUs — no #include "legacy/...".

namespace scenic {
namespace detail {

enum Err {
  kErrNone = 0,
  kErrFailure,
  kErrUnknown,
  kErrNotEnoughMem,

  kErrOpenInner,
  kErrArchiveInner,
  kErrCreateInner,
  kErrSaveInner,
  kErrMathInner,

  kErrFuncInner,

  kErrInvalidHandle,
  kErrInvalidParam,
  kErrInvalidFile,

  kErrUnsupported,
  kErrUnsupportedDevice,
  kErrUnsupportedFmts,
  kErrUnsupportedGeotype,

  kErrDsInner,
  kErrDbOper,
  kErrSmfOper,

  kWrnAlreadyExist,
};

// Do not define kPi / kEpsilon here — base/math already owns those names and
// scenic::detail re-exports them via math_alias.h.
inline constexpr double kCoordEpsilon = 1e-5;

// Transitional name used by rhi2d host / LTH / xform (was leftover dEPSILON).
inline constexpr double dEPSILON = kCoordEpsilon;

inline bool is_equal(double a, double b, double eps) {
  return fabs(a - b) < eps;
}

}  // namespace detail
}  // namespace scenic

#define SAFE_DELETE(p)   \
  do {                   \
    if ((p) != nullptr) { \
      delete (p);        \
      (p) = nullptr;     \
    }                    \
  } while (0)

#define SAFE_DELETE_A(p) \
  do {                   \
    if ((p) != nullptr) { \
      delete[] (p);      \
      (p) = nullptr;     \
    }                    \
  } while (0)

#define EQUAL(a, b) \
  (::scenic::detail::is_equal((a), (b), ::scenic::detail::kCoordEpsilon))

// Transitional aliases while scenic TUs finish the Smt* strip.
#define SMT_ERR_NONE ::scenic::detail::kErrNone
#define SMT_ERR_FAILURE ::scenic::detail::kErrFailure
#define SMT_ERR_UNKOWN ::scenic::detail::kErrUnknown
#define SMT_ERR_NOT_ENOUGH_MEM ::scenic::detail::kErrNotEnoughMem
#define SMT_ERR_INVALID_HANDLE ::scenic::detail::kErrInvalidHandle
#define SMT_ERR_INVALID_PARAM ::scenic::detail::kErrInvalidParam
#define SMT_ERR_INVALID_FILE ::scenic::detail::kErrInvalidFile
#define SMT_ERR_UNSUPPORTED ::scenic::detail::kErrUnsupported
#define SMT_ERR_UNSUPPORTED_DEVICE ::scenic::detail::kErrUnsupportedDevice
#define SMT_SAFE_DELETE SAFE_DELETE
#define SMT_SAFE_DELETE_A SAFE_DELETE_A
#define SMT_EQUAL EQUAL
#define SMT_OK S_OK
#define SMT_FALSE S_FALSE

#endif  // SCENIC_DETAIL_ERR_H_
