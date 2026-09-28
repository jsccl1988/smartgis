/*
File:    bl_envelope.h

Desc:    Envelope,MBR

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_ENVELOPE_H
#define _BL_ENVELOPE_H


#include "base/core/export.h"
#include "legacy/core/core.h"

namespace base {
class BASE_EXPORT Envelope {
 public:
  Envelope();
  virtual ~Envelope() = default;

  bool is_init() const;

  void merge(Envelope const& sOther);
  void merge(double dfX, double dfY);

  void intersect(Envelope const& sOther);
  bool intersects(Envelope const& other) const;
  bool contains(Envelope const& other) const;
  bool contains(double dfX, double dfY) const;

  double MinX;
  double MaxX;
  double MinY;
  double MaxY;
};
}  // namespace base

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_BL_ENVELOPE_H