/*
File:    bl_envelope.h

Desc:    Envelope,MBR

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _BL_ENVELOPE_H
#define _BL_ENVELOPE_H

#include "smt_core.h"

namespace Smt_Base
{
	class SMT_EXPORT_CLASS Envelope
	{
	public:
		Envelope();
		virtual ~Envelope();

		bool			IsInit() const;

		void			Merge( Envelope const& sOther );
		void			Merge( double dfX, double dfY );

		void			Intersect( Envelope const& sOther );
		bool			Intersects(Envelope const& other) const;
		bool			Contains(Envelope const& other) const;
		bool			Contains(double dfX, double dfY) const;

		double			MinX;
		double			MaxX;
		double			MinY;
		double			MaxY;
	};
}

#if !defined(Export_SmtBaseLib)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtBaseLibD.lib")
#       else
#          pragma comment(lib,"SmtBaseLib.lib")
#	    endif  
#endif

#endif //_BL_ENVELOPE_H