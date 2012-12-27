/// \file waCgi.cpp
/// Cgi,Cookie��ʵ���ļ�

#include <cstdlib>
#include <climits>
#include <iostream>
#include <vector>
#include "waString.h"
#include "waEncode.h"
#include "waCgi.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
////////////////////////////////////////////////////////////////////////////////	

/// \ingroup waCgi
/// \fn void http_head()
/// ���HTML Content-Type header,�Զ������ظ����
void http_head() {
	static bool WEBAPP_ALREADY_HTTPHEAD = false;
	if ( !WEBAPP_ALREADY_HTTPHEAD ) {
		cout << "Content-Type: text/html\n" << endl;
		WEBAPP_ALREADY_HTTPHEAD = true;
	}
}

/// \ingroup waCgi
/// \fn string get_env( const string &envname )
/// ȡ�û�������
/// \param envname ����������
/// \return �ɹ����ػ�������ֵ,���򷵻ؿ��ַ���
string get_env( const string &envname ) {
	const char *env = envname.c_str();
	char *val = getenv( env );
	if ( val != NULL ) 
		return string( val );
	else 
		return string( "" );
}

////////////////////////////////////////////////////////////////////////////
// CGI

/// ���캯��
/// ��ȡ������CGI����
/// \param formdata_maxsize ������"multipart/form-data"��ʽPOSTʱ�����FORM�ϴ����ݴ�С,
/// �������ֱ��ضϲ�����,��λΪbyte,Ĭ��Ϊ0�����������ݴ�С
Cgi::Cgi( const long formdata_maxsize ) {
	// get envionment variable REQUEST_METHOD
	_method = get_env( "REQUEST_METHOD" );
	_method.upper();
	
	// set trunc flag
	_trunc = false;
	
	// method = GET
	if ( _method == "GET" ) {
		// read and parse QUERY_STRING
		this->parse_urlencoded( get_env("QUERY_STRING") );
	}
	
	// method = POST
	else if ( _method == "POST" ) {
		// get envionment variable CONTENT_TYPE
		string content_type = get_env( "CONTENT_TYPE" );
		
		char c;
		string buf;
		if ( formdata_maxsize > 0 )
			buf.reserve( formdata_maxsize );
		else
			buf.reserve( 256 );
		
		if ( content_type.find("application/x-www-form-urlencoded") != content_type.npos ) {
			// read stdin
			int content_length = atoi( (get_env("CONTENT_LENGTH")).c_str() );
			for ( int i=0; i<content_length; ++i ) {
				cin >> c;
				buf += c;
			}
			
			// parse stdin
			this->parse_urlencoded( buf );
			
		} else if ( content_type.find("multipart/form-data") != content_type.npos ) {
			// read stdin
			cin.unsetf( ios::skipws );

			if ( formdata_maxsize > 0 ) {
				// max size set
				long size = 0;
				
				while ( cin ) {
					cin >> c;
					buf += c;
					++size;
					
					if ( size > formdata_maxsize ) {
						cin.ignore( LONG_MAX );
						_trunc = true;
						break;
					}
				}
			} else {
				// no size limit
				while ( cin ) {
					cin >> c;
					buf += c;
				}
			}
			
			// parse stdin
			this->parse_multipart( content_type, buf );
		}
	}
}

/// ȡ��CGI����
/// \param name CGI������,��Сд����
/// \return �ɹ�����CGI����ֵ,���򷵻ؿ��ַ���,���ͬ��CGI����ֵ֮��ָ���Ϊ��ǿո�' '
string Cgi::get_cgi( const string &name ) const{
	if ( name == "" ) 
		return string( "" );
	
	if ( _method=="GET" || _method=="POST" ) {
		map<string,string>::const_iterator iter;
		iter = _cgi.find(name);
		if ( iter != _cgi.end() )
			return iter->second;
		else 
			return string( "" );
	}
	else if ( _method != "OPTIONS" && _method != "HEAD" && _method != "PUT" &&
			  _method != "DELETE" && _method != "TRACE" ) {
		// �ն˲���ģʽ���û����� cgi ����
		string cgival;
		cout << "Input value of CGI parameter \"" << name << "\", type _SPACE_ if no value: ";
		cin >> cgival;
		if ( cgival != "_SPACE_" )
			return cgival;
		else
			return string( "" );
	}
	else 
		return string( "" );
}

/// ����CGI����
/// \param name CGI������,��Сд����
/// \param value CGI����ֵ
void Cgi::add_cgi( const string &name, const string &value ) {
	if ( _cgi[name] == "" )
		_cgi[name] = value;
	else
		_cgi[name] += ( " " + value );
}

/// ����urlencoded��������
/// \param buf Ҫ����������
void Cgi::parse_urlencoded( const string &buf ) {
	/*****************************
	name1=value1&name2=value2&...
	*****************************/

	String buffer = buf;
	vector<String> pairslist = buffer.split( "&" );
	char szBuf[255];
	for ( size_t i=0; i<pairslist.size(); ++i ) {
		String name = pairslist[i].substr( 0, pairslist[i].find("=") );
		String value = pairslist[i].substr( pairslist[i].find("=")+1 );

		name.replace_all( "+", " " );
		name = uri_decode( name );
		value.replace_all( "+", " " );
		value = uri_decode( value );
		
		//�ؼ��ʻ��ɴ�д
		sprintf_s(szBuf,255,"%s",name.c_str());
		name = strupr(szBuf);

		add_cgi( name, value );
	}
}

/// ����multipart��������,
/// HTML FORM ����Ϊ enctype=multipart/form-data
/// \param content_type Content-Type�����ַ���
/// \param buf Ҫ����������
void Cgi::parse_multipart( const string &content_type, const string &buf ) {
	/*******************************************************
	��ָ���Ϊ{boundary}���س�(0x0D)�ͻ��з�(0x0A)Ϊ<CR>
	
	multipart/form-data, boundary={boundary}
	
	1.�ļ��Ͳ�����multipart��ʽ��
	--{boundary}<CR>
	Content-Disposition: form-data; name="��������"; filename="�ļ�����"<CR>
	Content-Type: {Content-Type}<CR>
	<CR>
	�ļ�����
	<CR>
	--{boundary}<CR>
	
	2.��ͨ������multipart��ʽ��
	--{boundary}<CR>
	Content-Disposition: form-data; name="��������"<CR>
	<CR>
	����ֵ
	<CR>
	--{boundary}<CR>
	*******************************************************/
	
	String buffer = buf;
	size_t pos = 0;
	const char cr[3] = "\x0D\x0A";
	String boundary = "";
	
	// get boundary
	/*****************************************************
	multipart/form-data, boundary={boundary}
	*****************************************************/
	
	if ( (pos=content_type.find("boundary=")) != content_type.npos )
		boundary = ( "--" + content_type.substr(pos+9) ); // 9: strlen("boundary=")
	else return; // format error
	
	// split by boundary
	vector<String> item = buffer.split( boundary );
	for ( size_t i=0; i<item.size(); ++i ) {
		if ( item[i].length() > 0 ) {
			String itembuf = item[i];
			size_t nextpos = 0;
			String itemname;
			String itemvalue;

			// split tags
			String crcr = cr; crcr += cr;	// <CR><CR>
			String qucr = "\""; qucr += cr;	// "<CR>
			String qucrcr = qucr + cr;		// "<CR><CR>

			if ( (pos=itembuf.find("; name=\"")) != itembuf.npos ) {
				size_t filename_pos = 0;
				if ( (filename_pos=itembuf.find("; filename=\"")) != itembuf.npos ) {					
					// �ļ��Ͳ���
					/******************************************************
					Content-Disposition: form-data; name="��������"; filename="�ļ�����"<CR>
					Content-Type: {Content-Type}<CR>
					<CR>
					�ļ�����
					<CR>
					******************************************************/
					
					// �������
					/******************************************************
					�������� = �ļ�����
					��������_name = �ļ�����
					��������_type = {Content-Type}
					******************************************************/

					String filename;
					String filetype;
					
					// ��������
					if ( filename_pos > (pos+9) ) // 9: strlen("; name=\"\"")
						itemname = itembuf.substr( pos+8, filename_pos-pos-9 ); // 9->8?

					if ( itemname != "" ) {
						// ��������_name = �ļ�����
						if ( (nextpos=itembuf.find(qucr,filename_pos)) != itembuf.npos
							&& nextpos >= (filename_pos+12) ) { // 12: strlen("; filename=\"")						
							filename = itembuf.substr( filename_pos+12, nextpos-filename_pos-12 );
							if ( filename != "" )
								add_cgi( (itemname+"_name"), filename );
						}

						// ��������_type = {Content-Type}
						if ( (pos=itembuf.find("Content-Type: ",filename_pos)) != itembuf.npos
							&& (nextpos=itembuf.find(crcr,pos)) != itembuf.npos
							&& nextpos >= (pos+14) ) { // 14: strlen("Content-Type: ")
							
							filetype = itembuf.substr( pos+14, nextpos-pos-14 );
							if ( filetype != "" )
								add_cgi( (itemname+"_type"), filetype );
						}

						// �������� = �ļ�����
						if ( (pos=nextpos+4) != itembuf.npos // 4: strlen("")
							&& (nextpos=itembuf.rfind(cr)) != itembuf.npos
							&& nextpos >= (pos+1) ) {
							itemvalue = itembuf.substr( pos, nextpos-pos );
							if ( itemvalue!= "" )
								add_cgi( itemname, itemvalue );
						}
					}
				} // �ļ��Ͳ���
				
				else {
					// ��ͨ����
					/******************************************************
					Content-Disposition: form-data; name="��������"<CR>
					<CR>
					����ֵ
					<CR>
					******************************************************/
					
					// ��������
					if ( (nextpos=itembuf.find(qucr,pos)) != itembuf.npos )
						itemname = itembuf.substr( pos+8, nextpos-pos-8 ); // 8: strlen("; name=\"")
					
					// �������� = ����ֵ
					if ( itemname != ""
						&& (pos=itembuf.find(qucrcr)) != itembuf.npos 
						&& (nextpos=itembuf.rfind(cr)) != itembuf.npos
						&& nextpos >= (pos+6) ) { // 6: strlen("\"<CR>\n")
						// 5: strlen("\"<CR>") 6: strlen("\"<CR>\n")
						itemvalue = itembuf.substr( pos+5, nextpos-pos-5 );
						if ( itemvalue != "" )
							add_cgi( itemname, itemvalue );
					}
				}// ��ͨ����
			}
		} // one item
	} // split by boundary
	
}

////////////////////////////////////////////////////////////////////////////
// Cookie

/// ���캯��
/// ��ȡ������Cookie��������
Cookie::Cookie() {
	this->parse_cookie( get_env("HTTP_COOKIE") );
}

/// ȡ��cookie����
/// \param name cookie������,��Сд����
/// \return �ɹ�����cookie����ֵ,���򷵻ؿ��ַ���
string Cookie::get_cookie( const string &name ) {
	if ( name!="" && _cookies.find(name)!=_cookies.end() )
		return _cookies[name];
	else
		return string( "" );
}

/// ����cookie����
/// ���������content-typeǰ����
/// \param name cookie����
/// \param value cookieֵ
/// \param expires cookie��Ч��,GMT��ʽ�����ַ���,Ĭ��Ϊ��
/// \param path cookie·��,Ĭ��Ϊ"/"
/// \param domain cookie��,Ĭ��Ϊ""
void Cookie::set_cookie( const string &name, const string &value, 
	const string &expires, const string &path, const string &domain ) const 
{
	// Set-Cookie: name=value; expires=expires; path=path; domain=domain;
	
	string expires_setting;
	if ( expires != "" )
		expires_setting = "expires=" + expires + "; ";
	else
		expires_setting = "";
	
	cout << "Set-Cookie: " + name + "=" + value + "; "
		 << expires_setting
		 << "path=" + path + "; "
		 << "domain=" + domain + ";" << endl;
}

/// ����cookie����
/// \param buf Ҫ����������
void Cookie::parse_cookie( const string &buf ) {
	/*****************************
	name1=value1; name2=value2; ...
	*****************************/

	String buffer = buf;
	vector<String> pairslist = buffer.split( "; " );
	
	for ( size_t i=0; i<pairslist.size(); ++i ) {
		String name = pairslist[i].substr( 0, pairslist[i].find("=") );
		String value = pairslist[i].substr( pairslist[i].find("=")+1 );

		name.replace_all( "+", " " );
		name = uri_decode( name );
		value.replace_all( "+", " " );
		value = uri_decode( value );
		
		_cookies[name] = value;
	}
}

} // namespace
