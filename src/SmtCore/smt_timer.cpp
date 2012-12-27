#include "smt_timer.h"

#include <mmsystem.h>

#pragma comment(lib,"winmm.lib")

namespace Smt_Core
{
	SmtTimer::SmtTimer()
	{
		m_fClock        = 0.0f;
		m_nCur_time     = 0;
		m_nLast_time    = 0;
		m_fTime_elapsed = 1.0f;
		m_fFaktor       = 1.0f;
		memset(m_szTime,'\0',20);
		
		// either QueryPerformanceCounter or timeGetTime 
		if (QueryPerformanceFrequency((LARGE_INTEGER *) &m_nPerf_cnt)) 
		{ 
			QueryPerformanceCounter((LARGE_INTEGER *) &m_nLast_time); 
			m_blnPerf_flag = true;
			m_fTime_scale  = 1.0f / m_nPerf_cnt;
		}
		else 
		{ 
			m_blnPerf_flag = false;
			m_nLast_time   = timeGetTime(); 
			m_fTime_scale  = 0.001f;
	   } 
	}

	SmtTimer::~SmtTimer()
	{
       ;
	}

	/**
	 * Counts the actual time and adjusts clock.
	 */
	void SmtTimer::Update() 
	{
	   if (m_blnPerf_flag) 
		  QueryPerformanceCounter((LARGE_INTEGER *) &m_nCur_time); 
	   else 
		  m_nCur_time = timeGetTime(); 
   
	   m_fTime_elapsed = (m_nCur_time - m_nLast_time)* m_fTime_scale;

	   m_nLast_time = m_nCur_time;
		
	   // adjust clock by seconds passed
	   m_fClock += (m_fTime_elapsed * m_fFaktor);
	   if (m_fClock >= 1.0f) {
		  m_nSS++;
		  m_fClock = 0.0f;
		  }
	   if (m_nSS >= 60) {
		  m_nMM++;
		  m_nSS = 0;
		  }
	   if (m_nMM >= 60) {
		  m_nHH++;
		  m_nMM = 0;
		  }
	   if (m_nHH >= 24)
		  m_nHH = 0;

	   // calculate timestamp from clock and 2 random digits
	   m_fStamp = 0.0f;
	   m_fStamp += m_fClock;
	   m_fStamp += m_nSS;
	   m_fStamp += m_nMM  * 100;     // move left 2 decimal positions 
	   m_fStamp += m_nHH  * 10000;   // move left 4 decimal positions

	   sprintf_s(m_szTime,20 ,"%.2d:%.2d:%.2d", m_nHH, m_nMM, m_nSS);
	} // Update


	/**
	 * Sets the clock to any given starting time.
	 */
	void SmtTimer::SetClock(unsigned char nHH, unsigned char nMM) 
	{
	   // set clock
	   if (nHH > 23)
		  nHH = 0;
	   if (nMM > 59)
		  nMM = 0;

	   m_nHH = nHH;
	   m_nMM = nMM;
	} // SetClock


	/**
	 * Gives you a time string with hours, minutes and seconds as return
	 * and hours and/or minutes as reference parameters.
	 */

	const char * SmtTimer::GetClock() const
	{
		return m_szTime;
	} 
	// GetClock

}