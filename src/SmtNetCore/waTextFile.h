/// \file waTextFile.h
/// 固定分隔符文本文件读取解析类头文件
/// 读取解析固定分隔符文本文件
/// 依赖于 webapp::String

#ifndef _WEBAPPLIB_TEXTFILE_H_
#define _WEBAPPLIB_TEXTFILE_H_ 

#include <cstdio>
#include "waString.h"

#include "smt_core.h"

#define  ONE_LINE_BUF		1024
using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
/// 固定分隔符文本文件读取解析类
class SMT_EXPORT_CLASS TextFile {
	public:
		
	/// 默认构造函数
	TextFile():_fp(0)
	{};
	
	/// 参数为文本文件名的构造函数
	TextFile( const string &file )
	:_fp(0){
		this->open( file );
	}
	
	/// 析构函数
	~TextFile() {
		this->close();
	}
	
	/// 打开文本文件
	bool open( const string &file );
	
	/// 关闭文本文件
	void close();
	
	/// 读取下一行
	bool next_line( string &line );
	
	/// 读取下一行并按分隔符拆分字段
	bool next_fields( vector<String> &fields, const string &split="\t", const int limit=0 );
	
	////////////////////////////////////////////////////////////////////////////
	private:
		
	FILE *_fp;
	char  _line[ONE_LINE_BUF];
};
	
} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_TEXTFILE_H_

