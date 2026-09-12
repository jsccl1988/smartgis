/// \file waEncode.h
/// 编码,加解密函数头文件
/// 字符串BASE64、URI、MD5编码函数
   
#ifndef _WEBAPPLIB_ENCODE_H_
#define _WEBAPPLIB_ENCODE_H_ 

#include <string>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// URI编码
string SMT_EXPORT_API uri_encode( const string &source );
/// URI解码
string SMT_EXPORT_API uri_decode( const string &source );

/// 字符串MIME BASE64编码
string SMT_EXPORT_API base64_encode( const string &source );
/// 字符串MIME BASE64解码
string SMT_EXPORT_API base64_decode( const string &source );

/// MD5编码
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

