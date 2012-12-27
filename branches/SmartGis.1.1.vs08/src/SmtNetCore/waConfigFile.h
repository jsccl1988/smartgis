/// \file waConfigFile.h
/// INI��ʽ�����ļ�������ͷ�ļ�
/// ������ webapp::String, webapp::TextFile

#ifndef _WEBAPPLIB_CONFIGFILE_H_
#define _WEBAPPLIB_CONFIGFILE_H_

#include <string>
#include <vector>
#include <map>

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// INI��ʽ�����ļ�������
class SMT_EXPORT_CLASS ConfigFile
{
	public:
	
	/// Ĭ�Ϲ��캯��
	ConfigFile(): _file(""), _unsaved(false) {};
	
	/// ����Ϊ�����ļ����Ĺ��캯��
	ConfigFile( const string &file )
	: _file(""), _unsaved(false) {
		this->load( file );
	}
	
	/// ��������
	~ConfigFile() {
		if ( _unsaved ) this->save();
	};

	////////////////////////////////////////////////////////////////////////////
	/// ��ȡ���������ļ�
	bool load( const string &file );
	
	/// ���������ļ�
	bool save( const string &file = "" );

	/// ����������Ƿ����	
	bool value_exist( const string &block, const string &name );
	
	/// ������ÿ��Ƿ����
	bool block_exist( const string &block );

	////////////////////////////////////////////////////////////////////////////
	/// ��ȡ���������ֵ
	inline string operator[] ( const string &name ) {
		return this->get_value( "", name );
	}

	/// ��ȡ���������ֵ
	string get_value( const string &block, const string &name, const string &default_value = "" );

	/// ��ȡָ�����ÿ��ȫ�����������ֵ
	map<string,string> get_block( const string &block );

	/// ��ȡȫ�����ÿ��б�					
	vector<string> block_list();
	
	////////////////////////////////////////////////////////////////////////////
	/// ����������
	inline bool set_value( const string &name, const string &value ) {
		return this->set_value( "", name, value );
	}

	/// ����������
	bool set_value( const string &block, const string &name, const string &value );
	
	/// ����ָ�����ÿ���������б�
	bool set_block( const string &block, const map<string,string> &valuelist );

	////////////////////////////////////////////////////////////////////////////
	/// ɾ��������
	void del_value( const string &block, const string &name );
	
	/// ɾ�����ÿ�
	void del_block( const string &block );
	
	////////////////////////////////////////////////////////////////////////////
	private:
	
	typedef map<string,string> value_def; // name -> value
	typedef map<string,value_def> block_def; // block -> ( name -> value )

	string _file;
	bool _unsaved;
	block_def _config;
	block_def::iterator _biter;
	value_def::iterator _viter;
};

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_CONFIGFILE_H_ 

