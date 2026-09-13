/*
File:    smt_timer.h

Desc:    SmartGis ,�߾���ʱ��

Version: Version 1.0

Writter:  �´���

Date:    2011.10.10

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_TIMER_H
#define _SMT_TIMER_H

#include "base/core/core.h"

namespace base
{
	class   CORE_EXPORT SmtTimer
	{
	public:
		SmtTimer();
		virtual~SmtTimer(void);

	public:
		void					update(void);

		void					set_clock(unsigned char nHH,unsigned char nMM);
		const char				*get_clock() const; 

		void					set_scale(float fFaktor) { m_fFaktor = fFaktor; }
		float					get_scale(void)  const   { return m_fFaktor; }
		
		float					get_time_stamp(void) const  { return m_fStamp; }
		float					get_elapsed(void) const { return (m_fTime_elapsed*m_fFaktor); }
		float					get_fps(void) const   { return (1./m_fTime_elapsed); }
		
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

#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_TIMER_H