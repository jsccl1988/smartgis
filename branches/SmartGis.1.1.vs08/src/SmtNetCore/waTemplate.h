/// \file waTemplate.h
/// HTMLģ�崦����ͷ�ļ�
/// ֧��������ѭ���ű���HTMLģ�崦����
/// ������ waString
/// <a href="wa_template.html">ʹ��˵���ĵ����򵥷���</a>

#ifndef _WEBAPPLIB_TMPL_H_
#define _WEBAPPLIB_TMPL_H_ 

#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "waString.h"

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
/// ֧��������ѭ���ű���HTMLģ�崦����
/// <a href="wa_template.html">ʹ��˵���ĵ����򵥷���</a>
class SMT_EXPORT_CLASS Template {
	public:
	
	/// Ĭ�Ϲ��캯��
	Template(){};
	
	/// ���캯��
	/// \param tmpl_file ģ���ļ�
	Template( const string tmpl_file ) {
		this->load( tmpl_file );
	}
	
	/// ���캯��
	/// \param tmpl_dir ģ��Ŀ¼
	/// \param tmpl_file ģ���ļ�
	Template( const string tmpl_dir, const string tmpl_file ) {
		this->load( tmpl_dir, tmpl_file );
	}
	
	/// ��������
	virtual ~Template(){};
	
	/// \enum ���ʱ�Ƿ����������Ϣ
	enum output_mode {
		/// ��ʾ������Ϣ
		TMPL_OUTPUT_DEBUG,	
		/// ����ʾ
		TMPL_OUTPUT_RELEASE	
	};

	/// ��ȡHTMLģ���ļ�
	bool load( const string &tmpl_file );

	/// ��ȡģ��
	/// \param tmpl_dir ģ��Ŀ¼
	/// \param tmpl_file ģ���ļ�
	/// \retval true ��ȡ�ɹ�
	/// \retval false ʧ��
	inline bool load( const string &tmpl_dir, const string &tmpl_file ) {
		return this->load( tmpl_dir + "/" + tmpl_file );
	}
	
	/// ����HTMLģ������
	void tmpl( const string &tmpl );

	/// �����滻����
	void set( const string &name, const string &value );
	/// �����滻����
	/// \param name ģ��������
	/// \param value �滻ֵ
	inline void set( const string &name, const long value ) {
		this->set( name, itos(value) );
	}
	
	/// �½�ѭ��
	void def_loop( const string &loop, const char* field_0, ... );
	/// ���һ�����ݵ�ѭ��
	void append_row( const string &loop, const char* value_0, ... );
	/// ���һ��ָ����ʽ�����ݵ�ѭ��
	void append_format( const string &loop, const char* format, ... );
	
	/// ��������滻����
	void clear_set();

	/// ����HTML�ַ���
	string html();
	/// ���HTML��stdout
	void print( const output_mode mode = TMPL_OUTPUT_RELEASE );
	/// ���HTML���ļ�
	bool print( const string &file, const output_mode mode = TMPL_OUTPUT_RELEASE);
	
	////////////////////////////////////////////////////////////////////////////
	private:
	
	/// ��ȡָ��λ�õ�ģ��ű����ͼ����ʽ
	int parse_script( const string &tmpl, const unsigned int pos,
		string &exp, int &type );

	/// �������ʽ��ֵ
	string exp_value( const string &expression );

	/// ��������ģ��
	void parse( const string &tmpl, ostream &output );
	
	/// ������������ʽ�Ƿ����
	bool compare( const string &exp );
	
	/// ��������Ƿ����
	bool check_if( const string &exp );

	/// ������������ģ��
	size_t parse_if( const string &tmpl, ostream &output, 
		const bool parent_state, const string &parsed_exp,
		const int parsed_length );

	/// �����ֶ�λ��
	int field_pos( const string &loop, const string &field );

	/// ���ѭ������Ƿ���Ч
	bool check_loop( const string &loopname );
	
	/// ����ѭ����ָ��λ���ֶε�ֵ
	string loop_value( const string &field );

	/// ����ѭ������ģ��
	size_t parse_loop( const string &tmpl, ostream &output, 
		const bool parent_state, const string &parsed_exp,
		const int parsed_length );
							
	/// ģ����������¼
	void error_log( const size_t lines, const string &error );
	/// ģ�������¼
	void parse_log( ostream &output );

	// ���ݶ���
	typedef vector<string> strings;		// �ַ����б�
	typedef struct {					// ѭ��ģ�����ýṹ
		int cols;						// ѭ���ֶ�����
		int rows;						// ѭ����������
		int cursor;						// ��ǰ���λ��
		strings fields;					// ѭ���ֶζ����б�
		map<string,int> fieldspos;		// ѭ���ֶ�λ��,for speed
		vector<strings> datas;			// ѭ������
	} tmpl_loop;

	// ģ������
	String _tmpl;						// HTMLģ������
	map<string,string> _sets;			// �滻�����б� <ģ��������,ģ����ֵ>
	map<string,tmpl_loop> _loops;		// ѭ���滻�����б� <ѭ������,ѭ��ģ�����ýṹ>
	
	// ������������
	string _loop;						// ��ǰѭ������
	int _cursor;						// ��ǰѭ�����λ��
	int _lines;							// �Ѵ���ģ������

	string _tmplfile;					// HTMLģ���ļ���
	char _date[15];						// ��ǰ����
	char _time[15];						// ��ǰʱ��
	output_mode _debug;					// ����ģʽ
	multimap<int,string> _errlog;		// ���������¼ <����λ������,����������Ϣ>
};

// ģ���﷨��ʽ����
const char TMPL_BEGIN[]		= "{{";		const int TMPL_BEGIN_LEN 	= strlen(TMPL_BEGIN);
const char TMPL_END[]		= "}}";		const int TMPL_END_LEN 		= strlen(TMPL_END);
const char TMPL_SUBBEGIN[]	= "(";		const int TMPL_SUBBEGIN_LEN = strlen(TMPL_SUBBEGIN);
const char TMPL_SUBEND[]	= ")";		const int TMPL_SUBEND_LEN 	= strlen(TMPL_SUBEND);
const char TMPL_SPLIT[]		= ",";		const int TMPL_SPLIT_LEN 	= strlen(TMPL_SPLIT);
const char TMPL_NEWLINE[]	= "\n";		const int TMPL_NEWLINE_LEN 	= strlen(TMPL_NEWLINE);

const char TMPL_VALUE[]		= "$";		const int TMPL_VALUE_LEN 	= strlen(TMPL_VALUE);
const char TMPL_DATE[]		= "%DATE";	const int TMPL_DATE_LEN 	= strlen(TMPL_DATE);
const char TMPL_TIME[]		= "%TIME";	const int TMPL_TIME_LEN 	= strlen(TMPL_TIME);
const char TMPL_SPACE[]		= "%SPACE";	const int TMPL_SPACE_LEN	= strlen(TMPL_SPACE);
const char TMPL_BLANK[]		= "%BLANK";	const int TMPL_BLANK_LEN 	= strlen(TMPL_BLANK);
                                        
const char TMPL_LOOP[]		= "#FOR";	const int TMPL_LOOP_LEN 	= strlen(TMPL_LOOP);
const char TMPL_ENDLOOP[]	= "#ENDFOR";const int TMPL_ENDLOOP_LEN = strlen(TMPL_ENDLOOP);
const char TMPL_LOOPVALUE[]	= ".$";		const int TMPL_LOOPVALUE_LEN = strlen(TMPL_LOOPVALUE);
const char TMPL_LOOPSCOPE[]	= "@";		const int TMPL_LOOPSCOPE_LEN = strlen(TMPL_LOOPSCOPE);
const char TMPL_CURSOR[]	= "%CURSOR";const int TMPL_CURSOR_LEN	= strlen(TMPL_CURSOR);
const char TMPL_ROWS[]		= "%ROWS";	const int TMPL_ROWS_LEN		= strlen(TMPL_ROWS);

const char TMPL_IF[]		= "#IF";	const int TMPL_IF_LEN 		= strlen(TMPL_IF);
const char TMPL_ELSIF[]		= "#ELSIF";	const int TMPL_ELSIF_LEN 	= strlen(TMPL_ELSIF);
const char TMPL_ELSE[]		= "#ELSE";	const int TMPL_ELSE_LEN 	= strlen(TMPL_ELSE);
const char TMPL_ENDIF[]		= "#ENDIF";	const int TMPL_ENDIF_LEN 	= strlen(TMPL_ENDIF);

// �Ƚϲ���������
const char TMPL_AND[]		= "AND";	const int TMPL_AND_LEN 		= strlen(TMPL_AND);
const char TMPL_OR[]		= "OR";		const int TMPL_OR_LEN 		= strlen(TMPL_OR);
const char TMPL_EQ[]		= "==";		const int TMPL_EQ_LEN 		= strlen(TMPL_EQ);
const char TMPL_NE[]		= "!=";		const int TMPL_NE_LEN 		= strlen(TMPL_NE);
const char TMPL_LE[]		= "<=";		const int TMPL_LE_LEN 		= strlen(TMPL_LE);
const char TMPL_LT[]		= "<";		const int TMPL_LT_LEN 		= strlen(TMPL_LT);
const char TMPL_GE[]		= ">=";		const int TMPL_GE_LEN 		= strlen(TMPL_GE);
const char TMPL_GT[]		= ">";		const int TMPL_GT_LEN 		= strlen(TMPL_GT);

// Htt::format_row()��ʽ����
const char TMPL_FMTSTR[]	= "%s";		const int TMPL_FMTSTR_LEN 	= strlen(TMPL_FMTSTR);
const char TMPL_FMTINT[]	= "%d";		const int TMPL_FMTINT_LEN 	= strlen(TMPL_FMTINT);

// �ű��������
enum tmpl_scripttype {
	TMPL_S_VALUE,	
	TMPL_S_LOOPVALUE,	
	TMPL_S_LOOP,	
	TMPL_S_ENDLOOP,	
	TMPL_S_IF,		
	TMPL_S_ELSIF,	
	TMPL_S_ELSE,		
	TMPL_S_ENDIF,
	TMPL_S_CURSOR,
	TMPL_S_ROWS,
	TMPL_S_DATE,
	TMPL_S_TIME,
	TMPL_S_SPACE,
	TMPL_S_BLANK,
	TMPL_S_UNKNOWN
};

// �߼���������
enum tmpl_logictype {
	TMPL_L_NONE,	
	TMPL_L_AND,
	TMPL_L_OR
};

// �Ƚ���������
enum tmpl_cmptype {
	TMPL_C_EQ,
	TMPL_C_NE,
	TMPL_C_LE,
	TMPL_C_LT,
	TMPL_C_GE,
	TMPL_C_GT
};

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_TMPL_H_

