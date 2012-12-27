/// \file waEncode.h
/// ±àÂë,¼Ó½âÃÜº¯ÊýÍ·ÎÄ¼þ
/// ×Ö·û´®BASE64¡¢URI¡¢MD5±àÂëº¯Êý
   
#ifndef _WEBAPPLIB_ENCODE_H_
#define _WEBAPPLIB_ENCODE_H_ 

#include <string>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// URI±àÂë
string SMT_EXPORT_API uri_encode( const string &source );
/// URI½âÂë
string SMT_EXPORT_API uri_decode( const string &source );

/// ×Ö·û´®MIME BASE64±àÂë
string SMT_EXPORT_API base64_encode( const string &source );
/// ×Ö·û´®MIME BASE64½âÂë
string SMT_EXPORT_API base64_decode( const string &source );

/// MD5±àÂë
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

