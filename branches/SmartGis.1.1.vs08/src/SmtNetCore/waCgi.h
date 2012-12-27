/// \file waCgi.h
/// webapp::Cgi,webapp::Cookie��ͷ�ļ�
/// ������ webapp::String, webapp::Encode

#ifndef _WEBAPPLIB_CGI_H_
#define _WEBAPPLIB_CGI_H_ 

#include <string>
#include <map>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {

////////////////////////////////////////////////////////////////////////////////	
/// ���HTML Content-Type header
void	SMT_EXPORT_API http_head();
/// ȡ�û�������
string	SMT_EXPORT_API get_env( const string &envname );

////////////////////////////////////////////////////////////////////////////////

/// \defgroup waCgi waCgi�������������ȫ�ֺ���

/// \ingroup waCgi
/// \typedef CgiList 
/// Cgi ����ֵ�б����� (map<string,string>)
typedef map<string,string> CgiList;

/// CGI������ȡ��
class SMT_EXPORT_CLASS Cgi {
	public:

	/// ���캯��
	Cgi( const long formdata_maxsize = 0 );
	
	/// ��������
	virtual ~Cgi(){};
	
	/// ȡ��CGI����
	string get_cgi( const string &name )const;
	
	/// ȡ��CGI����
	inline string operator[] ( const string &name ) const{
		return this->get_cgi( name );
	}
	
	/// FORM���ݴ�С�Ƿ񳬳�����
	inline bool is_trunc() const {
		return _trunc;
	}
	
	/// ���ز���ֵ�б�
	/// \return ����ֵ����ΪCgiList,��map<string,string>.	
	inline CgiList dump() const {
		return _cgi;
	}
	
	////////////////////////////////////////////////////////////////////////////
	private:
	
	/// ����CGI����
	void add_cgi( const string &name, const string &value );
	
	/// ����urlencoded��������
	void parse_urlencoded( const string &buf );
	
	/// ����multipart��������
	void parse_multipart( const string &content_type, const string &buf );

	map<string,string> _cgi;
	String _method;
	bool _trunc;
};

////////////////////////////////////////////////////////////////////////////
/// \ingroup waCgi
/// \typedef CookieList 
/// Cookie ����ֵ�б����� (map<string,string>)
typedef map<string,string> CookieList;

/// Cookie��ȡ,������
class SMT_EXPORT_CLASS Cookie {
	public:
	
	/// ���캯��
	Cookie();
	
	/// ��������
	virtual ~Cookie(){};
	
	/// ȡ��cookie����
	string get_cookie( const string &name );
	
	/// ȡ��cookie����
	inline string operator[] ( const string &name ) {
		return this->get_cookie( name );
	}
	
	/// ����cookie����
	void set_cookie( const string &name, const string &value, 
		const string &expires = "", const string &path = "/", 
		const string &domain = "" ) const;

	/// ���ָ����cookie����
	/// \param name cookie����
	inline void del_cookie( const string &name ) const {
		this->set_cookie( name, "", "Thursday,01-January-1970 08:00:01 GMT" );
	}
	
	/// ���ز���ֵ�б�
	/// \return ����ֵ����ΪCookieList,��map<string,string>.	
	inline CookieList dump() const {
		return _cookies;
	}

	////////////////////////////////////////////////////////////////////////////
	private:
	
	/// ����cookie����
	void parse_cookie( const string &buf );

	map<string,string> _cookies;		
};

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_CGI_H_

