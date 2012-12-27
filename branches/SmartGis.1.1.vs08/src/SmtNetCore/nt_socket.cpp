#include "nt_socket.h"

#pragma comment(lib, "wsock32.lib")

namespace Smt_NetWork
{
	//////////////////////////////////////////////////////////////////////////
	class WSAIniter
	{
	public:
		WSAIniter()
		{
			WSADATA wsaData;

			WSAStartup(MAKEWORD(2, 0), &wsaData);
		}

		~WSAIniter()
		{
			WSACleanup();
		}
	};

	static WSAIniter	wsaInit;

	//////////////////////////////////////////////////////////////////////////
	SOCKADDR_IN TransAddr(std::string const & address, u_int port)
	{
		SOCKADDR_IN sockAddr_in;
		std::memset(&sockAddr_in, 0, sizeof(sockAddr_in));

		if (address.empty())
			sockAddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
		else
			sockAddr_in.sin_addr.s_addr = inet_addr(address.c_str());

		if (INADDR_NONE == sockAddr_in.sin_addr.s_addr)
		{
			LPHOSTENT pHostEnt = gethostbyname(address.c_str());
			if (pHostEnt != NULL)
				std::memcpy(&sockAddr_in.sin_addr.s_addr,pHostEnt->h_addr_list[0], pHostEnt->h_length);
			else
				;
		}

		sockAddr_in.sin_family = AF_INET;
		sockAddr_in.sin_port = htons(port);

		return sockAddr_in;
	}

	std::string TransAddr(SOCKADDR_IN const & sockAddr, u_int& port)
	{
		port = ntohs(sockAddr.sin_port);
		return std::string(inet_ntoa(sockAddr.sin_addr));
	}

	// 获取主机地址
	/////////////////////////////////////////////////////////////////////////////////
	IN_ADDR GetHostAddr()
	{
		IN_ADDR addr;
		memset(&addr, 0, sizeof(addr));

		char host[256];
		if (0 == gethostname(host, sizeof(host)))
		{
			HOSTENT* pHostEnt = gethostbyname(host);
			std::memcpy(&addr.S_un.S_addr, pHostEnt->h_addr_list[0], pHostEnt->h_length);
		}

		return addr;
	}

	//////////////////////////////////////////////////////////////////////////
	// 构造函数
	/////////////////////////////////////////////////////////////////////////////////
	SmtSocket::SmtSocket(): m_socket(INVALID_SOCKET)
	{
	}

	// 析构函数
	/////////////////////////////////////////////////////////////////////////////////
	SmtSocket::~SmtSocket()
	{
		this->Close();
	}

	// 建立套接字
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::Create(int socketType, int protocolType, int addressFormat)
	{
		this->Close();

		this->m_socket = socket(addressFormat, socketType, protocolType);
		if (this->m_socket != INVALID_SOCKET)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	// 关闭套接字
	/////////////////////////////////////////////////////////////////////////////////
	void SmtSocket::Close()
	{
		if (this->m_socket != INVALID_SOCKET)
		{
			closesocket(this->m_socket);
			this->m_socket = INVALID_SOCKET;
		}
	}

	// 服务端应答
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::Accept(SmtSocket& connectedSocket, SOCKADDR_IN& sockAddr)
	{
		connectedSocket.Close();

		socklen_t len(sizeof(sockAddr));
		connectedSocket.m_socket = accept(this->m_socket,reinterpret_cast<SOCKADDR*>(&sockAddr), &len);

		if (connectedSocket.m_socket != INVALID_SOCKET)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	long SmtSocket::Accept(SmtSocket& connectedSocket)
	{
		connectedSocket.Close();
		connectedSocket.m_socket = accept(this->m_socket, NULL, NULL);

		if (connectedSocket.m_socket != INVALID_SOCKET)
			return SMT_ERR_NONE;
		else
			return SMT_ERR_FAILURE;
	}

	// 绑定端口
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::Bind(SOCKADDR_IN const & sockAddr)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == bind(this->m_socket, reinterpret_cast<SOCKADDR const *>(&sockAddr),sizeof(sockAddr)))
			return SMT_ERR_FAILURE;
		
		return SMT_ERR_NONE;
	}

	// 套接字IO控制
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::IOCtrl(long command, u_int* argument)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == ioctlsocket(this->m_socket, command, reinterpret_cast<u_long*>(argument)))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 服务端监听
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::Listen(int connectionBacklog)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == listen(this->m_socket, connectionBacklog))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 有连接的情况下发送数据
	/////////////////////////////////////////////////////////////////////////////////
	int SmtSocket::Send(void const * buf, int len, int flags)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		return send(this->m_socket, static_cast<char const *>(buf), len, flags);
	}

	// 有连接的情况下接收数据
	/////////////////////////////////////////////////////////////////////////////////
	int SmtSocket::Receive(void* buf, int len, int flags)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		return recv(this->m_socket, static_cast<char*>(buf), len, flags);
	}

	// 强制关闭
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::ShutDown(eShutDownMode eSDM)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == shutdown(this->m_socket, eSDM))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 有连接的情况下获取点名称
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::PeerName(SOCKADDR_IN& sockAddr, socklen_t& len)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == getpeername(this->m_socket,reinterpret_cast<SOCKADDR*>(&sockAddr), &len))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 获取套接字名称
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::SockName(SOCKADDR_IN& sockAddr, socklen_t& len)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == getsockname(this->m_socket,reinterpret_cast<SOCKADDR*>(&sockAddr), &len))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 设置套接字参数
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::SetSockOpt(int optionName, void const * optionValue, socklen_t optionLen, int level)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == setsockopt(this->m_socket, level, optionName,static_cast<char const *>(optionValue), optionLen))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 获取套接字参数
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::GetSockOpt(int optionName, void* optionValue, socklen_t& optionLen, int level)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == getsockopt(this->m_socket, level, optionName,static_cast<char*>(optionValue), &optionLen))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 无连接情况下接收数据
	/////////////////////////////////////////////////////////////////////////////////
	int SmtSocket::ReceiveFrom(void* buf, int len, SOCKADDR_IN& sockFrom, int flags)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		socklen_t fromLen(sizeof(sockFrom));
		return recvfrom(this->m_socket, static_cast<char*>(buf), len, flags,
			reinterpret_cast<SOCKADDR*>(&sockFrom), &fromLen);
	}

	// 无连接情况下发送数据
	/////////////////////////////////////////////////////////////////////////////////
	int SmtSocket::SendTo(void const * buf, int len, SOCKADDR_IN const & sockTo, int flags)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		return sendto(this->m_socket, static_cast<char const *>(buf), len, flags,
			reinterpret_cast<SOCKADDR const *>(&sockTo), sizeof(sockTo));
	}

	// 连接服务端
	/////////////////////////////////////////////////////////////////////////////////
	long SmtSocket::Connect(SOCKADDR_IN const & sockAddr)
	{
		if (this->m_socket == INVALID_SOCKET)
			return SMT_ERR_FAILURE;

		if (SOCKET_ERROR == connect(this->m_socket,reinterpret_cast<SOCKADDR const *>(&sockAddr), sizeof(sockAddr)))
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	// 设置超时时间
	/////////////////////////////////////////////////////////////////////////////////
	void SmtSocket::TimeOut(u_int MicroSecs)
	{
		timeval timeOut;

		timeOut.tv_sec = MicroSecs / 1000;
		timeOut.tv_usec = MicroSecs % 1000;

		SetSockOpt(SO_RCVTIMEO, &timeOut, sizeof(timeOut));
		SetSockOpt(SO_SNDTIMEO, &timeOut, sizeof(timeOut));
	}

	// 获取超时时间
	/////////////////////////////////////////////////////////////////////////////////
	u_int SmtSocket::TimeOut()
	{
		timeval timeOut;
		socklen_t len(sizeof(timeOut));

		this->GetSockOpt(SO_RCVTIMEO, &timeOut, len);

		return timeOut.tv_sec * 1000 + timeOut.tv_usec;
	}
}
