/// \file waHttpClient.h
/// HTTP�ͻ�����ͷ�ļ�
/// ������ webapp::String, webapp::Encode
/// <a href="wa_httpclient.html">ʹ��˵���ĵ����򵥷���</a>

#ifndef _WEBAPPLIB_HTTPCLIENT_H_
#define _WEBAPPLIB_HTTPCLIENT_H_ 

#include <string>
#include <vector>
#include <map>
#include "waString.h"

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
const string HTTP_CRLF = "\r\n";
const string DOUBLE_CRLF = "\r\n\r\n";
	
/// ����TCP����ȡ�û�Ӧ����
int		SMT_EXPORT_API tcp_request( const string &server, const int port, const string &request, 
	string &response, const int timeout );
/// ���ݷ���������ȡ��IP
string  SMT_EXPORT_API gethost_byname( const string &domain );
/// �ж��ַ����Ƿ�Ϊ��ЧIP
bool	SMT_EXPORT_API isip( const string &ipstr );

/// HTTP�ͻ�����
/// <a href="wa_httpclient.html">ʹ��˵���ĵ����򵥷���</a>
class SMT_EXPORT_CLASS HttpClient {
	public:
	
	/// \enum ������Ϣ
	enum error_msg {
		/// �޴���
		ERROR_NULL					= 0,
		/// ����socketʧ��
		ERROR_CREATE_SOCKET			= 1,
		/// �޷����ӷ�����
		ERROR_CONNECT_SERVER		= 2,
		/// ��������ʧ��
		ERROR_SEND_REQUEST			= 3,
		/// ���ö�ʱ��ʧ�ܻ������ӳ�ʱ
		ERROR_RESPONSE_TIMEDOUT		= 4,
		/// �����ڵ�ַ��Ϣ����
		ERROR_SERVERINFO_NULL		= 5,
		/// HTTP�����ʽ����
		ERROR_REQUEST_NULL			= 6,
		/// ��������ӦΪ��
		ERROR_RESPONSE_NULL			= 7,
		/// ��������Ӧ��ʽ����
		ERROR_RESPONSE_INVALID		= 8,
		/// ��������ӦHTTP״̬����
		ERROR_HTTPSTATUS			= 9,
		/// δ֪����
		ERROR_UNKNOWN				= 10
	};

	/// Ĭ�Ϲ��캯��
	HttpClient(){};
	
	/// ���첢ִ��HTTP����
	/// \param url HTTP����URL
	/// \param server ������IP,Ϊ���ַ�������ݲ���1���,Ĭ��Ϊ���ַ���
	/// \param port �������˿�,Ĭ��Ϊ80
	/// \param method HTTP����Method,Ĭ��Ϊ"GET"
	/// \param timeout HTTP����ʱʱ��,��λΪ��,Ĭ��Ϊ5��,Ϊ0���жϳ�ʱ
	HttpClient( const string &url, const string &server = "", const int port = 80, 
		const string &method = "GET", const int timeout = 5 ) 
	{
		this->request( url, server, port, method, timeout );
	}
		  
	/// ��������
	virtual ~HttpClient(){};

	/// ����ָ����HTTP����Header
	void set_header( const string &name, const string &value );
	/// ����HTTP����Referer Header
	void set_referer( const string &referer );
	/// ����HTTP����Authorization Header
	void set_auth( const string &username, const string &password );
	/// ����HTTP����Cookie Header
	void set_cookie( const string &name, const string &value );
	/// ����HTTP����CGI����
	void set_param( const string &name, const string &value );

	/// ִ��HTTP����
	bool request( const string &url, const string &server = "", const int port = 80, 
		const string &method = "GET", const int timeout = 5 );
	/// URL �Ƿ���Ч
	bool exist( const string &url, const string &server = "", const int port = 80 );

	/// ��ȡָ����HTTP����Header
	string get_header( const string &name );
	/// ��ȡHTTP����Set-Cookie Header
	vector<String> get_cookie();
	/// ��ȡHTTP����Header
	string dump_header();
	
	/// ִ��HTTP�����Ƿ�ɹ�
	bool done() const;
	/// ����������ü�״ֵ̬
	void clear();
	
	/// ��ȡHTTP����Status
	/// \return HTTP����Status�ַ���
	inline string status() const {
		return _status;
	}
	/// ��ȡHTTP����Content����
	/// \return HTTP����Content����
	inline string content() const {
		return _content;
	}
	/// ��ȡHTTP����Content���ĳ���(Content-Length)
	/// \return HTTP����Content���ĳ���
	inline unsigned int content_length() const {
		return _content.length();
	}
	
	/// ���ش�����Ϣ����
	/// ������Ϣ����μ� Http::error_msg
	inline error_msg errnum() const {
		return _errno;
	}
	/// ���ش�����Ϣ����
	string error() const;

	/// ������ɵ�HTTP����ȫ��
	/// \return �������ɵ�HTTP����ȫ��
	inline string dump_request() const {
		return _request;
	}
	/// �����õķ���������ȫ��
	/// \return ���ػ�õķ���������ȫ��
	inline string dump_response() const {
		return _response;
	}
	
	////////////////////////////////////////////////////////////////////////////
	private:

	/// ����HTTP URL�ַ���
	void parse_url( const string &url, string &parsed_host, string &parsed_addr,
		string &parsed_url, string &parsed_param, int &parsed_port );
	/// ����HTTP�����ַ���
	string gen_httpreq( const string &url, const string &params,
		const string &host, const string &method );
	/// ����HTTP����
	void parse_response( const string &response );
	/// ����HTTP����chunked����content����
	string parse_chunked( const string &chunkedstr );
	
	// set		
	String _request;			// generated request
	String _params;				// http request params
	map<string,string> _sets;	// push http headers

	// get
	String _response;			// server response
	String _status;				// http response status
	String _content;			// http response content
	map<string,string> _gets;	// recv http headers
	
	error_msg _errno;			// current error code
};

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_HTTPCLIENT_H_ 

