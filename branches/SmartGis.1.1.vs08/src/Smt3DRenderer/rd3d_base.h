/*
File:    rd3d_base.h

Desc:    ÈýÎ¬»ù´¡¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _RD3D_RDBASE_H
#define _RD3D_RDBASE_H

#include "rd3d_3drenderdefs.h"
#include "ml3d_mathlib.h"
using namespace Smt_3DMath;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	struct Viewport3D 
	{
		ulong ulX;                // position of upper
		ulong ulY;                // ... left corner
		ulong ulWidth;
		ulong ulHeight;
		float fFovy;
		float fZNear;
		float fZFar;

		Viewport3D():ulX(0)
			,ulY(0),ulWidth(0),ulHeight(0)
			,fFovy(0),fZNear(0),fZFar(0)
		{
			;
		}

		Viewport3D(ulong _ulX,ulong _ulY,ulong _ulWidth,ulong _ulHeight,float _fFovy,float _fZNear,float _fZFar):ulX(_ulX)
			,ulY(_ulY),ulWidth(_ulWidth),ulHeight(_ulHeight)
			,fFovy(_fFovy),fZNear(_fZNear),fZFar(_fZFar)
		{
			;
		}
	};

	struct SMT_EXPORT_CLASS  SmtColor
	{
		union 
		{
			struct 
			{
				float fRed,fGreen,fBlue,fA;
			};
			float c[4];
		};

		//
		SmtColor(float red,float green,float blue,float a = 1.);
		SmtColor(void);
	};

	//////////////////////////////////////////////////////////////////////////
    enum  LIGHTTYPE 
	{
		    LGT_DIRECTIONAL,     // directional light source
			LGT_POINT,           // point light source
			LGT_SPOT             // spot light source
	};
	
	class SMT_EXPORT_CLASS SmtLight
	{
	public:
		SmtLight(void);

		//set
		void                     SetType(LIGHTTYPE type);
		void                     SetDiffuseValue(const SmtColor& diffuse);
		void                     SetSpecularValue(const SmtColor& specular);
		void                     SetAmbientValue(const SmtColor& ambient);

        void                     SetPoistion(const Vector4& position);
		void                     SetDirection(const Vector4& direction);


		void                     SetExponent(float exponent){m_fExponent = exponent;}
		void                     SetCutoffAngle(float cutoffangle) {m_fCutoffAngle = cutoffangle;}
		
		void                     SetRange(float fRange){m_fRange = fRange;}
		void                     SetThetaAngle(float fThetaAngle){ m_fThetaAngle = fThetaAngle;}
		void                     SetPhiAngle(float fPhiAngle){ m_fPhiAngle = fPhiAngle;}

		void                     SetAttenuationConstant(float constant) {m_fAttenuationConstant = constant;}	
		void                     SetAttenuationLinear(float linear){m_fAttenuationLinear = linear;}
		void                     SetAttenuationQuadric(float quadric){m_fAttenuationQuadric = quadric;}

		//get
		LIGHTTYPE                GetType(void);
		const SmtColor&          GetDiffuseValue(void); 
		const SmtColor&          GetSpecularValue(void);
		const SmtColor&          GetAmbientValue(void);

		const Vector4&           GetPosition(void);
		const Vector4&           GetDirection(void);

		float                    GetExponent(){return m_fExponent;}
		float                    GetCutoffAngle(){return m_fCutoffAngle;}

		float                    GetRange(){return m_fRange;}
		float                    GetThetaAngle(){return m_fThetaAngle;}
		float                    GetPhiAngle(){return m_fPhiAngle;}

		float                    GetAttenuationConstant(){return m_fAttenuationConstant;}
		float                    GetAttenuationLinear(){return m_fAttenuationLinear;}
		float                    GetAttenuationQuadric(){return m_fAttenuationQuadric;}

	private:
		LIGHTTYPE                m_Type;					// type of light
		SmtColor                 m_cDiffuse;				// RGBA diffuse light value
		SmtColor                 m_cSpecular;				// RGBA specular light value
		SmtColor                 m_cAmbient;				// RGBA ambient light value
		Vector4                  m_vPosition;				// light position
		Vector4                  m_vDirection;				// light direction

		//opengl spot light model
		float                    m_fCutoffAngle;			// angle of spot light cone
		float                    m_fExponent;

		//d3d spot light model
		float					 m_fRange;					// range of light
		float					 m_fThetaAngle;				// angle of spot light inner cone
		float					 m_fPhiAngle;				// angle of spot light outer cone

		float                    m_fAttenuationConstant;    // change of intensity over distance
		float                    m_fAttenuationLinear;      // change of intensity over distance
		float                    m_fAttenuationQuadric;     // change of intensity over distance
	};
	
	//////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtMaterial
	{
	public:
		SmtMaterial(void);
	
		//set
		void                    SetDiffuseValue(const SmtColor& diffuse);
		void                    SetSpecularValue(const SmtColor& specular);
		void                    SetAmbientValue(const SmtColor& ambient);
		void                    SetEmissiveValue(const SmtColor& emissive);
		void                    SetShininessValue(float shininess);

		//get
		const SmtColor&			GetDiffuseValue(void); 
		const SmtColor&         GetSpecularValue(void);
		const SmtColor&         GetAmbientValue(void);
		const SmtColor&         GetEmissiveValue(void);
		float					GetShininessValue(void);

	private:
		SmtColor                m_cDiffuse;             // RGBA diffuse light value
		SmtColor                m_cAmbient;             // RGBA ambient light value
		SmtColor                m_cSpecular;            // RGBA specular light value
		SmtColor                m_cEmissive;            // RGBA emissive light value
		float					m_fShininess;           // shininess index
   };

	//////////////////////////////////////////////////////////////////////////
	class  SMT_EXPORT_CLASS SmtCombinedCamera
	{
	public:
		SmtCombinedCamera();
		virtual ~SmtCombinedCamera(void);

		inline	void			SetEye(Vector3& eye){ m_vEye = eye;}
		inline Vector3&         GetEye(){ return m_vEye;}

		inline	void			SetUp(Vector3& up){ m_vUp = up;}
		inline Vector3&         GetUp(){ return m_vUp;}

		inline	void			SetTarget(Vector3& target){ m_vTarget = target;}
		inline Vector3&         GetTarget() { return m_vTarget;}

		void                    SetCamera(Vector3 &eye,Vector3 &target,Vector3 &up);

	public:

		void                    RaiseViewDirection(float angle);
		void                    TurnViewDirection(float angle);

		void                    ShiftCamera(float step);
		void                    ForwardCamera(float step);
		void                    RiseCamera(float step);
		void                    LeanCamera(float angle);

		void                    MoveCamera(Vector3 &vec);
		void                    MoveCameraToPos(Vector3 &pos);

		//
		void					SetSphereCameraMove(long deltX,long deltY);

	private:
		Vector3                 m_vEye;
		Vector3                 m_vUp;
		Vector3                 m_vTarget;
	};
   //////////////////////////////////////////////////////////////////////////

	enum FrustumSide
	{
		FS_RIGHT	= 0,		// The RIGHT side of the frustum
		FS_LEFT		= 1,		// The LEFT	 side of the frustum
		FS_BOTTOM	= 2,		// The BOTTOM side of the frustum
		FS_TOP		= 3,		// The TOP side of the frustum
		FS_BACK		= 4,		// The BACK	side of the frustum
		FS_FRONT	= 5			// The FRONT side of the frustum
	}; 

	// Like above, instead of saying a number for the ABC and D of the plane, we
	// want to be more descriptive.
	enum PlaneData
	{
		P_A = 0,			// The X value of the plane's normal
		P_B = 1,			// The Y value of the plane's normal
		P_C = 2,			// The Z value of the plane's normal
		P_D = 3				// The distance the plane is from the origin
	};

	class SMT_EXPORT_CLASS SmtFrustum
	{
	public:
        SmtFrustum(void);
		virtual ~SmtFrustum(void);

		//get frustum matrix
		void                     GetFrustum(float frustum[6][4]);

		//set frustum matrix
		void                     SetFrustum(float frustum[6][4]);
        
		// This takes a 3D point and returns TRUE if it's inside of the frustum
		bool                     IsPointIn(float x, float y, float z);
			
		// This takes a 3D point and a radius and returns TRUE if the sphere is inside of the frustum
		bool                     IsSphereIn(float x, float y, float z, float radius);
			
		// This takes the center and half the length of the cube.
		bool                     IsCubeIn( float x, float y, float z, float size );
			
		bool                     IsCuboidIn( float x, float y, float z, SIZE size );

		bool                     IsBoxIn( float max_x, float max_y, float max_z, float min_x, float min_y, float min_z );
		bool                     IsBoxIn( Vector3 &Max, Vector3 &Min )
		{
			return IsBoxIn( Max.x, Max.y, Max.z, Min.x, Min.y, Min.z );
		}
			
	private:
		// This holds the A B C and D values for each side of our frustum.
		float					m_frustum[6][4];
	};
}

#if !defined(Export_Smt3DRenderer)
#if     defined( _DEBUG)
#          pragma comment(lib,"Smt3DRendererD.lib")
#       else
#          pragma comment(lib,"Smt3DRenderer.lib")
#	    endif
#endif

#endif //_RD3D_RDBASE_H