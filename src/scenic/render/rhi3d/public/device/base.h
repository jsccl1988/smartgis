// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _RD3D_RDBASE_H
#define _RD3D_RDBASE_H

#include <cstring>

#include "base/math/math.h"
#include "scenic/scenic_impl_export.h"
#include "scenic/render/rhi3d/public/device/render_defs.h"

namespace scenic {
namespace detail {
struct Viewport3D {
  ulong ulX;  // position of upper
  ulong ulY;  // ... left corner
  ulong ulWidth;
  ulong ulHeight;
  float fFovy;
  float fZNear;
  float fZFar;

  Viewport3D()
      : ulX(0), ulY(0), ulWidth(0), ulHeight(0), fFovy(0), fZNear(0), fZFar(0) {
    ;
  }

  Viewport3D(ulong _ulX, ulong _ulY, ulong _ulWidth, ulong _ulHeight,
             float _fFovy, float _fZNear, float _fZFar)
      : ulX(_ulX),
        ulY(_ulY),
        ulWidth(_ulWidth),
        ulHeight(_ulHeight),
        fFovy(_fFovy),
        fZNear(_fZNear),
        fZFar(_fZFar) {
    ;
  }
};

struct LEGACY_RENDER_EXPORT Color {
  union {
    struct {
      float fRed, fGreen, fBlue, fA;
    };
    float c[4];
  };

  Color(float red, float green, float blue, float a = 1.);
  Color(void);
};

enum LIGHTTYPE {
  LGT_DIRECTIONAL,  // directional light source
  LGT_POINT,        // point light source
  LGT_SPOT          // spot light source
};

class LEGACY_RENDER_EXPORT Light {
 public:
  Light(void);

  // set
  void SetType(LIGHTTYPE type);
  void SetDiffuseValue(const Color& diffuse);
  void SetSpecularValue(const Color& specular);
  void SetAmbientValue(const Color& ambient);

  void SetPoistion(const ::base::Vector4& position);
  void SetDirection(const ::base::Vector4& direction);

  void SetExponent(float exponent) { m_fExponent = exponent; }
  void SetCutoffAngle(float cutoffangle) { m_fCutoffAngle = cutoffangle; }

  void SetRange(float fRange) { m_fRange = fRange; }
  void SetThetaAngle(float fThetaAngle) { m_fThetaAngle = fThetaAngle; }
  void SetPhiAngle(float fPhiAngle) { m_fPhiAngle = fPhiAngle; }

  void SetAttenuationConstant(float constant) {
    m_fAttenuationConstant = constant;
  }
  void SetAttenuationLinear(float linear) { m_fAttenuationLinear = linear; }
  void SetAttenuationQuadric(float quadric) { m_fAttenuationQuadric = quadric; }

  // get
  LIGHTTYPE GetType(void);
  const Color& GetDiffuseValue(void);
  const Color& GetSpecularValue(void);
  const Color& GetAmbientValue(void);

  const Vector4& GetPosition(void);
  const Vector4& GetDirection(void);

  float GetExponent() { return m_fExponent; }
  float GetCutoffAngle() { return m_fCutoffAngle; }

  float GetRange() { return m_fRange; }
  float GetThetaAngle() { return m_fThetaAngle; }
  float GetPhiAngle() { return m_fPhiAngle; }

  float GetAttenuationConstant() { return m_fAttenuationConstant; }
  float GetAttenuationLinear() { return m_fAttenuationLinear; }
  float GetAttenuationQuadric() { return m_fAttenuationQuadric; }

 private:
  LIGHTTYPE m_Type;      // type of light
  Color m_cDiffuse;   // RGBA diffuse light value
  Color m_cSpecular;  // RGBA specular light value
  Color m_cAmbient;   // RGBA ambient light value
  Vector4 m_vPosition;   // light position
  Vector4 m_vDirection;  // light direction

  // opengl spot light model
  float m_fCutoffAngle;  // angle of spot light cone
  float m_fExponent;

  // d3d spot light model
  float m_fRange;       // range of light
  float m_fThetaAngle;  // angle of spot light inner cone
  float m_fPhiAngle;    // angle of spot light outer cone

  float m_fAttenuationConstant;  // change of intensity over distance
  float m_fAttenuationLinear;    // change of intensity over distance
  float m_fAttenuationQuadric;   // change of intensity over distance
};

class LEGACY_RENDER_EXPORT Material {
 public:
  Material(void);

  // set
  void SetDiffuseValue(const Color& diffuse);
  void SetSpecularValue(const Color& specular);
  void SetAmbientValue(const Color& ambient);
  void SetEmissiveValue(const Color& emissive);
  void SetShininessValue(float shininess);

  // get
  const Color& GetDiffuseValue(void);
  const Color& GetSpecularValue(void);
  const Color& GetAmbientValue(void);
  const Color& GetEmissiveValue(void);
  float GetShininessValue(void);

 private:
  Color m_cDiffuse;   // RGBA diffuse light value
  Color m_cAmbient;   // RGBA ambient light value
  Color m_cSpecular;  // RGBA specular light value
  Color m_cEmissive;  // RGBA emissive light value
  float m_fShininess;    // shininess index
};

inline Color::Color(float red, float green, float blue, float a) {
  fRed = red;
  fGreen = green;
  fBlue = blue;
  fA = a;
}

inline Color::Color(void) {
  fRed = 0.;
  fGreen = 0.;
  fBlue = 0.;
  fA = 1.;
}

inline Light::Light(void) {
  m_fCutoffAngle = 180.f;
  m_fExponent = 0.0f;
  m_fAttenuationConstant = 1.f;
  m_fAttenuationLinear = 0.f;
  m_fAttenuationQuadric = 0.f;
  m_fRange = 1000;
  m_fThetaAngle = 180;
  m_fPhiAngle = 45;
}

inline void Light::SetType(LIGHTTYPE type) { m_Type = type; }

inline void Light::SetDiffuseValue(const Color& diffuse) {
  m_cDiffuse = diffuse;
}

inline void Light::SetSpecularValue(const Color& specular) {
  m_cSpecular = specular;
}

inline void Light::SetAmbientValue(const Color& ambient) {
  m_cAmbient = ambient;
}

inline void Light::SetPoistion(const ::base::Vector4& position) {
  m_vPosition = position;
}

inline void Light::SetDirection(const ::base::Vector4& direction) {
  m_vDirection = direction;
}

inline LIGHTTYPE Light::GetType() { return m_Type; }

inline const Color& Light::GetDiffuseValue(void) { return m_cDiffuse; }

inline const Color& Light::GetSpecularValue(void) { return m_cSpecular; }

inline const Color& Light::GetAmbientValue(void) { return m_cAmbient; }

inline const Vector4& Light::GetPosition(void) { return m_vPosition; }

inline const Vector4& Light::GetDirection(void) { return m_vDirection; }

inline Material::Material(void) { ; }

inline void Material::SetDiffuseValue(const Color& diffuse) {
  m_cDiffuse = diffuse;
}

inline void Material::SetSpecularValue(const Color& specular) {
  m_cSpecular = specular;
}

inline void Material::SetAmbientValue(const Color& ambient) {
  m_cAmbient = ambient;
}

inline void Material::SetEmissiveValue(const Color& emissive) {
  m_cEmissive = emissive;
}

inline void Material::SetShininessValue(float shininess) {
  m_fShininess = shininess;
}

inline const Color& Material::GetDiffuseValue(void) { return m_cDiffuse; }

inline const Color& Material::GetSpecularValue(void) {
  return m_cSpecular;
}

inline const Color& Material::GetAmbientValue(void) { return m_cAmbient; }

inline const Color& Material::GetEmissiveValue(void) {
  return m_cEmissive;
}

inline float Material::GetShininessValue(void) { return m_fShininess; }

}  // namespace detail
}  // namespace scenic

#if !defined(LEGACY_RENDER_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "scenic_impl_d.lib")
#else
#pragma comment(lib, "scenic_impl.lib")
#endif
#endif

#endif  //_RD3D_RDBASE_H