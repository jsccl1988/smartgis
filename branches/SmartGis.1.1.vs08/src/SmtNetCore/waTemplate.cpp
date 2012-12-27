/// \file waTemplate.cpp
/// HTMLģ�崦����ʵ���ļ�

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <cstdlib>  
#include <time.h>  
#include <stdio.h>  

#include "waTemplate.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
////////////////////////////////////////////////////////////////////////////
// set functions

/// ��ȡHTMLģ���ļ�
/// \param tmpl_file ģ��·���ļ���
/// \retval true ��ȡ�ɹ�
/// \retval false ʧ��
bool Template::load( const string &tmpl_file ) {
	if ( _tmpl.load_file(tmpl_file) ) {
		_tmplfile = tmpl_file;
		return true;
	} else {
		_tmplfile = "Error: Can't open file " + tmpl_file;
		this->error_log( 0, _tmplfile );
		return false;
	}
}

/// ����HTMLģ������
/// \param tmpl ģ�������ַ���
void Template::tmpl( const string &tmpl ) {
	_tmplfile = "Read from string";
	_tmpl = tmpl;
}

/// �����滻����
/// \param name ģ��������
/// \param value �滻ֵ
void Template::set( const string &name, const string &value ) {
	if ( name != "" )
		_sets[name] = value;
}

/// �½�ѭ��
/// \param loop ѭ������
/// \param field_0 field_0��field_0֮��Ϊ�ֶ������б�,���һ������������NULL
/// \param ... �ֶ������б�,���һ������������NULL
void Template::def_loop( const string &loop, const char* field_0, ... ) {
	va_list ap;
	const char *p;
	string field;
	strings fields;
	size_t cols = 0;
	
	// check same name loop
	if ( _loops.find(loop) != _loops.end() )
		this->error_log( 0, "Warning: loop name \""+loop+"\" redefined" );
	
	// get fields
	va_start( ap, field_0 );
	for ( p=field_0; p; p=va_arg(ap,const char*) ) {
		if ( (field=p) != "" ) {
			fields.push_back( field );
			_loops[loop].fieldspos[field] = cols; // for speed
			++cols;
		}
	}
	va_end( ap );

	// for waTemplate old version templet script ( < v0.7 )
	if ( _sets.find(loop) == _sets.end() )
		set( loop, loop );

	// init loop
	_loops[loop].fields = fields;
	for ( size_t i=0; i<_loops[loop].datas.size(); ++i )
		_loops[loop].datas[i].clear();
	_loops[loop].datas.clear();
	_loops[loop].cursor = 0;
	_loops[loop].rows = 0;
	_loops[loop].cols = cols;
}

/// ���һ�����ݵ�ѭ��
/// �����ȵ���Template::def_loop()��ʼ��ѭ���ֶζ���,������ֹ
/// \param loop ѭ������
/// \param value_0 value_0��value_0֮��Ϊ�ֶ������б�,���һ������������NULL
/// \param ... �ֶ������б�,���һ������������NULL
void Template::append_row( const string &loop, const char* value_0, ... ) {
	// loop must exist
	if ( _loops.find(loop) == _loops.end() ) {
		this->error_log( 0, "Error: Call def_loop() to init $"+loop+" first, in append_row()" );
		return;
	}
	
	// get values
	va_list ap;
	const char *p;
	string value;
	strings values;
	int cols = 0;
	
	va_start( ap, value_0 );
	for ( p=value_0; p; p=va_arg(ap,const char*) ) {
		value = p;
		values.push_back( value );
		++cols;
		
		// enough now
		if ( cols >= _loops[loop].cols )
			break;
	}
	va_end( ap );

	// fill blank if not enough
	if( cols < _loops[loop].cols ) {
		for ( int i=cols; i<_loops[loop].cols; ++i )
			values.push_back( "" );
	}
	
	// insert into loop
	_loops[loop].datas.push_back( values );
	++_loops[loop].rows;
}

/// ���һ��ָ����ʽ�����ݵ�ѭ��
/// �����ȵ���Template::def_loop()��ʼ��ѭ���ֶζ���,������ֹ
/// \param loop ѭ������
/// \param format �ֶ���ѭ��ʽ����,"%d,%s,..."��ʽ
/// \param ... ������������Ϊ�ֶ�ֵ�б�,
/// �ֶ�ֵ���������������ڸ�ʽ�������format��ָ���ĸ���
void Template::append_format( const string &loop, const char* format, ... ) {
	// loop must exist
	if ( _loops.find(loop) == _loops.end() ) {
		this->error_log( 0, "Error: Call def_loop() to init $"+loop+" first, in append_format()" );
		return;
	}
	
	// split format string
	String fmtstr = format;
	vector<String> fmtlist = fmtstr.split( TMPL_SPLIT );
	
	// get values
	va_list ap;
	string value;
	strings values;
	int cols = 0;
	
	va_start( ap, format );
	for ( size_t i=0; i<fmtlist.size(); ++i ) {
		// read
		fmtlist[i].trim();
		if ( fmtlist[i] == TMPL_FMTSTR )
			value = va_arg( ap, const char* ); // %s
		else
			value = itos( va_arg(ap,long) ); // %d or other
			
		// push data
		values.push_back( value );
		++cols;

		// enough now
		if ( cols >= _loops[loop].cols )
			break;
	}
	va_end( ap );

	// fill blank if not enough
	if( cols < _loops[loop].cols ) {
		for ( int i=cols; i<_loops[loop].cols; ++i )
			values.push_back( "" );
	}
	
	// insert into loop
	_loops[loop].datas.push_back( values );
	++_loops[loop].rows;
}

/// ��������滻����
/// ��������ѭ���滻����
void Template::clear_set() {
	_sets.clear();
	_loops.clear();
}

////////////////////////////////////////////////////////////////////////////
// parse functions

/// �����ֶ�λ��
/// \param loop ѭ������
/// \param field �ֶ�����
/// \return �ҵ������ֶ�λ��,���򷵻�-1
int Template::field_pos( const string &loop, const string &field ) {
	if ( _loops[loop].fieldspos.find(field) == _loops[loop].fieldspos.end() )
		return -1;
	return _loops[loop].fieldspos[field];
}

/// ��ȡָ��λ�õ�ģ��ű����ͼ����ʽ
/// \param tmpl ģ���ַ���
/// \param pos ��ʼ������λ��
/// \param exp ��ȡ���ı��ʽ�ַ���
/// \param type �������Ľű��������
/// \return ����ֵΪ���η������ַ�������,��������-1
int Template::parse_script( const string &tmpl, const size_t pos, 
	string &exp, int &type ) 
{
	// find TMPL_END
	size_t begin = pos + TMPL_BEGIN_LEN;
	size_t end;
	if ( (end=tmpl.find(TMPL_END,begin)) == tmpl.npos )
		return -1;	// can not find TMPL_END

	// script type and content
	String content = tmpl.substr( begin, end-begin );
	content.trim();

	if ( strncmp(content.c_str(),TMPL_VALUE,TMPL_VALUE_LEN) == 0 ) {
		// simple value: $xxx
		type = TMPL_S_VALUE;
		
	} else if ( strncmp(content.c_str(),TMPL_LOOPVALUE,TMPL_LOOPVALUE_LEN) == 0 ) {
		// current value in loop: .$xxx
		type = TMPL_S_LOOPVALUE;
		
	} else if ( strncmp(content.c_str(),TMPL_LOOP,TMPL_LOOP_LEN) == 0 ) {
		// for begin: #FOR xxx
		type = TMPL_S_LOOP;
		content = tmpl.substr( begin+TMPL_LOOP_LEN, end-begin-TMPL_LOOP_LEN );
		content.trim();
		
	} else if ( strcmp(content.c_str(),TMPL_ENDLOOP) == 0 ) {
		// for end: #ENDFOR
		type = TMPL_S_ENDLOOP;
		
	} else if ( strncmp(content.c_str(),TMPL_IF,TMPL_IF_LEN) == 0 ) {
		// if begin: #IF xxx
		type = TMPL_S_IF;
		content = tmpl.substr( begin+TMPL_IF_LEN, end-begin-TMPL_IF_LEN );
		content.trim();
	
	} else if ( strncmp(content.c_str(),TMPL_ELSIF,TMPL_ELSIF_LEN) == 0 ) {
		// elseif: #ELSIF xxx
		type = TMPL_S_ELSIF;
		content = tmpl.substr( begin+TMPL_ELSIF_LEN, end-begin-TMPL_ELSIF_LEN );
		content.trim();
	
	} else if ( strcmp(content.c_str(),TMPL_ELSE) == 0 ) {
		// else: #ELSE
		type = TMPL_S_ELSE;
	
	} else if ( strcmp(content.c_str(),TMPL_ENDIF) == 0 ) {
		// if end: #ENDIF
		type = TMPL_S_ENDIF;
	
	} else if ( strncmp(content.c_str(),TMPL_CURSOR,TMPL_CURSOR_LEN) == 0 ) {
		// current loop cursor: %CURSOR
		type = TMPL_S_CURSOR;
	
	} else if ( strncmp(content.c_str(),TMPL_ROWS,TMPL_ROWS_LEN) == 0 ) {
		// current loop cursor: %ROWS
		type = TMPL_S_ROWS;

	} else if ( strcmp(content.c_str(),TMPL_DATE) == 0 ) {
		// date: %DATE
		type = TMPL_S_DATE;
	
	} else if ( strcmp(content.c_str(),TMPL_TIME) == 0 ) {
		// time: %TIME
		type = TMPL_S_TIME;
	
	} else if ( strcmp(content.c_str(),TMPL_SPACE) == 0 ) {
		// space char: %SPACE
		type = TMPL_S_SPACE;
	
	} else if ( strcmp(content.c_str(),TMPL_BLANK) == 0 ) {
		// blank string: %BLANK
		type = TMPL_S_BLANK;
	
	} else {
		type = TMPL_S_UNKNOWN;
	}
	
	// return parsed length
	exp = content;
	return ( end-pos+TMPL_END_LEN );
}

/// �������ʽ��ֵ
/// \param exp ���ʽ�ַ���
/// \return ����ֵΪ�ñ��ʽ��ֵ,�����ʽ�Ƿ��򷵻ر��ʽ�ַ���
string Template::exp_value( const string &exp ) {
	if ( strncmp(exp.c_str(),TMPL_VALUE,TMPL_VALUE_LEN) == 0 ) {
		// simple value: $xxx
		string val = exp.substr( TMPL_VALUE_LEN );
		return _sets[val];
		
	} else if ( strncmp(exp.c_str(),TMPL_LOOPVALUE,TMPL_LOOPVALUE_LEN) == 0 ) {
		// current value in loop: .$xxx
		string val = exp.substr( TMPL_LOOPVALUE_LEN );
		return this->loop_value( val );
		
	} else if ( strncmp(exp.c_str(),TMPL_CURSOR,TMPL_CURSOR_LEN) == 0 ) {
		// current loop cursor: %CURSOR
		size_t pos = exp.find( TMPL_LOOPSCOPE );
		if ( pos != exp.npos ) {
			string loop_name = this->exp_value( exp.substr(pos+TMPL_LOOPSCOPE_LEN) );
			int cursor = _loops[loop_name].cursor;
			return itos(cursor+1);
		} else {
			return itos(_cursor+1);
		}
	
	} else if ( strncmp(exp.c_str(),TMPL_ROWS,TMPL_ROWS_LEN) == 0 ) {
		// current loop cursor: %ROWS
		size_t pos = exp.find( TMPL_LOOPSCOPE );
		if ( pos != exp.npos ) {
			string loop_name = this->exp_value( exp.substr(pos+TMPL_LOOPSCOPE_LEN) );
			return itos( _loops[loop_name].rows );
		} else {
			return itos( _loops[_loop].rows );
		}

	} else if ( strcmp(exp.c_str(),TMPL_DATE) == 0 ) {
		// date: %DATE
		return _date;
	
	} else if ( strcmp(exp.c_str(),TMPL_TIME) == 0 ) {
		// time: %TIME
		return _time;
	
	} else if ( strcmp(exp.c_str(),TMPL_SPACE) == 0 ) {
		// space char: %SPACE
		return " ";
	
	} else if ( strcmp(exp.c_str(),TMPL_BLANK) == 0 ) {
		// blank string: %BLANK
		return "";
	
	} else  {
		// string
		return exp;
	}
}

/// ��������ģ��
/// \param tmpl ģ���ַ���
/// \param output ���������������
void Template::parse( const string &tmpl, ostream &output ) {
	// init datetime

	time_t     ltime;
	struct tm  *ptm = NULL;

	if (time(&ltime) == -1) {                
		return;
	}

	ptm = localtime(&ltime);

	snprintf( _date, 15, "%d-%d-%d", ptm->tm_year+1900, ptm->tm_mon+1, ptm->tm_mday );
	snprintf( _time, 15, "%d:%d:%d", ptm->tm_hour, ptm->tm_min, ptm->tm_sec );

	free(ptm);

	// confirm if inited
	if ( _tmpl == "" ) {
		this->error_log( 0, "Error: Templet not initialized" );
		return;
	}
	
	// parse init
	_loop = "";
	_cursor = 0;
	_lines = 0;
	
	size_t lastpos = 0;
	size_t currpos = 0;

	// for parse_script()
	string exp;
	int type;
	int parsed;
	
	// search TMPL_BEGIN in tmpl
	while( (currpos=tmpl.find(TMPL_BEGIN,lastpos)) != tmpl.npos ) {
		// output html before TMPL_BEGIN
		output << tmpl.substr( lastpos, currpos-lastpos );
		
		// log current position
		if ( _debug == TMPL_OUTPUT_DEBUG ) {
			String orightml = tmpl.substr( lastpos, currpos-lastpos );
			_lines += orightml.count( TMPL_NEWLINE );
		}
		
		// get script content between TMPL_BEGIN and TMPL_END
		parsed = this->parse_script( tmpl, currpos, exp, type );
		
		if ( parsed < 0 ) {
			// can't find TMPL_END
			lastpos = currpos;
			this->error_log( _lines, "Error: Can't find TMPL_END" );
			break;
		}
		
		// parse by script type
		switch ( type ) {
			case TMPL_S_VALUE:
				// replace
			case TMPL_S_DATE:
				// replace with date
			case TMPL_S_TIME:
				// replace with time
			case TMPL_S_SPACE:
				// replace with space char
			case TMPL_S_BLANK:
				// replace with blank string
				output << this->exp_value( exp );
				break;

			case TMPL_S_IF:
				// condition replace
				parsed = this->parse_if( tmpl.substr(currpos), output, true, exp, parsed );
				break;
				
			case TMPL_S_LOOP:
				// cycle replace
				parsed = this->parse_loop( tmpl.substr(currpos), output, true, exp, parsed );
				// restore loop status
				_loop = "";
				_cursor = 0;
				break;

			case TMPL_S_UNKNOWN:
				// unknown script, maybe html code
				this->error_log( _lines, "Warning: Unknown script, in parse()" );
				
				// for syntax error
				size_t backlen;
				if ( (backlen=exp.find(TMPL_BEGIN)) != exp.npos )
					parsed = backlen+TMPL_BEGIN_LEN;
					
				output << tmpl.substr( currpos, parsed );
				break;
				
			default:
				// syntax error
				this->error_log( _lines, "Error: Unexpected script, in parse()" );
		}

		// location to next position
		lastpos = currpos + parsed;
	}
	
	// output tail html
	output << tmpl.substr( lastpos );
}

/// ������������ʽ�Ƿ����
/// \param exp ����Ϊ�������ʽ,
/// �����ʽΪ�ַ���,��ֵ��Ϊ""���Ҳ�Ϊ"0"ʱ����true,���򷵻�false,
/// ��Ϊ�Ƚϱ��ʽ,��������true,���򷵻�false
/// \retval true �������ʽ����
/// \retval false �������ʽ������
bool Template::compare( const string &exp ) {
	// read compare type
	// supported: ==,!=,<=,<,>=,>
	string cmpop;
	size_t oppos;
	tmpl_cmptype optype;
	
	if ( (oppos=exp.find(TMPL_EQ)) != exp.npos ) {
		// ==
		cmpop = TMPL_EQ;
		optype = TMPL_C_EQ;
	
	} else if ( (oppos=exp.find(TMPL_NE)) != exp.npos ) {
		// !=
		cmpop = TMPL_NE;
		optype = TMPL_C_NE;
	
	} else if ( (oppos=exp.find(TMPL_LE)) != exp.npos ) {
		// <=
		cmpop = TMPL_LE;
		optype = TMPL_C_LE;
	
	} else if ( (oppos=exp.find(TMPL_LT)) != exp.npos ) {
		// <
		cmpop = TMPL_LT;
		optype = TMPL_C_LT;
	
	} else if ( (oppos=exp.find(TMPL_GE)) != exp.npos ) {
		// >=
		cmpop = TMPL_GE;
		optype = TMPL_C_GE;
	
	} else if ( (oppos=exp.find(TMPL_GT)) != exp.npos ) {
		// >
		cmpop = TMPL_GT;
		optype = TMPL_C_GT;
	
	} else {
		// read value, compare and return
		string val = this->exp_value( exp );
		if ( val!="" && val!="0" )
			return true;
		else
			return false;
	}

	// split exp by compare operator
	String lexp = exp.substr( 0, oppos );
	String rexp = exp.substr( oppos+cmpop.length() );
	
	// read value
	lexp.trim(); rexp.trim();
	lexp = this->exp_value( lexp );
	rexp = this->exp_value( rexp );

	// compare
	int cmp;
	if ( lexp.isnum() && rexp.isnum() ) {
		int lv = atoi( lexp.c_str() );
		int rv = atoi( rexp.c_str() );
		cmp = ( lv>rv ) ? 1 : ( lv==rv ) ? 0 : -1;
	} else {
		cmp = strcmp( lexp.c_str(), rexp.c_str() );
	}

	// return
	switch ( optype ) {
		case TMPL_C_EQ:
			return ( cmp==0 ) ? true : false;
		case TMPL_C_NE:
			return ( cmp!=0 ) ? true : false;
		case TMPL_C_LE:
			return ( cmp<=0 ) ? true : false;
		case TMPL_C_LT:
			return ( cmp<0 ) ? true : false;
		case TMPL_C_GE:
			return ( cmp>=0 ) ? true : false;
		case TMPL_C_GT:
			return ( cmp>0 ) ? true : false;
		default:
			return false;
	}
}

/// ��������Ƿ����	
/// \param exp ����Ϊ�������ʽ�������
/// \retval true �������ʽ����
/// \retval false �������ʽ������
bool Template::check_if( const string &exp ) {
	tmpl_logictype exp_type = TMPL_L_NONE;
	String exps;

	// check expression type
	if ( strncmp(exp.c_str(),TMPL_AND,TMPL_AND_LEN) == 0 ) {
		exp_type = TMPL_L_AND;
		exps = exp.substr( TMPL_AND_LEN );
	} else if ( strncmp(exp.c_str(),TMPL_OR,TMPL_OR_LEN) == 0 ) {
		exp_type = TMPL_L_OR;
		exps = exp.substr( TMPL_OR_LEN );
	}

	// none logic expression
	if ( exp_type == TMPL_L_NONE )
		return this->compare( exp );

	// check TMPL_SUBBEGIN/TMPL_SUBEND
	exps.trim();
	size_t explen = exps.length();
	if ( exps.substr(0,TMPL_SUBBEGIN_LEN)!=TMPL_SUBBEGIN ||
		 exps.substr(explen-TMPL_SUBEND_LEN)!=TMPL_SUBEND ) {
		this->error_log( _lines, "Warning: Maybe wrong TMPL_AND or TMPL_OR script" );
		return this->compare( exp );
	}
		
	// split expressions list
	exps = exps.substr( TMPL_SUBBEGIN_LEN, explen-TMPL_SUBBEGIN_LEN-TMPL_SUBEND_LEN );
	vector<String> explist = exps.split( TMPL_SPLIT );

	// judge
	if ( exp_type == TMPL_L_AND ) {
		// TMPL_AND
		for ( unsigned int i=0; i<explist.size(); i++ ) {
			explist[i].trim();
			if ( !this->compare(explist[i]) )
				return false;
		}
		return true;
		
	} else {
		// TMPL_OR
		for ( size_t i=0; i<explist.size(); i++ ) {
			explist[i].trim();
			if ( this->compare(explist[i]) )
				return true;
		}
		return false;
	}
	
	return false; // for warning
}

/// ������������ģ��
/// \param tmpl ģ���ַ���
/// \param output ���������������
/// \param parent_state ���øú���ʱ������״̬
/// \param parsed_exp �ѷ����������ű����ʽ
/// \param parsed_length �ѷ����������ű����ʽ����
/// \return ����ֵΪ���η������ַ�������
size_t Template::parse_if( const string &tmpl, ostream &output, 
	const bool parent_state, const string &parsed_exp, const int parsed_length ) 
{
	// parsed length
	size_t length = parsed_length;
		
	// check status
	bool status;
	bool effected;
	if ( parent_state == false ) {
		status = false;
		effected = true;
	} else if ( this->check_if(parsed_exp) ) {
		status = true;
		effected = true;
	} else {
		status = false;
		effected = false;
	}
		
	// parse
	size_t lastpos = parsed_length;
	size_t currpos = parsed_length;

	// for parse_script()
	string exp;
	int type;
	int parsed;
	
	while( (currpos=tmpl.find(TMPL_BEGIN,lastpos)) != tmpl.npos ) {
		// output html before TMPL_BEGIN if status valid
		if ( status )
			output << tmpl.substr( lastpos, currpos-lastpos );
		length += ( currpos-lastpos );
		
		// log current position
		if ( _debug == TMPL_OUTPUT_DEBUG ) {
			String orightml = tmpl.substr( lastpos, currpos-lastpos );
			_lines += orightml.count( TMPL_NEWLINE );
		}
		
		// get script content between TMPL_BEGIN and TMPL_END
		parsed = this->parse_script( tmpl, currpos, exp, type );
		
		if ( parsed < 0 ) {
			// can't find TMPL_END
			this->error_log( _lines, "Error: Can't find TMPL_END" );
			lastpos = currpos;
			break;
		}
		
		// parse by script type
		switch ( type ) {
			case TMPL_S_VALUE:
				// replace if status is true
			case TMPL_S_LOOPVALUE:
				// replace with loop value if status is true
			case TMPL_S_CURSOR:
				// replace with cursor
			case TMPL_S_ROWS:
				// replace with rows
			case TMPL_S_DATE:
				// replace with date
			case TMPL_S_TIME:
				// replace with time
			case TMPL_S_SPACE:
				// replace with space char
			case TMPL_S_BLANK:
				// replace with blank string
				if ( status )
					output << this->exp_value( exp );
				break;

			case TMPL_S_ELSIF:
				// check and set status
				if ( effected ) {
					// something effected before
					status = false;
				} else if ( this->check_if(exp) ) {
					// nothing effected and status is true now
					status = true;
					effected = true;
				} else {
					// nothing effected, status is false now
					status = false;
				}
				break;

			case TMPL_S_ELSE:
				// check and set status
				if ( effected ) {
					// something effected before
					status = false;
				} else {
					// nothing effected, then status is true
					status = true;
					effected = true;
				}
				break;

			case TMPL_S_ENDIF:
				// parsed length
				length += parsed;
				// exit function
				return length;
					
			case TMPL_S_IF:
				// sub condition replace
				parsed = this->parse_if( tmpl.substr(currpos), output, status, exp, parsed );
				break;
				
			case TMPL_S_LOOP: { // make compiler happy
				// backup current loop status
				string parent_loop = _loop;
				int parent_cursor = _cursor;
				
				// sub cycle replace
				parsed = this->parse_loop( tmpl.substr(currpos), output, status, exp, parsed );
				
				// restore loop status
				_loop = parent_loop;
				_cursor = parent_cursor;
				_loops[_loop].cursor = _cursor;
				}
				break;

			case TMPL_S_UNKNOWN:
				// unknown script, maybe html code
				this->error_log( _lines, "Warning: Unknown script, in parse_if()" );

				// for syntax error
				size_t backlen;
				if ( (backlen=exp.find(TMPL_BEGIN)) != exp.npos )
					parsed = backlen+TMPL_BEGIN_LEN;

				if ( status )
					output << tmpl.substr( currpos, parsed );
				break;

			default:
				// syntax error
				error_log( _lines, "Error: Unexpected script, in parse_if()" );
		}

		// parsed length
		length += parsed;
		// location to next position
		lastpos = currpos + parsed;
	}
	
	// can not find TMPL_ENDIF
	this->error_log( _lines, "Error: Can't find TMPL_ENDIF" );

	// output tail html
	if ( status )
		output << tmpl.substr( lastpos );
	length += ( tmpl.size()-lastpos );
	
	return length;
}

/// ���ѭ������Ƿ���Ч
/// \param loop ѭ��ѭ������
/// \retval true ѭ���Ѷ���
/// \retval false δ����
bool Template::check_loop( const string &loopname ) {
	string loop = this->exp_value( loopname );
	
	if ( _loops.find(loop) != _loops.end() && _loops[loop].rows > 0 ) {
		return true;
	} else {
		this->error_log( _lines, "Warning: loop " + loopname + " \""+loop+
			"\" not defined or not set data" );
		return false;
	}
}

/// ����ѭ����ָ��λ���ֶε�ֵ
/// \param fleid ѭ�������ֶ���
/// \return ����ȡ�ɹ�����ֵ�ַ���,���򷵻ؿ��ַ���
string Template::loop_value( const string &field ) {
	// get loop info
	size_t pos = field.find( TMPL_LOOPSCOPE );
	if ( pos != field.npos ) {
		string loop_name = this->exp_value( field.substr(pos+TMPL_LOOPSCOPE_LEN) );
		string field_name = field.substr( 0, pos );
		int cursor = _loops[loop_name].cursor;

		// return value
		int col = this->field_pos( loop_name, field_name );
		if ( col!=-1 && cursor<_loops[loop_name].rows )
			return _loops[loop_name].datas[cursor][col];
		else
			return string( "" );
	} else {
		// return value
		int col = this->field_pos( _loop, field );
		if ( col!=-1 && _cursor<_loops[_loop].rows )
			return _loops[_loop].datas[_cursor][col];
		else
			return string( "" );
	}
}

/// ����ѭ������ģ��
/// \param tmpl ģ���ַ���
/// \param output ���������������
/// \param parent_state ���øú���ʱ������״̬
/// \param parsed_exp �ѷ�����ѭ���ű����ʽ
/// \param parsed_length �ѷ�����ѭ���ű����ʽ����
/// \return ����ֵΪ���η������ַ�������
size_t Template::parse_loop( const string &tmpl, ostream &output, 
	const bool parent_state, const string &parsed_exp, const int parsed_length ) 
{
	// parsed length
	size_t length = parsed_length;
		
	// check status
	bool status;
	if ( parent_state == false )
		status = false;
	else if ( this->check_loop(parsed_exp) )
		status = true;
	else
		status = false;
		
	// init
	bool cycled = false;
	int cursor = 0;
	size_t start_pos = parsed_length;
	size_t start_len = parsed_length;
	size_t lastpos = parsed_length;
	size_t currpos = parsed_length;
	
	// current loop name
	string loop = this->exp_value( parsed_exp );
	_loop = loop;
	_loops[_loop].cursor = 0;

	// for parse_script()		
	string exp;
	int type;
	int parsed;

	while( (currpos=tmpl.find(TMPL_BEGIN,lastpos)) != tmpl.npos ) {
		// output html before TMPL_BEGIN if status valid
		if ( status )
			output << tmpl.substr( lastpos, currpos-lastpos );
		length += ( currpos-lastpos );
		
		// log current position
		if ( !cycled && _debug==TMPL_OUTPUT_DEBUG ) {
			String orightml = tmpl.substr( lastpos, currpos-lastpos );
			_lines += orightml.count( TMPL_NEWLINE );
		}
		
		// get script content between TMPL_BEGIN and TMPL_END
		parsed = this->parse_script( tmpl, currpos, exp, type );
		
		if ( parsed < 0 ) {
			// can't find TMPL_END
			if ( !cycled )
				this->error_log( _lines, "Error: Can't find TMPL_END" );
			lastpos = currpos;
			break;
		}
		
		// current loop cursor
		_cursor = cursor;

		// parse by script type
		switch ( type ) {
			case TMPL_S_VALUE:
				// replace
			case TMPL_S_LOOPVALUE:
				// replace with loop value in loop
			case TMPL_S_CURSOR:
				// replace with cursor
			case TMPL_S_ROWS:
				// replace with rows				
			case TMPL_S_DATE:
				// replace with date
			case TMPL_S_TIME:
				// replace with time
			case TMPL_S_SPACE:
				// replace with space char
			case TMPL_S_BLANK:
				// replace with blank string
				if ( status )           
					output << this->exp_value( exp );
				break;

			case TMPL_S_ENDLOOP:
				// at the end of this cycle
				++cursor; // data cursor
				_loops[_loop].cursor = cursor;
				if ( status && cursor<_loops[loop].rows ) {
					// next cycle
					length = start_len;		// reset parsed length
					lastpos = start_pos;	// reset start position
					cycled = true;			// do not add current position value
					continue;
				} else {
					// parsed length
					length += parsed;
					// exit function
					return length;
				}
					
			case TMPL_S_IF:
				// sub condition replace
				parsed = this->parse_if( tmpl.substr(currpos), output, status, exp, parsed );
				break;
				
			case TMPL_S_LOOP:
				// sub cycle replace
				parsed = this->parse_loop( tmpl.substr(currpos), output, status, exp, parsed );
				// restore loop status
				_loop = loop;
				_cursor = cursor;
				_loops[_loop].cursor = _cursor;
				break;

			case TMPL_S_UNKNOWN:
				// unknown script, maybe html code
				if ( !cycled )
					this->error_log( _lines, "Warning: Unknown script, in parse_loop()" );

				// for syntax error
				size_t backlen;
				if ( (backlen=exp.find(TMPL_BEGIN)) != exp.npos )
					parsed = backlen+TMPL_BEGIN_LEN;

				if ( status )
					output << tmpl.substr( currpos, parsed );
				break;

			default:
				// syntax error
				if ( !cycled )
					this->error_log( _lines, "Error: Unexpected script, in parse_loop()" );
		}

		// parsed length
		length += parsed;
		// location to next position
		lastpos = currpos + parsed;
	}
	
	// can not find TMPL_ENDLOOP
	this->error_log( _lines, "Error: Can't find TMPL_ENDLOOP" );

	// output tail html
	if ( status )
		output << tmpl.substr( lastpos );
	length += ( tmpl.size()-lastpos );

	return length;
}

////////////////////////////////////////////////////////////////////////////
// output functions

/// ģ����������¼
/// \param pos ģ�������λ��
/// \param error ��������
void Template::error_log( const size_t lines, const string &error ) {
	if ( error != "" )
		_errlog.insert( multimap<int,string>::value_type(lines,error) );
}

/// ����ģ�������¼
/// \param output ���������������
void Template::parse_log( ostream &output ) {
	output << endl;
	output << "<!-- Generated by waTemplate " << _date << " " << _time << endl
		<< "  Templet source: " << _tmplfile << endl
		<< "  Loops: " << _loops.size() << endl;

	for ( map<string,tmpl_loop>::const_iterator i=_loops.begin(); i!=_loops.end(); ++i ) {
		if ( i->first != "" ) {
			output << "    Loop " << i->first
				<< "\t\t" << (i->second).cursor << " rows" << endl;
		}
	}

	output << "  Errors: " << _errlog.size() << endl;
	for ( multimap<int,string>::const_iterator i=_errlog.begin(); i!=_errlog.end(); ++i ) {
		output << "    Line " << i->first+1
			<< "\t\t" << i->second << endl;
	}
			   
	output << "-->";
	_errlog.clear();
}

/// ����HTML�ַ���
/// \return ����ģ�����������
string Template::html() {
	ostringstream result;
	this->parse( _tmpl, result );
	result << ends;
	return result.str();
}

/// ���HTML��stdout
/// \param mode �Ƿ����������Ϣ
/// - Template::TMPL_OUTPUT_DEBUG ���������Ϣ
/// - Template::TMPL_OUTPUT_RELEASE �����������Ϣ
/// - Ĭ��Ϊ�����������Ϣ
void Template::print( const output_mode mode ) {
	_debug = mode;
	this->parse( _tmpl, std::cout );
	if ( _debug == TMPL_OUTPUT_DEBUG ) 
		this->parse_log( std::cout );
}

/// ���HTML���ļ�
/// \param file ����ļ���
/// \param mode �Ƿ����������Ϣ
/// - Template::TMPL_OUTPUT_DEBUG ���������Ϣ
/// - Template::TMPL_OUTPUT_RELEASE �����������Ϣ
/// - Ĭ��Ϊ�����������Ϣ
/// \param permission �ļ����Բ�����Ĭ��Ϊ0666
/// \retval true �ļ�����ɹ�
/// \retval false ʧ��
bool Template::print( const string &file, const output_mode mode) 
{
	ofstream outfile( file.c_str(), ios::trunc|ios::out );
	if ( outfile ) {
		// parse
		_debug = mode;
		this->parse( _tmpl, outfile );
		if ( _debug == TMPL_OUTPUT_DEBUG ) 
			this->parse_log( outfile );
		outfile.close();

		return true;
	}
	
	return false;
}

} // namespace


