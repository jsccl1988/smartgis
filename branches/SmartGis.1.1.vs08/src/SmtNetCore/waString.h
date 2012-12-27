/// \file waString.h
/// webapp::String��ͷ�ļ�
/// �̳���string���ַ�����
/// <a href=std_string.html>����stringʹ��˵���ĵ�</a>

#ifndef _WEBAPPLIB_STRING_H_
#define _WEBAPPLIB_STRING_H_ 

#include <sys/stat.h>
#include <cstdarg>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
////////////////////////////////////////////////////////////////////////////////	
// �հ��ַ��б�
const char BLANK_CHARS[] = " \t\n\r\v\f";

////////////////////////////////////////////////////////////////////////////////
/// long intת��Ϊstring
string	SMT_EXPORT_API itos( const long i, const ios::fmtflags base = ios::dec );
/// stringת��Ϊint
long	SMT_EXPORT_API stoi( const string &s, const ios::fmtflags base = ios::dec );

/// doubleת��Ϊstring
string	SMT_EXPORT_API ftos( const double f, const int ndigit = 2 );
/// stringת��Ϊdouble
double	SMT_EXPORT_API stof( const string &s );

/// �ж�һ��˫�ֽ��ַ��Ƿ���GBK���뺺��
bool	SMT_EXPORT_API isgbk( const unsigned char c1, const unsigned char c2 );

/// �ɱ�����ַ�����ʽ������va_start()��va_end()�����ʹ��
string	SMT_EXPORT_API va_sprintf( va_list ap, const string &format );
/// ��ʽ���ַ���������
string	SMT_EXPORT_API va_str( const char *format, ... );

////////////////////////////////////////////////////////////////////////////////
/// �̳���string���ַ�����
/// <a href="std_string.html">����stringʹ��˵���ĵ�</a>
class SMT_EXPORT_CLASS String : public string {
	public:
	
	////////////////////////////////////////////////////////////////////////////
	/// Ĭ�Ϲ��캯��
	String(){}
	
	/// ����Ϊchar*�Ĺ��캯��
	String( const char *s ) {
		if( s ) this->assign( s );
		else this->erase();
	}
	
	/// ����Ϊstring�Ĺ��캯��
	String( const string &s ) {
		this->assign( s );
	}
	
	/// ��������
	virtual ~String(){}
	
	////////////////////////////////////////////////////////////////////////////
	/// \enum ����String::split()�ָ������ط�ʽ
	enum split_mode {
		/// ������������ָ��������ؽ���������ֶ�
		SPLIT_IGNORE_BLANK,
		/// ��������������ָ��������ؽ���������ֶ�
		SPLIT_KEEP_BLANK
	};	

	////////////////////////////////////////////////////////////////////////////
	/// ���� char* �ͽ���������߱������ delete[] �ͷ��������ڴ�
	char* c_char() const;
	
	/// �����ַ�������֧��ȫ���ַ�
	string::size_type w_length() const;
	/// ��ȡ���ַ�����֧��ȫ���ַ�
	String w_substr( const string::size_type pos = 0, 
		const string::size_type n = npos ) const;

	/// ������հ��ַ�
	void trim_left( const string &blank = BLANK_CHARS );
	/// ����Ҳ�հ��ַ�
	void trim_right( const string &blank = BLANK_CHARS );
	/// �������հ��ַ�
	void trim( const string &blank = BLANK_CHARS );

	/// ����߽�ȡָ�������Ӵ�
	String left( const string::size_type n ) const;
	/// ���м��ȡָ�������Ӵ�
	String mid( const string::size_type pos, 
		const string::size_type n = npos ) const;
	/// ���ұ߽�ȡָ�������Ӵ�
	String right( const string::size_type n ) const;
	
	/// �����ַ�������
	void resize( const string::size_type n );

	/// ͳ��ָ���Ӵ����ֵĴ���
	int count( const string &str ) const;
	
	/// ���ݷָ���ָ��ַ���
	vector<String> split( const string &tag, const int limit = 0, 
		const split_mode mode = SPLIT_IGNORE_BLANK ) const;
	
	/// ת���ַ���ΪMAP�ṹ(map<string,string>)
	map<string,string> tomap( const string &itemtag = "&", 
		const string &exptag = "=" ) const;

	/// ����ַ���
	void join( const vector<string> &strings, const string &tag );
	void join( const vector<String> &strings, const string &tag );

	/// ��ʽ����ֵ
	bool sprintf( const char *format, ... );
	
	/// �滻
	int replace( const string &oldstr, const string &newstr );
	/// ȫ���滻
	int replace_all( const string &oldstr, const string &newstr );
	
	/// ת��Ϊ��д��ĸ
	void upper();
	/// ת��ΪСд��ĸ
	void lower();
	
	/// �ַ����Ƿ���ȫ���������
	bool isnum() const;
	
	/// ��ȡ�ļ����ַ���
	bool load_file( const string &filename );
	/// �����ַ������ļ�
	bool save_file( const string &filename, const ios::openmode mode = ios::trunc|ios::out) const;
};

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_STRING_H_

