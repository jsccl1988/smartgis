/// \file waTextFile.h
/// �̶��ָ����ı��ļ���ȡ������ͷ�ļ�
/// ��ȡ�����̶��ָ����ı��ļ�
/// ������ webapp::String

#ifndef _WEBAPPLIB_TEXTFILE_H_
#define _WEBAPPLIB_TEXTFILE_H_ 

#include <cstdio>
#include "waString.h"

#include "smt_core.h"

#define  ONE_LINE_BUF		1024
using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
/// �̶��ָ����ı��ļ���ȡ������
class SMT_EXPORT_CLASS TextFile {
	public:
		
	/// Ĭ�Ϲ��캯��
	TextFile():_fp(0)
	{};
	
	/// ����Ϊ�ı��ļ����Ĺ��캯��
	TextFile( const string &file )
	:_fp(0){
		this->open( file );
	}
	
	/// ��������
	~TextFile() {
		this->close();
	}
	
	/// ���ı��ļ�
	bool open( const string &file );
	
	/// �ر��ı��ļ�
	void close();
	
	/// ��ȡ��һ��
	bool next_line( string &line );
	
	/// ��ȡ��һ�в����ָ�������ֶ�
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

