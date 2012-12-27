#ifndef _MSVR_CGIWRAPPER_H
#define _MSVR_CGIWRAPPER_H

#include <map>

#include "nt_socket.h"
#include "smt_bas_struct.h"
#include "msvr_core.h"

using namespace Smt_Core;
using namespace Smt_NetWork;

#define			MSVR_CGIWRAPPER_LOG_NAME		"MSVR_CGIWRAPPER"

namespace Smt_CgiWrapper
{
	class SmtCgiWrapper
	{
	public:
		SmtCgiWrapper();
		~SmtCgiWrapper();

	public:
		long					Init(uint unSvrPort = 9001);
		long					Create();
		long					Destroy();

		long					Process(const std::map<string,string> &mapKey2Value);

	protected:
		long					SendRequest(const std::map<string,string> &mapKey2Value);
		long					RecvResponse();

	protected:
		SmtSocket				m_sClient;
		uint					m_unSvrPort;
		SOCKADDR_IN				m_svrAdr;
	};
}

#endif //_MSVR_CGIWRAPPER_H