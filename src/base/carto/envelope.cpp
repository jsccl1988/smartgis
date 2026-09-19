#include "base/carto/envelope.h"

namespace base {
Envelope::Envelope() { MinX = MaxX = MinY = MaxY = SMT_C_INVALID_DBF_VALUE; }

bool Envelope::is_init() const {
  return (MinX != SMT_C_INVALID_DBF_VALUE || MinY != SMT_C_INVALID_DBF_VALUE ||
          MaxX != SMT_C_INVALID_DBF_VALUE || MaxY != SMT_C_INVALID_DBF_VALUE);
}

void Envelope::merge(Envelope const& sOther) {
  if (is_init() && sOther.is_init()) {
    MinX = min(MinX, sOther.MinX);
    MaxX = max(MaxX, sOther.MaxX);
    MinY = min(MinY, sOther.MinY);
    MaxY = max(MaxY, sOther.MaxY);
  } else {
    MinX = sOther.MinX;
    MaxX = sOther.MaxX;
    MinY = sOther.MinY;
    MaxY = sOther.MaxY;
  }
}
void Envelope::merge(double dfX, double dfY) {
  if (is_init()) {
    MinX = min(MinX, dfX);
    MaxX = max(MaxX, dfX);
    MinY = min(MinY, dfY);
    MaxY = max(MaxY, dfY);
  } else {
    MinX = MaxX = dfX;
    MinY = MaxY = dfY;
  }
}

void Envelope::intersect(Envelope const& sOther) {
  if (intersects(sOther)) {
    if (is_init()) {
      MinX = max(MinX, sOther.MinX);
      MaxX = min(MaxX, sOther.MaxX);
      MinY = max(MinY, sOther.MinY);
      MaxY = min(MaxY, sOther.MaxY);
    } else {
      MinX = sOther.MinX;
      MaxX = sOther.MaxX;
      MinY = sOther.MinY;
      MaxY = sOther.MaxY;
    }
  } else {
    MinX = 0;
    MaxX = 0;
    MinY = 0;
    MaxY = 0;
  }
}

bool Envelope::intersects(Envelope const& other) const {
  return MinX <= other.MaxX && MaxX >= other.MinX && MinY <= other.MaxY &&
         MaxY >= other.MinY;
}

bool Envelope::contains(Envelope const& other) const {
  return MinX <= other.MinX && MinY <= other.MinY && MaxX >= other.MaxX &&
         MaxY >= other.MaxY;
}

bool Envelope::contains(double dfX, double dfY) const {
  return MinX <= dfX && MinY <= dfY && MaxX >= dfX && MaxY >= dfY;
}
}  // namespace base