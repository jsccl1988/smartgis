/// \file waEncode.h
/// ����,�ӽ��ܺ���ͷ�ļ�
/// �ַ���BASE64��URI��MD5���뺯��
   
#ifndef _WEBAPPLIB_ENCODE_H_
#define _WEBAPPLIB_ENCODE_H_ 

#include <string>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// URI����
string SMT_EXPORT_API uri_encode( const string &source );
/// URI����
string SMT_EXPORT_API uri_decode( const string &source );

/// �ַ���MIME BASE64����
string SMT_EXPORT_API base64_encode( const string &source );
/// �ַ���MIME BASE64����
string SMT_EXPORT_API base64_decode( const string &source );

/// MD5����
string SMT_EXPORT_API md5_encode( const string &source );

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_ENCODE_H_

