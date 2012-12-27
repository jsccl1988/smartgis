/// \file waString.cpp
/// webapp::String��ʵ���ļ�

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <set>
#include <fstream>
#include "waString.h"

/// Web Application Library namaspace
namespace webapp {
	
////////////////////////////////////////////////////////////////////////////////	

/// \defgroup waString waString���ȫ�ֺ���

/// \ingroup waString
/// \fn string itos( const long i, const ios::fmtflags base )
/// long intת��Ϊstring
/// \param i long int����int
/// \param base ת�����Ʋ���,��ѡ
/// - ios::dec 10����
/// - ios::oct 8����
/// - ios::hex 16����
/// - Ĭ��Ϊ10����
/// \return ���ؽ��string,ת��ʧ�ܷ���"0"
string itos( const long i, const ios::fmtflags base ) {
	char format[] = "%ld";
	if ( base == ios::oct )
		strcpy( format, "%o" );
	else if ( base == ios::hex )
		strcpy( format, "%X" );

	// try
	int strlen = 32;
	char *buf = new char[strlen];
	memset( buf, 0, strlen );
	int size = snprintf( buf, strlen, format, i );
	if ( size >= strlen ) {
		// retry
		delete[] buf;
		buf = new char[size+1];
		memset( buf, 0, size+1 );
		snprintf( buf, size+1, format, i );
	}
	
	string result( buf );
	delete[] buf;
	return result;
}

/// \ingroup waString
/// \fn long stoi( const string &s, const ios::fmtflags base )
/// stringת��Ϊlong int
/// \param s string
/// \param base ת�����Ʋ���,��ѡ
/// - ios::dec 10����
/// - ios::oct 8����
/// - ios::hex 16����
/// - Ĭ��Ϊ10����
/// \return ���ؽ��long int,ת��ʧ�ܷ���0
long stoi( const string &s, const ios::fmtflags base ) {
	int ibase = 10;
	char *ep;
	
	if ( base == ios::hex )
		ibase = 16;
	else if ( base == ios::oct )
		ibase = 8;
		
	String ps = s;
	ps.trim();
	return strtol( ps.c_str(), &ep, ibase );
}

/// \ingroup waString
/// \fn string ftos( const doule f, const int ndigit )
/// doubleת��Ϊstring
/// \param f double
/// \param ndigit С�������λ��,Ĭ��Ϊ2
/// \return ת���ɹ�����string,���򷵻�"0"
string ftos( const double f, const int ndigit ) {
	int fmtlen = 10;
	int strlen = 64;
	int buflen;
	
	// create format string
	char *fmt = new char[fmtlen];
	memset( fmt, 0, fmtlen );
	buflen = snprintf( fmt, fmtlen, "%%.%df", ndigit );
	if ( buflen >= fmtlen ) {
		delete[] fmt;
		fmt = new char[buflen+1];
		memset( fmt, 0, buflen+1 );
		snprintf( fmt, buflen+1, "%%.%df", ndigit );
	}
	
	// convert
	char *str = new char[strlen];
	memset( str, 0, strlen );
	buflen = snprintf( str, strlen, fmt, f );
	if ( buflen >= strlen ) {
		delete[] str;
		str = new char[buflen+1];
		memset( str, 0, buflen+1 );
		snprintf( str, buflen+1, fmt, f );
	}

	string s = str;
	delete[] fmt;
	delete[] str;
	return s;
}

/// \ingroup waString
/// \fn double stof( const string &s )
/// stringת��Ϊdouble
/// \param s string
/// \return ת���ɹ�����double,���򷵻�0
double stof( const string &s ) {
	char *ep;
	return strtod( s.c_str(), &ep );
}

/// \ingroup waString
/// \fn bool isgbk( const unsigned char c1, const unsigned char c2 )
/// �ж�һ��˫�ֽ��ַ��Ƿ���GBK���뺺��
/// \param c1 ˫�ֽ��ַ�1
/// \param c2 ˫�ֽ��ַ�2
/// \retval true ��
/// \retval false ��
bool isgbk( const unsigned char c1, const unsigned char c2 ) {
	if ( (c1>=0x81&&c1<=0xFE) && ((c2>=0x40&&c2<=0x7E)||(c2>=0xA1&&c2<=0xFE)) )
		return true;
	else
		return false;
}

/// \ingroup waString
/// \fn string va_sprintf( va_list ap, const string &format )
/// �ɱ�����ַ�����ʽ������va_start()��va_end()�����ʹ��
/// \param format �ַ�����ʽ
/// \param ap �ɱ�����б�
/// \return ��ʽ���ַ������
string va_sprintf( va_list ap, const string &format ) {
	int strlen = 256;
	char *buf = new char[strlen];
	memset( buf, 0, strlen );

	int size = vsnprintf( buf, strlen, format.c_str(), ap );
	if ( size >= strlen ) {
		delete[] buf; buf = NULL;
		buf = new char[size+1];
		memset( buf, 0, size+1 );
		vsnprintf( buf, size+1, format.c_str(), ap );
	}
	
	string result = buf;
	delete[] buf; buf = NULL;
	return result;
}

/// \ingroup waString
/// \fn string va_str( const char *format, ... )
/// ��ʽ���ַ��������أ��������������׼sprintf()������ȫ��ͬ
/// \return ��ʽ���ַ������
string va_str( const char *format, ... ) {
	va_list ap;
	va_start( ap, format );
	string result = va_sprintf( ap, format );
	va_end( ap );
	return result;
}

/// ���� char* �ͽ���������߱������ delete[] �ͷ��������ڴ�
/// \return char*�������ݽ��
char* String::c_char() const {
	size_t size = this->length();
	char *buf = new char[ size + 1 ];
	memset( buf, 0, size );
	strncpy( buf, this->c_str(), size );
	return buf;
}

/// �����ַ�������GBK��������һ���ַ�
/// \return �ַ�����
string::size_type String::w_length() const {
	size_t wlen = 0;
	size_t len = this->length();
	
	for ( size_t i=0; i<len; ++i )  {
		if ( i<(len-1) && isgbk(this->at(i),this->at(i+1)) ) 
			++i;
		++wlen;
	}
	
	return wlen;
}

/// ��ȡ���ַ���,������ְ������
/// ����ȡ�������βΪ���������ɾ��,ɾ��������ֺ�������Ϊ���ַ���,
/// �ú��������ڽ�ȡʱ��һ���������ַֿ�,���ַ�����ԭ�еĲ����������ַ���������
/// \param pos ��ʼλ��,Ĭ��Ϊ0,���ֽڼ�����ʽ
/// \param n Ҫ��ȡ���ַ�������,Ĭ��Ϊ��ĩβ,���ֽڼ�����ʽ
/// \return ����ȡ���ַ���
String String::w_substr( const string::size_type pos, 
	const string::size_type n ) const 
{
    size_t len = this->length();
    if ( len<=0 || pos>=len || n<=0 )
        return String( "" );

    size_t from = pos;
    size_t to = min( pos+n, len );

    // location
    for ( size_t i=0; i<to; ++i ) {
        if ( (i+1)<len && isgbk(this->at(i),this->at(i+1)) ) {
            if ( i == from-1 )
                ++from;
            else if ( i == to-1 )
                --to;
            ++i;
        }
    }

    // substr
    if ( to > from )
        return String( this->substr(from,to-from) );
    else
        return String( "" );
}

/// ������հ��ַ�
/// \param blank Ҫ���˵��Ŀհ��ַ��б�,Ĭ��Ϊwebapp::BLANK_CHARS
void String::trim_left( const string &blank ) {
	while ( this->length()>0 && blank.find(this->at(0))!=npos )
		this->erase( 0, 1 );
}

/// ����Ҳ�հ��ַ�
/// \param blank Ҫ���˵��Ŀհ��ַ��б�,Ĭ��Ϊwebapp::BLANK_CHARS
void String::trim_right( const string &blank ) {
	while ( this->length()>0 && blank.find(this->at(length()-1))!=npos )
		erase( this->length()-1, 1 );
}

/// �������հ��ַ�
/// \param blank Ҫ���˵��Ŀհ��ַ��б�,Ĭ��Ϊwebapp::BLANK_CHARS
void String::trim( const string &blank ) {
	this->trim_left( blank );
	this->trim_right( blank );
}

/// ����߽�ȡָ�������Ӵ�
/// \param n Ҫ��ȡ���ַ�������,�����ȳ����򷵻�ԭ�ַ���
/// \return ����ȡ���ַ���
String String::left( const string::size_type n ) const {
	size_t len = this->length();
	len = ( n>len ) ? len : n;
	return String( this->substr(0,len) );
}

/// ���м��ȡָ�������Ӵ�
/// \param pos ��ʼ��ȡ��λ��
/// \param n Ҫ��ȡ���ַ�������,�����ȳ����򷵻�ԭ�ַ���,Ĭ��Ϊ��ĩβ
/// \return ����ȡ���ַ���
String String::mid( const string::size_type pos, 
	const string::size_type n ) const 
{
	if ( pos > this->length() )	
		return String( "" );
	return String( this->substr(pos,n) );
}		

/// ���ұ߽�ȡָ�������Ӵ�
/// \param n Ҫ��ȡ���ַ�������,�����ȳ����򷵻�ԭ�ַ���
/// \return ����ȡ���ַ���
String String::right( const string::size_type n ) const {
	size_t len = this->length();
	len = ( n>len )? n : len;
	return String( this->substr(len-n,n) );
}		

/// �����ַ�������
/// \param n ���ַ�������,��С�ڵ�ǰ������ض�,�����ڵ�ǰ�����򲹳�հ��ַ�
void String::resize( const string::size_type n ) {
	size_t len = this->length();
	if ( n < len ) {
		*this = this->substr( 0, n );
	} else if ( n > len ) {
		for ( size_t i=0; i<(n-len); ++i )
			this->append( " " );
	}
}

/// ͳ��ָ���Ӵ����ֵĴ���
/// \param str Ҫ���ҵ��Ӵ�
/// \return �Ӵ����ظ����ֵĴ���
int String::count( const string &str ) const {
	size_t pos = 0;
	size_t count = 0;
	size_t step = str.length();
	
	while( (pos=this->find(str,pos)) != npos ) {
		++count;
		pos += step;
	}
	
	return count;
}
	
/// ���ݷָ���ָ��ַ���
/// \param tag �ָ����ַ���
/// \param limit �ָ��������,Ĭ��Ϊ0��������
/// \param mode �������ģʽ,��ѡ
/// - String::SPLIT_IGNORE_BLANK ������������ָ��������ؽ���������ֶ�
/// - String::SPLIT_KEEP_BLANK ��������������ָ��������ؽ���������ֶ�
/// - Ĭ��ΪString::SPLIT_IGNORE_BLANK
/// \return �ָ����ַ������� vector<String>
vector<String> String::split( const string &tag, const int limit, 
	const split_mode mode ) const 
{
	string src = *this;
	string curelm;
	vector<String> list;
	int count = 0;

	list.clear();
	if ( tag.length()>0 && src.length()>0 ) {
		// how to split
		size_t pos = src.find( tag );
		
		while ( pos < src.length() ) {
			curelm = src.substr( 0, pos );
			
			// is keep blank
			if ( !(mode==SPLIT_IGNORE_BLANK && curelm.length()==0) ) {
				list.push_back( curelm );
				++count;
			}

			// split
			src = src.substr( pos + tag.length() );
			pos = src.find( tag );
			
			if ( limit>0 && count>=limit )
				break;
		}
		
		// is keep blank
		if ( !(mode==SPLIT_IGNORE_BLANK && src.length()==0) )
			list.push_back( src );
	}
	
	return list;
}

/// ת���ַ���ΪMAP�ṹ(map<string,string>)
/// \param itemtag ���ʽ֮��ķָ���,Ĭ��Ϊ"&"
/// \param exptag ���ʽ�б����������ֵ֮��ķָ���,Ĭ��Ϊ"="
/// \return ת����� map<string,string>
map<string,string> String::tomap( const string &itemtag, 
	const string &exptag ) const 
{
	map<string,string> hashmap;
	
	if ( itemtag!="" && exptag!="" ) {
		vector<String> items = this->split( itemtag );
		string name, value;
		size_t pos;
		for ( size_t i=0; i< items.size(); ++i ) {           
			pos = items[i].find( exptag );
			name = (items[i]).substr( 0, pos );
			value = (items[i]).substr( pos+exptag.length() );
			if ( name != "" )
				hashmap.insert( map<string,string>::value_type(name,value) );
		}
	}
	
	return hashmap;
}

/// ����ַ���,��split()�෴
/// \param strings �ַ�������
/// \param tag ��Ϸָ���
void String::join( const vector<string> &strings, const string &tag ) {
	if ( strings.size() > 0 ) {
		this->erase();
		*this = strings[0];
		
		for ( size_t i=1; i<strings.size(); ++i )
			*this += ( tag + strings[i] );
	}
}
// for vector<String>
void String::join( const vector<String> &strings, const string &tag ) {
	if ( strings.size() > 0 ) {
		this->erase();
		*this = strings[0];
		
		for ( size_t i=1; i<strings.size(); ++i )
			*this += ( tag + strings[i] );
	}
}

/// ��ʽ����ֵ
/// �������������׼sprintf()������ȫ��ͬ
/// \retval true ִ�гɹ�
/// \retval false ʧ��
bool String::sprintf( const char *format, ... ) {
	va_list ap;
	va_start( ap, format );
	*this = va_sprintf( ap, format );
	va_end( ap );
	return true;
}

/// �滻
/// �ú���������string::replace()
/// \param oldstr ���滻�����ַ���
/// \param newstr �����滻���ַ��������ַ���
/// \retval 1 �滻�ɹ�
/// \retval 0 ʧ��
int String::replace( const string &oldstr, const string &newstr ) {
	size_t pos = 0;
	if ( oldstr!="" && (pos=this->find(oldstr))!=npos ) {
		string::replace( pos, oldstr.length(), newstr );
		return 1;
	}
	return 0;
}

/// ȫ���滻
/// \param oldstr ���滻�����ַ���
/// \param newstr �����滻���ַ��������ַ���
/// \return ִ���滻�Ĵ���
int String::replace_all( const string &oldstr, const string &newstr ) {
	if ( oldstr == "" )
		return 0;
	
	int i = 0;
	size_t pos = 0;
	size_t curpos = 0;
	while ( (pos=this->find(oldstr,curpos)) != npos ) {
		string::replace( pos, oldstr.length(), newstr );
		curpos = pos + newstr.length();
		++i;
	}
	return i;
}

/// ת��Ϊ��д��ĸ
void String::upper() {
	for( size_t i=0; i<this->length(); i++ )
		(*this)[i] = toupper( (*this)[i] );
}

/// ת��ΪСд��ĸ
void String::lower() {
	for( size_t i=0; i<this->length(); i++ )
		(*this)[i] = tolower( (*this)[i] );
}

/// �ַ����Ƿ���ȫ���������
/// \retval true ��
/// \retval false ��
bool String::isnum() const {
	if ( this->length() == 0 )
		return false;
		
	for ( size_t i=0; i<this->length(); ++i ) {
		if ( !isdigit((*this)[i]) )
			return false;
	}
	return true;
}

/// ��ȡ�ļ����ַ���
/// \param filename Ҫ��ȡ���ļ�����·������
/// \retval true ��ȡ�ɹ�
/// \retval false ʧ��
bool String::load_file( const string &filename ) {
	FILE *fp = fopen( filename.c_str(), "rb" );
	if ( fp == NULL ) return false;
	
	// read file size
	fseek( fp, 0, SEEK_END );
	int bufsize = ftell( fp );
	rewind( fp );
	
	char *buf = new char[bufsize+1];
	memset( buf, 0, bufsize+1 );
	fread( buf, 1, bufsize, fp );
	fclose( fp );
	*this = buf;
	delete[] buf;
	return true;
}

/// �����ַ������ļ�
/// \param filename Ҫд����ļ�·������
/// \param mode д�뷽ʽ,Ĭ��Ϊios::trunc|ios::out
/// \param permission �ļ����Բ�����Ĭ��Ϊ0666
/// \retval true д��ɹ�
/// \retval false ʧ��
bool String::save_file( const string &filename, const ios::openmode mode) const 
{
	ofstream outfile( filename.c_str(), mode );
	if ( outfile ) {
		outfile << *this;
		outfile.close();

		return true;
	}
	return false;
}

} // namespace

