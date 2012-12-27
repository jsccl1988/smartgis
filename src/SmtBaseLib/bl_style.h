/*
File:    bl_style.h

Desc:    SmtStyle,SmtStyleTable

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_STYLE_H
#define _BL_STYLE_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "smt_env_struct.h"

namespace Smt_Base
{
	enum StType
	{
       ST_PenDesc = 0x0001,
	   ST_BrushDesc = 0x0002,
	   ST_AnnoDesc = 0x0004,
	   ST_SymbolDesc = 0x008
	};

	struct SmtPenDesc 
	{
		long    lPenColor;
		long    lPenStyle;
		float   fPenWidth;

		SmtPenDesc(void)
		{
			lPenColor = RGB(0,255,0);
			lPenStyle = PS_SOLID;
			fPenWidth = 0.002;
		}
	};

	struct SmtBrushDesc 
	{
		enum brushType
		{
			BT_Solid,
			BT_Hatch    
		}       brushTp;

		long    lBrushColor;
		long    lBrushStyle;

		SmtBrushDesc(void)
		{
			brushTp     = BT_Solid;
			lBrushColor = RGB(0,255,255);
			lBrushStyle = HS_FDIAGONAL;
		}
	};

	struct SmtAnnotationDesc 
	{
		float	fHeight;
		float	fWidth;
		long	lEscapement;
		long	lOrientation;
		long	lWeight;
		byte	lItalic;
		byte	lUnderline;
		byte	lStrikeOut;
		byte	lCharSet;
		byte	lOutPrecision;
		byte	lClipPrecision;
		byte	lQuality;
		byte	lPitchAndFamily;
		char	szFaceName[ 32 ];

		long	lAnnoClr;
		float	fAngle;
		float	fSpace;
		
        SmtAnnotationDesc(void)
		{
			fHeight = 1.6;
			fWidth  = 1.6;
			lEscapement = 0;
			lOrientation = 0;
			lWeight = 50;
			lItalic = FALSE;
			lUnderline = FALSE;
			lStrikeOut = 0;
			lCharSet = DEFAULT_CHARSET;
			lOutPrecision = OUT_TT_PRECIS;
			lClipPrecision = CLIP_CHARACTER_PRECIS;
			lQuality = DEFAULT_QUALITY;
			lPitchAndFamily=FIXED_PITCH;
			strcpy (szFaceName, "Times New Roman");

			lAnnoClr             = RGB(0,0,0);
			fAngle				 = 0;
			fSpace				 = 0.2;
		}
	};

	struct SmtSymbolDesc 
	{
		long    lSymbolID;
		float   fSymbolWidth;
		float   fSymbolHeight;

		SmtSymbolDesc(void)
		{
			lSymbolID = 0;
			fSymbolHeight = 0.4;
			fSymbolWidth = 0.4;
		}
	};

	class SMT_EXPORT_CLASS SmtStyle
	{
	public:
		SmtStyle(void);
		SmtStyle(const char			  *szName,
			  const SmtPenDesc        &penDesc,
		      const SmtBrushDesc      &brushDesc,
			  const SmtAnnotationDesc &annoDesc,
			  const SmtSymbolDesc     &symbolDesc);

		~SmtStyle();

	public:
		const char						*GetStyleName() const {return m_szName;}
		ulong							GetStyleType(void) const {return m_stType;}

		void							SetStyleType(ulong tp) {m_stType = tp;}
		void							SetStyleName(const char * szName){strcpy(m_szName,szName);}
		SmtStyle						*Clone(const char * szNewName) const ;

//////////////////////////////////////////////////////////////////////////
		inline void						SetPenDesc(const SmtPenDesc & penDesc) {m_stPenDesc = penDesc;}
		inline void						SetBrushDesc(const SmtBrushDesc &brushDesc) {m_stBrushDesc = brushDesc;}
		inline void						SetAnnoDesc(const SmtAnnotationDesc &annoDesc) {m_stAnnoDesc = annoDesc;}
		inline void						SetSymbolDesc(const SmtSymbolDesc &symbolDesc) {m_stSymbolDesc = symbolDesc;}

//////////////////////////////////////////////////////////////////////////
		inline  SmtPenDesc				&GetPenDesc(void) { return m_stPenDesc; }  
		inline  SmtBrushDesc			&GetBrushDesc(void)  { return m_stBrushDesc; }  
		inline  SmtAnnotationDesc		&GetAnnoDesc(void)  { return m_stAnnoDesc; }   
		inline  SmtSymbolDesc			&GetSymbolDesc(void) { return m_stSymbolDesc; }        

		inline  const SmtPenDesc		&GetPenDesc(void)  const { return m_stPenDesc; }  
		inline  const SmtBrushDesc		&GetBrushDesc(void)  const { return m_stBrushDesc; }  
		inline  const SmtAnnotationDesc	&GetAnnoDesc(void)  const { return m_stAnnoDesc; }   
		inline  const SmtSymbolDesc		&GetSymbolDesc(void)  const { return m_stSymbolDesc; }

	protected:
        char							m_szName[MAX_STYLENAME_LENGTH];
		ulong							m_stType; 

		SmtPenDesc						m_stPenDesc;
		SmtBrushDesc					m_stBrushDesc;
		SmtAnnotationDesc				m_stAnnoDesc;
		SmtSymbolDesc					m_stSymbolDesc;
	};

	typedef vector<SmtStyle*> StylePtrList;

	class SMT_EXPORT_CLASS SmtStyleTable
	{
	public:
		SmtStyleTable(void);
		~SmtStyleTable(void);

		inline int                   GetStyleCount(void) const{ return m_nStyleCount; }

		SmtStyleTable				*Clone(void) const;

		int                          AddStyle(const char *stylename);
		void                         RemoveStyle(const char *stylename);

		SmtStyle					*GetStyle(const char *stylename);
		const SmtStyle				*GetStyle(const char *stylename) const;

		SmtStyle					*GetStyle(int index);
		const SmtStyle				*GetStyle(int index) const;

		const char					*GetStyleName(int index);

	protected:
		int                          FindStyleNameIndex(const char * stylename) const;

	protected:
        char						**m_pStyleNames;
		int                          m_nStyleCount;
	};
}

#if !defined(Export_SmtBaseLib)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtBaseLibD.lib")
#       else
#          pragma comment(lib,"SmtBaseLib.lib")
#	    endif  
#endif

#endif //_BL_STYLE_H