// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_ENVELOPE_H_
#define GIS_ENVELOPE_H_

#include <algorithm>

namespace gis {

// Unset marker for axis-aligned extents (legacy SMT_C_INVALID_DBF_VALUE).
inline constexpr double k_envelope_unset = 1e10;

// Axis-aligned map / geometry extent (MBR). Header-only so base leftover
// helpers can use it without linking gis.dll.
class Envelope {
 public:
  Envelope()
      : MinX(k_envelope_unset),
        MaxX(k_envelope_unset),
        MinY(k_envelope_unset),
        MaxY(k_envelope_unset) {}

  bool is_init() const {
    return MinX != k_envelope_unset || MinY != k_envelope_unset ||
           MaxX != k_envelope_unset || MaxY != k_envelope_unset;
  }

  void merge(const Envelope& other) {
    if (is_init() && other.is_init()) {
      MinX = (std::min)(MinX, other.MinX);
      MaxX = (std::max)(MaxX, other.MaxX);
      MinY = (std::min)(MinY, other.MinY);
      MaxY = (std::max)(MaxY, other.MaxY);
    } else {
      MinX = other.MinX;
      MaxX = other.MaxX;
      MinY = other.MinY;
      MaxY = other.MaxY;
    }
  }

  void merge(double x, double y) {
    if (is_init()) {
      MinX = (std::min)(MinX, x);
      MaxX = (std::max)(MaxX, x);
      MinY = (std::min)(MinY, y);
      MaxY = (std::max)(MaxY, y);
    } else {
      MinX = MaxX = x;
      MinY = MaxY = y;
    }
  }

  void intersect(const Envelope& other) {
    if (intersects(other)) {
      if (is_init()) {
        MinX = (std::max)(MinX, other.MinX);
        MaxX = (std::min)(MaxX, other.MaxX);
        MinY = (std::max)(MinY, other.MinY);
        MaxY = (std::min)(MaxY, other.MaxY);
      } else {
        MinX = other.MinX;
        MaxX = other.MaxX;
        MinY = other.MinY;
        MaxY = other.MaxY;
      }
    } else {
      MinX = MaxX = MinY = MaxY = 0;
    }
  }

  bool intersects(const Envelope& other) const {
    return MinX <= other.MaxX && MaxX >= other.MinX && MinY <= other.MaxY &&
           MaxY >= other.MinY;
  }

  bool contains(const Envelope& other) const {
    return MinX <= other.MinX && MinY <= other.MinY && MaxX >= other.MaxX &&
           MaxY >= other.MaxY;
  }

  bool contains(double x, double y) const {
    return MinX <= x && MinY <= y && MaxX >= x && MaxY >= y;
  }

  double MinX;
  double MaxX;
  double MinY;
  double MaxY;
};

}  // namespace gis

#endif  // GIS_ENVELOPE_H_
