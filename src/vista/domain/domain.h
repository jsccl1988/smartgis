// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CPU session a host attaches to a World. Each concrete session is one
// domain (atmosphere today; factory, geology, offshore, storm surge, and
// space later). Render effects are not included here, and this header does
// not implement any domain.

#ifndef VISTA_DOMAIN_DOMAIN_H_
#define VISTA_DOMAIN_DOMAIN_H_

namespace vista {

enum class DomainKind {
  kAtmosphere,
  kFactory,
  kGeology,
  kOffshore,
  kStormSurge,
  kSpace,
};

// CPU session a host attaches to a World. Render effects are not included here.
class DomainSession {
 public:
  virtual ~DomainSession() = default;
  virtual DomainKind kind() const = 0;
};

}  // namespace vista

#endif  // VISTA_DOMAIN_DOMAIN_H_
