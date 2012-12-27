/// \file waHttpClient.cpp
/// HTTP�ͻ�����ʵ���ļ�

#include <cstring>
#include <winsock.h>
#include "waEncode.h"
#include "waHttpClient.h"

#pragma comment(lib, "wsock32.lib")

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// \defgroup waHttpClient waHttpClient���ȫ�ֺ���

/// \ingroup waHttpClient
/// \fn int tcp_request( const string &server, const int port, const string &request, string &response, const int timeout )
/// ����TCP����ȡ�û�Ӧ����
/// \param server ������IP
/// \param port �������˿�
/// \param request ���͵�TCP����
/// \param response �������Ļ�Ӧ����
/// \param timeout ��ʱʱ��,��λΪ��,Ϊ0���жϳ�ʱ
/// \retval 0 ִ�гɹ�
/// \retval 1 ����socketʧ��
/// \retval 2 �޷����ӷ�����
/// \retval 3 ��������ʧ��
/// \retval 4 ���ö�ʱ��ʧ�ܻ������ӳ�ʱ
/// \retval 10 δ֪����
int tcp_request( const string &server, const int port, const string &request,
	string &response, const int timeout ) 
{
	// init
	struct sockaddr_in sin;	 
	sin.sin_family = AF_INET;
	sin.sin_port = htons( port );
	sin.sin_addr.s_addr = inet_addr( server.c_str() );
	
	// create socket
	int fd;
	if ( (fd=socket(AF_INET,SOCK_STREAM,0)) < 0 )
		return 1;

	// connect
	if ( connect(fd,(struct sockaddr*)&sin,sizeof(sin)) < 0 ) {
		closesocket( fd );
		return 2;
	}

	// send request
	if ( send(fd,request.c_str(),request.length(),0) < 0 ) {
		closesocket( fd );
		return 3;
	}

	fd_set fds;
	struct timeval tv;

	// set timer
	if ( timeout > 0 ) {
		FD_ZERO( &fds );
		FD_SET( fd, &fds );
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		if ( select(fd+1,&fds,NULL,NULL,&tv) <= 0 ) {
			closesocket( fd );
			return 4;
		}
	}

	if ( ( timeout>0 && FD_ISSET(fd,&fds) ) || timeout<=0 ) {
		// recv response
		int readed = 0;
		int buflen = 1024;
		char buff[1024];
		
		while ( (readed=recv(fd,buff,buflen-1,0)) > 0 ) {
			buff[readed] = '\0';
			response += buff;
		}

		closesocket( fd );
		return 0;
	} else {
		closesocket( fd );
		return 10;
	}
}

/// \ingroup waHttpClient
/// \fn string gethost_byname( const string &domain )
/// ���ݷ���������ȡ��IP
/// \param domain ������������������"HTTP:://"ͷ���κ�'/'�ַ���
/// \return ִ�гɹ����ط�����IP,���򷵻ؿ��ַ���
string gethost_byname( const string &domain ) {
	string ip;
	if ( domain != "" ) {
		struct hostent *he = gethostbyname( domain.c_str() );
		if ( he!=NULL && he->h_addr!=NULL )
			ip = inet_ntoa( *((struct in_addr*)he->h_addr) );
	}

	return ip;
}

/// \ingroup waHttpClient
/// \fn bool isip( const string &ipstr )
/// �ж��ַ����Ƿ�Ϊ��ЧIP
/// \param ipstr IP�ַ���
/// \retval true ��Ч
/// \retval false ��Ч
bool isip( const string &ipstr ) {
	
	/*struct in_addr addr;
	if ( inet_aton(ipstr.c_str(),&addr) != 0 )
		return true;
	else
		return false;*/

	ULONG ret = INADDR_NONE;
	struct hostent *hostname = NULL;

	ret = inet_addr(ipstr.c_str());
	if(ret == INADDR_NONE)
	{
		return false;
	}
	return true;
}

/// ����ָ����HTTP����Header
/// \param name Header����
/// \param value Headerֵ,
void HttpClient::set_header( const string &name, const string &value ) {
	if ( name != "" )
		_sets[name] = value;
}

/// ����HTTP����Referer Header
/// \param referer Referer Headerֵ
void HttpClient::set_referer( const string &referer ) {
	if ( referer != "" )
		set_header( "Referer", referer );
}

/// ����HTTP����Authorization Header
/// \param username �û���
/// \param password �û�����
void HttpClient::set_auth( const string &username, const string &password ) {
	if ( username != "" ) {
		string auth = username + ":" + password;
		auth = "Basic " + base64_encode( auth );
		set_header( "Authorization", auth );
	}
}

/// ����HTTP����Cookie Header
/// \param name Cookie����
/// \param value Cookieֵ
void HttpClient::set_cookie( const string &name, const string &value ) {
	if ( name != "" ) {
		if ( _sets["Cookie"] != "" )
			_sets["Cookie"] += "; ";
		_sets["Cookie"] += ( uri_encode(name) + "=" + uri_encode(value) );
	}
}

/// ����HTTP����CGI����
/// \param name CGI��������
/// \param value CGI����ֵ
void HttpClient::set_param( const string &name, const string &value ) {
	if ( name != "" ) {
		if ( _params != "" ) _params += "&";
		_params += ( uri_encode(name) + "=" + uri_encode(value) );
	}
}

/// ����HTTP URL�ַ���
/// \param urlstr ����URL
/// \param parsed_host �������������������
/// \param parsed_host ������������ַ�������
/// \param parsed_url ����URL�������
/// \param parsed_param ��������������
/// \param parsed_port �������˿ڷ������
void HttpClient::parse_url( const string &urlstr, string &parsed_host, string &parsed_addr,
	string &parsed_url, string &parsed_param, int &parsed_port )
{
	String url = urlstr;
	url.trim();
	if ( url == "" ) {
		_errno = ERROR_REQUEST_NULL;
		return;
	}

	// parse hostname and url
	unsigned int pos;
	parsed_host = "";
	parsed_url = url;
	if ( strnicmp(url.c_str(),"HTTP://",7) == 0 ) {
		// http://...
		if ( (pos=url.find("/",7)) != url.npos ) {
			// http://hostname/...
			parsed_host = url.substr( 7, pos-7 );
			parsed_url = url.substr( pos );
		} else {
			// http://hostname
			parsed_host = url.substr( 7 );
			parsed_url = "/";
		}
	}

	// parse param
	parsed_param = "";
	if ( (pos=parsed_url.rfind("?")) != parsed_url.npos ) {
		// cgi?param
		parsed_param = parsed_url.substr( pos+1 );
		parsed_url = parsed_url.substr( 0, pos );
	}

	// parse port
	parsed_port = 80;
	if ( (pos=parsed_host.rfind(":")) != parsed_host.npos ) {
		// hostname:post
		parsed_port = stoi( parsed_host.substr(pos+1) );
		parsed_host = parsed_host.substr( 0, pos );
	}
	
	// parse addr
	if ( !isip(parsed_host) )
		parsed_addr = gethost_byname( parsed_host );
	else
		parsed_addr = parsed_host;
}
				   
/// ����HTTP�����ַ���
/// \param url �������URL
/// \param params �������URL��CGI����
/// \param host ����������(����IP)
/// \param method ���󷽷�(GET����POST)
/// \return �������ɵ�HTTP�����ַ���
string HttpClient::gen_httpreq( const string &url, const string &params, 
	const string &host, const string &method ) 
{
	string request;
	request.reserve( 512 );
	
	request += method + " " + url;
	if ( method!="POST" && params!="" )
		request += "?" + params;
	request += " HTTP/1.1" + HTTP_CRLF;
	
	request += "HOST: " + host + HTTP_CRLF;
	request += "Accept: */*" + HTTP_CRLF;
	request += "User-Agent: Mozilla/4.0 (compatible; WebAppLib HttpClient)" + HTTP_CRLF;
	request += "Pragma: no-cache" + HTTP_CRLF;
	request += "Cache-Control: no-cache" + HTTP_CRLF;
	
	map<string,string>::const_iterator i;
	for ( i=_sets.begin(); i!=_sets.end(); ++i ) {
		if ( i->first != "" )
			request += i->first + ": " + i->second + HTTP_CRLF;
	}

	request += "Connection: close" + HTTP_CRLF;

	if ( method == "POST" ) {
		// post data
		request += "Content-Type: application/x-www-form-urlencoded" + HTTP_CRLF;
		request += "Content-Length: " + itos(params.length()) + HTTP_CRLF;
		request += HTTP_CRLF;
		request += params + HTTP_CRLF;
	}

	request += HTTP_CRLF;
	return request;
}

/// ִ��HTTP����
/// \param url HTTP����URL
/// \param server ������IP��������,Ϊ���ַ�������ݲ���1���,Ĭ��Ϊ���ַ���,
/// ������url,server����������������ַ��Ϣ,��������ʧ��
/// \param port �������˿�,Ĭ��Ϊ80
/// \param method HTTP����Method,Ĭ��Ϊ"GET"
/// \param timeout HTTP����ʱʱ��,��λΪ��,Ĭ��Ϊ5��
/// \retval true ִ�гɹ�
/// \retval false ִ��ʧ��
bool HttpClient::request( const string &url, const string &host, const int port, 
	const string &method, const int timeout )
{
	_errno = ERROR_NULL;
	
	// parse host,port,url info
	string parsed_host, parsed_addr, parsed_url, parsed_param;
	int parsed_port;
	this->parse_url( url, parsed_host, parsed_addr, parsed_url, 
			   		 parsed_param, parsed_port );		
	
	// check params
	if ( parsed_param != "" ) {
		if ( _params != "" ) _params += "&";
		_params += parsed_param;
	}

	// check port
	if ( port != 80 ) parsed_port = port;
	
	// check host
	if ( host != "" ) {
		if ( !isip(parsed_host) ) {
			parsed_host = host;
			parsed_addr = gethost_byname( parsed_host );
		} else {
			parsed_addr = host;
		}
	}
	if ( parsed_addr == "" ) {
		_errno = ERROR_SERVERINFO_NULL;
		return false;
	}
	
	// generate request string
	_request = this->gen_httpreq( parsed_url, _params, parsed_host, method );
	// request
	int reqres = tcp_request( parsed_addr, parsed_port, _request, _response, timeout );
	if ( reqres != 0 ) {
		_errno = static_cast<error_msg>( reqres );
		return false;
	}
	
	// parse response
	if ( _response == "" ) {
		_errno = ERROR_RESPONSE_NULL;
		return false;
	}

	this->parse_response( _response );
	return true;
}

/// URL �Ƿ���Ч
/// \param url HTTP����URL
/// \param server ������IP,Ϊ���ַ�������ݲ���1���,Ĭ��Ϊ���ַ���,
/// ������url,server����������������ַ��Ϣ,��������ʧ��
/// \param port �������˿�,Ĭ��Ϊ80
/// \retval true URL��Ч
/// \retval false URL��ʧЧ
bool HttpClient::exist( const string &url, const string &server, 
	const int port ) 
{
	bool res = false;

	// check
	if ( this->request(url,server,port,"HEAD",5) && this->done() )
		res = true;
		
	// clear
	_status = "";
	_content = "";
	_gets.clear();
	
	return res;
}

/// ����HTTP����
/// \param response HTTP�����ַ���
void HttpClient::parse_response( const string &response ) {
	// clear response status
	_status = "";
	_content = "";
	_gets.clear();

	// split header and body
	unsigned int pos;
	String head;
	String body;
	if ( (pos=response.find(DOUBLE_CRLF)) != response.npos ) {
		head = response.substr( 0, pos );
		body = response.substr( pos+4 );
	} else if ( (pos=response.find("\n\n")) != response.npos ) {
		head = response.substr( 0, pos );
		body = response.substr( pos+2 );
	} else {
		_errno = ERROR_RESPONSE_INVALID;
		return;
	}	
				
	// parse status
	String status;
	if ( (pos=head.find(HTTP_CRLF)) != head.npos ) {
		status = head.substr( 0, pos );
		head = head.substr( pos+2 );
		
		// HTTP/1.1 status_number description_string
		status.trim();
		_gets["HTTP_STATUS"] = status;
		if ( strncmp(status.c_str(),"HTTP/",5) == 0 ) {
			unsigned int b1, b2;
			if ( (b1=status.find(" "))!=status.npos
				 && (b2=status.find(" ",b1+1))!=status.npos )
				_status = status.substr( b1+1, b2-b1-1 );
		}
	}

	// http response status
	if ( _status.length()>1 && _status[0]!='2' )
		_errno = ERROR_HTTPSTATUS;
	
	// parse header
	String line, name, value;
	vector<String> hds = head.split( "\n" );
	for ( unsigned int i=0; i<hds.size(); ++i ) {
		line = hds[i];
		line.trim();
		
		// name: value
		if ( (pos=line.find(":")) != line.npos ) {
			name = line.substr( 0, pos );
			name.trim();
			value = line.substr( pos+1 );
			value.trim();
			
			if ( name != "" ) {
				if ( _gets[name] != "" )
					_gets[name] += "\n";
				_gets[name] += value;
			}
		}
	}
	
	// parse body
	if ( this->get_header("Transfer-Encoding") == "chunked" )
		_content = this->parse_chunked( body );
	else
		_content = body;
}

/// ��ȡָ����HTTP����Header
/// \param name Header����,
/// \return �ɹ�����Headerֵ,���򷵻ؿ��ַ���
string HttpClient::get_header( const string &name ) {
	if ( name != "" )
		return _gets[name];
	else
		return string( "" );		
}

/// ��ȡHTTP����Set-Cookie Header
/// \return ����Cookie�б�����,ÿ��Ԫ��Ϊһ��Cookieֵ
vector<String> HttpClient::get_cookie() {
	String ck = this->get_header( "Set-Cookie" );
	vector<String> cks = ck.split( "\n" );
	return cks;
}

/// ��ȡHTTP����Header
/// \return HTTP����Header�ַ���
string HttpClient::dump_header() {
	// status
	string header = ( this->get_header("HTTP_STATUS") + "\n" );
	
	// for multi header
	String headers;
	vector<String> headerlist;
	
	map<string,string>::const_iterator i;
	for ( i=_gets.begin(); i!=_gets.end(); ++i ) {
		if ( i->first!="" && i->first!="HTTP_STATUS" ) {
			if ( (i->second).find("\n") != (i->second).npos ) {
				headers = i->second;
				headerlist = headers.split( "\n" );
				for ( unsigned j=0; j<headerlist.size(); ++j )
					header += i->first + ": " + headerlist[j] + "\n";
			} else {
				header += i->first + ": " + i->second + "\n";
			}
		}
	}
	
	return header;
}

/// ִ��HTTP�����Ƿ�ɹ�
/// \retval true �ɹ�
/// \retval false ʧ��
bool HttpClient::done() const {
	if ( _status.isnum() ) {
		int ret = stoi( _status );
		if ( ret>=100 && ret<300 )
			return true;
	}
	return false;
}

/// ����������ü�״ֵ̬
void HttpClient::clear() {
	// set
	_params = "";
	_sets.clear();
	
	// get
	_status = "";
	_content = "";
	_gets.clear();
}

/// ����HTTP����chunked����content����
/// \param chunkedstr chunked�����ַ���
/// \return ���ؽ����ַ���
string HttpClient::parse_chunked( const string &chunkedstr ) {
	char crlf[3] = "\x0D\x0A";
	unsigned int pos, lastpos;
	int size = 0;
	string hexstr;

	// location HTTP_CRLF		
	if ( (pos=chunkedstr.find(crlf)) != chunkedstr.npos ) {
		hexstr = chunkedstr.substr( 0, pos );
		size = stoi( hexstr, ios::hex );
	}
	
	string res;
	res.reserve( chunkedstr.length() );
	
	while ( size > 0 ) {
		// append to content
		res += chunkedstr.substr( pos+2, size );
		lastpos = pos+size+4;
		
		// location next HTTP_CRLF
		if ( (pos=chunkedstr.find(crlf,lastpos)) != chunkedstr.npos ) {
			hexstr = chunkedstr.substr( lastpos, pos-lastpos );
			size = stoi( hexstr, ios::hex );
		} else {
			break;
		}
	}
			
	return res;
}

/// ���ش�����Ϣ����
/// \return ���ش�����Ϣ����
string HttpClient::error() const {
	switch ( _errno ) {
		case ERROR_NULL :
			return "ERROR_NULL";
		case ERROR_CREATE_SOCKET :
			return "ERROR_CREATE_SOCKET";
		case ERROR_CONNECT_SERVER :
			return "ERROR_CONNECT_SERVER";
		case ERROR_SEND_REQUEST :
			return "ERROR_SEND_REQUEST";
		case ERROR_RESPONSE_TIMEDOUT :
			return "ERROR_RESPONSE_TIMEDOUT";
		case ERROR_SERVERINFO_NULL :
			return "ERROR_SERVERINFO_NULL";
		case ERROR_REQUEST_NULL :
			return "ERROR_REQUEST_NULL";
		case ERROR_RESPONSE_NULL :
			return "ERROR_RESPONSE_NULL";
		case ERROR_RESPONSE_INVALID :
			return "ERROR_RESPONSE_INVALID";
		case ERROR_HTTPSTATUS :
			return "ERROR_HTTPSTATUS:" + status();
		default : 
			return "ERROR_UNKNOWN";
	}
}

} // namespace

