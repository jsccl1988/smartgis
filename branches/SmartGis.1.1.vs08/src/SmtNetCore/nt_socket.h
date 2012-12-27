#ifndef _NT_SOCKET_H
#define _NT_SOCKET_H

#include "smt_core.h"

#ifndef _WINSOCKAPI_
#include <winsock.h>
#endif
typedef int socklen_t;

namespace Smt_NetWork
{
	SOCKADDR_IN SMT_EXPORT_API TransAddr(std::string const & address, u_int port);
	std::string SMT_EXPORT_API TransAddr(SOCKADDR_IN const & sockAddr, u_int& port);
	IN_ADDR		SMT_EXPORT_API GetHostAddr();

	// Í¬²½Ì×½Ó×Ö
	///////////////////////////////////////////////////////////////////////////////
	class SMT_EXPORT_CLASS SmtSocket
	{
	public:
		enum eShutDownMode
		{
			SDM_Receives = 0,
			SDM_Sends = 1,
			SDM_Both = 2,
		};

	public:
		SmtSocket();
		~SmtSocket();

		long					Create(int socketType = SOCK_STREAM, 
									   int protocolType = IPPROTO_IP, int addressFormat = PF_INET);
		void					Close();

		long					Accept(SmtSocket& connectedSocket);
		long					Accept(SmtSocket& connectedSocket, SOCKADDR_IN& sockAddr);
		long					Bind(SOCKADDR_IN const & sockAddr);

		long					Connect(SOCKADDR_IN const & sockAddr);

		long					IOCtrl(long command, u_int* argument);
		long					Listen(int connectionBacklog = 5);

		int						Receive(void* buf, int len, int flags = 0);
		int						Send(void const * buf, int len, int flags = 0);

		int						ReceiveFrom(void* buf, int len, SOCKADDR_IN& sockFrom, int flags = 0);
		int						SendTo(void const * buf, int len, SOCKADDR_IN const & sockTo, int flags = 0);

		
		long					ShutDown(eShutDownMode eSDM = SDM_Sends);

		long					PeerName(SOCKADDR_IN& sockAddr, socklen_t& len);
		long					SockName(SOCKADDR_IN& sockAddr, socklen_t& len);

		long					SetSockOpt(int optionName, void const * optionValue,
											socklen_t optionLen, int level = SOL_SOCKET);
		long					GetSockOpt(int optionName, void* optionValue,
											socklen_t& optionLen, int level = SOL_SOCKET);

		void					NonBlock(bool nonBlock){u_int on(nonBlock);this->IOCtrl(FIONBIO, &on);}

		void					TimeOut(u_int microSecs);
		u_int					TimeOut();

	private:
		SOCKET					m_socket;
	};
}

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_NT_SOCKET_H