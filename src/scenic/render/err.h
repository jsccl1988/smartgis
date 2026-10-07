// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_ERR_H_
#define SCENIC_ERR_H_

#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Scenic-owned error codes. Numeric values stay stable for leftover `long`
// return contracts (0 = success).

namespace scenic {
namespace detail {

enum Err {
  kErrNone = 0,
  kErrFailure,
  kErrUnknown,
  kErrNotEnoughMem

  kErrOpenInner,
  kErrArchiveInner,
  kErrCreateInner,
  kErrSaveInner,
  kErrMathInner

  kErrFuncInner

  kErrInvalidHandle,
  kErrInvalidParam,
  kErrInvalidFile

  kErrUnsupported,
  kErrUnsupportedDevice,
  kErrUnsupportedFmts,
  kErrUnsupportedGeotype

  kErrDsInner,
  kErrDbOper,
  kErrSmfOper

  kWrnAlreadyExist,
};

inline constexpr double kCoordEpsilon = 1e-5;
inline constexpr double dEPSILON = kCoordEpsilon;

inline bool is_equal(double a, double b, double eps) {
  return fabs(a - b) < eps;
}

}  // namespace detail
}  // namespace scenic

#define SAFE_DELETE(p)    \
  do {                    \
    if ((p) != nullptr) { \
      delete (p);         \
      (p) = nullptr;      \
    }                     \
  } while (0)

#define SAFE_DELETE_A(p)  \
  do {                    \
    if ((p) != nullptr) { \
      delete[] (p);       \
      (p) = nullptr;      \
    }                     \
  } while (0)

#endif  // SCENIC_ERR_H_
