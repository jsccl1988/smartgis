/// \file waTextFile.cpp
/// �̶��ָ����ı��ļ���ȡ������ʵ���ļ�
/// ��ȡ�����̶��ָ����ı��ļ�

#include "waTextFile.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
/// ���ı��ļ�
/// \param file �ı��ļ�·����
/// \retval true ���ļ��ɹ�
/// \retval false ���ļ�ʧ��
bool TextFile::open( const string &file ) {
	if ( file == "" ) return false;
	if ( _fp != NULL ) this->close();
	
	_fp = fopen( file.c_str(), "r" );
	if ( _fp != NULL ) return true;
	else return false;
}

/// �ر��ı��ļ�
void TextFile::close() {
	if ( _fp != NULL ) {
		fclose( _fp );
		_fp = NULL;
	}
}

/// ��ȡ��һ��
/// \param line ��ȡ�����ı��У�����ĩβ��'\\n'
/// \retval true ��δ�����ļ�ĩβ
/// \retval false �Ѷ����ļ�ĩβ
bool TextFile::next_line( string &line ) {
	if( _fp == NULL ) 
		return false;

	fgets( _line, ONE_LINE_BUF, _fp );

	line = _line;

	return true;
}

/// ��ȡ��һ�в����ָ�������ֶ�
/// \param fields ��ȡ�����ַ�������
/// \param split �ֶ��зַָ�����Ĭ��Ϊ'\\t'
/// \param limit �ֶ��зִ������ƣ�Ĭ��Ϊ0��������
/// \retval true ��δ�����ļ�ĩβ
/// \retval false �Ѷ����ļ�ĩβ
bool TextFile::next_fields( vector<String> &fields, const string &split, const int limit ) {
	String line;
	if ( this->next_line(line) ) {
		fields = line.split( split, limit );
		return true;
	}
	return false;
}
	
} // namespace

