// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_BAS_STRUCT_H
#define _SMT_BAS_STRUCT_H

#include "legacy/core/core.h"
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned int uint;
typedef ushort varType;
typedef unsigned char byte;

#define TEMP_BUFFER_SIZE 255

namespace base {
struct lPoint {
  long x, y;

  lPoint(void) {
    x = 0;
    y = 0;
  }

  lPoint(long _x, long _y) {
    x = _x;
    y = _y;
  }

  inline bool operator==(lPoint lpt) const {
    return (x == lpt.x && y == lpt.y);
  }

  inline bool operator!=(lPoint lpt) const { return !(*this == lpt); }
};

struct l3DPoint {
  long x, y, z;

  l3DPoint(void) {
    x = 0;
    y = 0;
    z = 0;
  }

  l3DPoint(long _x, long _y, long _z) {
    x = _x;
    y = _y;
    z = _z;
  }

  inline bool operator==(l3DPoint lpt) const {
    return (x == lpt.x && y == lpt.y && z == lpt.z);
  }

  inline bool operator!=(l3DPoint lpt) const { return !(*this == lpt); }
};

struct lRect {
  lPoint lb;
  lPoint rt;

  inline void merge(long x, long y) {
    lPoint point0(0, 0);
    if (lb == point0 && rt == point0) {
      lb = rt = lPoint(x, y);
    } else {
      lb.x = min(lb.x, x);
      rt.x = max(rt.x, x);
      lb.y = min(lb.y, y);
      rt.y = max(rt.y, y);
    }
  }

  inline long height(void) const { return abs(rt.y - lb.y); }

  inline long width(void) const { return abs(rt.x - lb.x); }
};

struct fPoint {
  float x, y;

  fPoint(void) {
    x = 0;
    y = 0;
  }

  fPoint(float _x, float _y) {
    x = _x;
    y = _y;
  }

  inline bool operator==(fPoint lpt) const {
    return (SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y));
  }

  inline bool operator!=(fPoint lpt) const { return !(*this == lpt); }
};

struct f3DPoint {
  float x, y, z;

  f3DPoint(void) {
    x = 0;
    y = 0;
    z = 0;
  }

  f3DPoint(float _x, float _y, float _z) {
    x = _x;
    y = _y;
    z = _z;
  }

  inline bool operator==(f3DPoint lpt) const {
    return (SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y) && SMT_EQUAL(z, lpt.z));
  }

  inline bool operator!=(f3DPoint lpt) const { return !(*this == lpt); }
};

struct fRect {
  fPoint lb;
  fPoint rt;

  inline void merge(float x, float y) {
    fPoint point0(0, 0);
    if (lb == point0 && rt == point0) {
      lb = rt = fPoint(x, y);
    } else {
      lb.x = min(lb.x, x);
      rt.x = max(rt.x, x);
      lb.y = min(lb.y, y);
      rt.y = max(rt.y, y);
    }
  }

  inline float height(void) const { return fabs(rt.y - lb.y); }

  inline float width(void) const { return fabs(rt.x - lb.x); }
};

struct dbfPoint {
  double x, y;

  dbfPoint(void) {
    x = 0;
    y = 0;
  }

  dbfPoint(double _x, double _y) {
    x = _x;
    y = _y;
  }

  inline bool operator==(dbfPoint lpt) const {
    return (SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y));
  }

  inline bool operator!=(dbfPoint lpt) const {
    return !(SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y));
  }
};

struct dbf3DPoint {
  double x, y, z;

  dbf3DPoint(void) {
    x = 0;
    y = 0;
    z = 0;
  }

  dbf3DPoint(double _x, double _y, double _z) {
    x = _x;
    y = _y;
    z = _z;
  }

  inline bool operator==(dbf3DPoint lpt) const {
    return (SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y));
  }

  inline bool operator!=(dbf3DPoint lpt) const {
    return !(SMT_EQUAL(x, lpt.x) && SMT_EQUAL(y, lpt.y));
  }
};

struct dbfRect {
  dbfPoint lb;
  dbfPoint rt;

  inline void merge(double x, double y) {
    dbfPoint point0(0, 0);
    if (lb == point0 && rt == point0) {
      lb = rt = dbfPoint(x, y);
    } else {
      lb.x = min(lb.x, x);
      rt.x = max(rt.x, x);
      lb.y = min(lb.y, y);
      rt.y = max(rt.y, y);
    }
  }

  inline double height(void) const { return fabs(rt.y - lb.y); }

  inline double width(void) const { return fabs(rt.x - lb.x); }
};

struct SmtTriangle {
  long a, b, c;
  bool bDelete;

  SmtTriangle() : bDelete(false) { a = b = c = -1; }
};

typedef SmtTriangle Smt3DTriangle;

struct SmtTile {
  long lID;
  long lTileBufSize;
  char *pTileBuf;
  long lImageCode;
  bool bVisible;

  fRect rtTileRect;

  SmtTile()
      : pTileBuf(NULL),
        lTileBufSize(0),
        lImageCode(-1),
        lID(-1),
        bVisible(true) {}
};

typedef SmtTile SmtWSTile;

enum SmtVarType {
  SmtInteger = 0,
  SmtIntegerList = 1,
  SmtBool = 2,
  SmtReal = 3,
  SmtRealList = 4,
  SmtByte = 5,
  SmtString = 6,
  SmtStringList = 7,
  SmtBinary = 10,
  SmtDate = 11,
  SmtTime = 12,
  SmtDateTime = 13,
  SmtUnknown = 14
};

struct SmtVariant {
  union {
    int iVal;
    double dbfVal;
    bool boolVal;
    byte byteVal;
    char *bstrVal;

    struct {
      int nCount;
      int *paList;
    } iValList;

    struct {
      int nCount;
      double *paList;
    } dbfValList;

    struct {
      int nCount;
      char **paList;
    } bstrValList;

    struct {
      int nCount;
      byte *paData;
    } blobVal;

    struct {
      ushort Year;
      byte Month;
      byte Day;
      byte Hour;
      byte Minute;
      byte Second;
      // 0=unknown, 1=localtime(ambiguous), 100=GMT, 104=GMT+1, 80=GMT-5, etc.
      byte TZFlag;
    } dateVal;
  };
  varType Vt;
  ushort usReserved1;
  ushort usReserved2;
  ushort usReserved3;
};
}  // namespace base

#endif  //_SMT_BAS_STRUCT_H