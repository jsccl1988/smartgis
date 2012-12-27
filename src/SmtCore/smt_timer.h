/*
File:    smt_timer.h

Desc:    SmartGis ,高精度时钟

Version: Version 1.0

Writter:  陈春亮

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_TIMER_H
#define _SMT_TIMER_H

#include "smt_core.h"

namespace Smt_Core
{
	class   SMT_EXPORT_CLASS SmtTimer
	{
	public:
		SmtTimer();
		virtual~SmtTimer(void);

	public:
		void					Update(void);

		void					SetClock(unsigned char nHH,unsigned char nMM);
		const char				*GetClock() const; 

		void					SetScale(float fFaktor) { m_fFaktor = fFaktor; }
		float					GetScale(void)  const   { return m_fFaktor; }
		
		float					GetTimeStamp(void) const  { return m_fStamp; }
		float					GetElapsed(void) const { return (m_fTime_elapsed*m_fFaktor); }
		float					GetFPS(void) const   { return (1./m_fTime_elapsed); }
		
	protected:
		LONGLONG				m_nCur_time;         // current time
		LONGLONG				m_nPerf_cnt;         // performance timer frequency
		bool					m_blnPerf_flag;      // flag for timer to use
		LONGLONG				m_nLast_time;	     // time of previous frame
		float					m_fTime_elapsed;     // time elapsed since previous frame
		float					m_fTime_scale;       // scaling factor for time
		UCHAR					m_nHH;               // clock time hours
		UCHAR					m_nMM;               // clock time minutes
		UCHAR					m_nSS;               // clock time seconds
		float					m_fClock;            // sum up milliseconds
		float					m_fFaktor;           // slowmo or speedup
		float					m_fStamp;            // unique timestamp

		char					m_szTime[20];
	};
}

#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_TIMER_H