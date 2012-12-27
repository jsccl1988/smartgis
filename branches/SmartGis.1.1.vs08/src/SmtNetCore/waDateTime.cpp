/// \file waDateTime.cpp
/// webapp::DateTime��ʵ���ļ�

#include <cstdio>
#include <string.h>
#include <climits>
#include "waDateTime.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {
	
/// �Ե�ǰʱ�����ö���
void DateTime::set() {
	_time = ::time( 0 );
	struct tm  *ptm = NULL;
	ptm = localtime(&_time);
	memcpy(&_tm,ptm,sizeof(tm));
	free(ptm);
}

/// �� time_t �������ö���
/// \param tt time_t���Ͳ���
void DateTime::set( const time_t &tt ) {
	time_t _tt = tt;
	if ( tt < 0 ) _tt = 0;
	if ( tt > LONG_MAX ) _tt = LONG_MAX;
	
	_time = _tt;
	struct tm  *ptm = NULL;
	ptm = localtime(&_time);
	memcpy(&_tm,ptm,sizeof(tm));
	free(ptm);
}

/// ��ָ��ʱ�����ö���
/// ������������Ч����ʱ��,������Ϊϵͳ��ʼʱ�䣨1970/1/1��
/// ����������ʱ�䲻����,������Ϊ˳����Чʱ�䣨������2/29��Ϊ3/1��
/// \param year ��
/// \param mon ��
/// \param mday ��
/// \param hour ʱ,Ĭ��Ϊ0
/// \param min ��,Ĭ��Ϊ0
/// \param src ��,Ĭ��Ϊ0
void DateTime::set( const int year, const int mon, const int mday, 
	const int hour, const int min, const int sec ) 
{
	int _year = year;
	int _mon = mon;
	int _mday = mday;
	int _hour = hour;
	int _min = min;
	int _sec = sec;

	// confirm
	if ( _year<1 || _year>2038 )	_year = 1970;
	if ( _mon<1  || _mon>12 ) 		_mon  = 1;
	if ( _mday<1 || _mday>31 )		_mday = 1;
	if ( _hour<0 || _hour>23 )		_hour = 0;
	if ( _min<0  || _min>59 ) 		_min  = 0;
	if ( _sec<0  || _sec>59 ) 		_sec  = 0;
	
	_tm.tm_year = _year-1900;
	_tm.tm_mon = _mon-1;
	_tm.tm_mday = _mday;
	_tm.tm_hour = _hour;
	_tm.tm_min = _min;
	_tm.tm_sec = _sec;
	_tm.tm_isdst = -1;
	_time = mktime( &_tm );
}

/// �� tm �ṹ�������ö���
/// \param st struct tm���Ͳ���
void DateTime::set( const tm &st ) {
	this->set( st.tm_year+1900, st.tm_mon+1, st.tm_mday,
		st.tm_hour, st.tm_min, st.tm_sec );
}

/// �� DateTime �������ö���
/// \param date Date���Ͳ���
void DateTime::set( const DateTime &date ) {
	this->set( date.value() );
}

/// ��"YYYY-MM-DD HH:MM:SS"��ʽ�ַ������ö���
/// ���ַ�����ʽ�������ʱ��ֵ����������Ϊ��ǰʱ��
/// \param datetime "YYYY-MM-DD HH:MM:SS"��ʽ����ʱ���ַ���
/// \param datemark ���ڷָ��ַ�,Ĭ��Ϊ"-"
/// \param dtmark ����ʱ��ָ��ַ�,Ĭ��Ϊ" ",������datemark��timemark��ͬ
/// \param timemark ʱ��ָ��ַ�,Ĭ��Ϊ":"
void DateTime::set( const string &datetime, const string &datemark, 
	const string &dtmark, const string &timemark ) 
{
	// init struct tm
	tm tm0;
	tm0.tm_isdst = -1;

	// init format
	string fmt;
	if ( datetime.find(dtmark) != datetime.npos )
		fmt = "%Y" + datemark + "%m" + datemark + "%d" + dtmark + 
			  "%H" + timemark + "%M" + timemark + "%S";
	else
		fmt = "%Y" + datemark + "%m" + datemark + "%d";
	
	int year,month,day, hour,minite,second;
	sscanf(fmt.c_str(),"%d-%d-%d %d:%d:%d",&year,&month,&day,&hour,&minite,&second);
	tm0.tm_year = year-1900;
	tm0.tm_mon = month;
	tm0.tm_mday = day;
	tm0.tm_hour = hour;
	tm0.tm_min = minite;
	tm0.tm_sec = second;
	tm0.tm_isdst = 0;

	this->set( tm0 );
}

/// ��������ַ���
/// \param datemark ���ڷָ��ַ�,Ĭ��Ϊ"-"
/// \param leadingzero �Ƿ񲹳�ǰ����,Ĭ��Ϊ��
/// \return ���ָ����ʽ�������ַ���
string DateTime::date( const string &datemark, const bool leadingzero ) const {
	char date_str[32];
	if ( leadingzero )
		snprintf( date_str, 32, "%04d%s%02d%s%02d", 
			this->year(), datemark.c_str(), this->month(), datemark.c_str(), this->m_day() );
	else
		snprintf( date_str, 32, "%d%s%d%s%d", 
			this->year(), datemark.c_str(), this->month(), datemark.c_str(), this->m_day() );
	
	return string( date_str );
}

/// ���ʱ���ַ���
/// \param timemark ʱ��ָ��ַ�,Ĭ��Ϊ":"
/// \param leadingzero �Ƿ񲹳�ǰ����,Ĭ��Ϊ��
/// \return ���ָ����ʽ��ʱ���ַ���
string DateTime::time( const string &timemark, const bool leadingzero ) const {
	char time_str[32];
	if ( leadingzero )
		snprintf( time_str, 32, "%02d%s%02d%s%02d", 
			this->hour(), timemark.c_str(), this->minute(), timemark.c_str(), this->second() );
	else
		snprintf( time_str, 32, "%d%s%d%s%d", 
			this->hour(), timemark.c_str(), this->minute(), timemark.c_str(), this->second() );
	
	return time_str;
}

/// �������ʱ���ַ���
/// \param datemark ���ڷָ��ַ�,Ĭ��Ϊ"-"
/// \param dtmark ����ʱ��ָ��ַ�,Ĭ��Ϊ" "
/// \param timemark ʱ��ָ��ַ�,Ĭ��Ϊ":"
/// \param leadingzero �Ƿ񲹳�ǰ����,Ĭ��Ϊ��
/// \return ���ָ����ʽ������ʱ���ַ���
string DateTime::datetime( const string &datemark, const string &dtmark,
	const string &timemark, const bool leadingzero ) const 
{
	string datetime = this->date(datemark,leadingzero) + dtmark + 
		this->time(timemark,leadingzero);
	return datetime;
}

/// ��� GMT ��ʽ����ʱ���ַ���
/// ��Ҫ�������� cookie ��Ч��
/// \return GMT ��ʽ����ʱ���ַ���
string DateTime::gmt_datetime() const {
	time_t t= ::time( NULL );
	char szBuf[128]={0};
	strftime( szBuf ,127 ,"%A,%d-%b-%y %H:%M:%S" , gmtime(&t ) );
	return string( szBuf );
}

/// ��ֵ����
DateTime& DateTime::operator=( const DateTime &date ) {
	if ( this == &date ) return *this;
	this->set( date );
	return *this;	
}
/// ��ֵ����
DateTime& DateTime::operator=( const time_t &tt ) {
	this->set( tt );
	return *this;
}

/// ��������
DateTime& DateTime::operator+=( const DateTime &date ) {
	this->set( value() + date.value() );
	return *this;
}
/// ��������
DateTime& DateTime::operator+=( const time_t &tt ) {
	this->set( value() + tt );
	return *this;
}

/// �ݼ�����
DateTime& DateTime::operator-=( const DateTime &date ) {
	this->set( value() - date.value() );
	return *this;
}
/// �ݼ�����
DateTime& DateTime::operator-=( const time_t &tt ) {
	this->set( value() - tt );
	return *this;
}

/// ���ص�����������Χ1~31
int DateTime::m_days() const {
	int m = this->month();
	if ( m==1 || m==3 || m==5 || m==7 || m==8 || m==10 || m==12 ) { 
		return 31;
	} else if ( m == 2 ) {
		int leap = (this->year()) % 4;
		if ( leap == 0 ) {
			return 29;
		} else {
			return 28;
		}
	} else {
		return 30;
	}
}

/// ��Ӳ���
DateTime operator+( const DateTime &date1, const DateTime &date2 ) {
	DateTime newdate;
	newdate.set( date1.value() + date2.value() );
	return newdate;
}
/// ��Ӳ���
DateTime operator+( const DateTime &date, const time_t &tt ) {
	DateTime newdate;
	newdate.set( date.value() + tt );
	return newdate;
}

/// �������
DateTime operator-( const DateTime &date1, const DateTime &date2 ) {
	DateTime newdate;
	newdate.set( date1.value() - date2.value() );
	return newdate;
}
/// �������
DateTime operator-( const DateTime &date, const time_t &tt ) {
	DateTime newdate;
	newdate.set( date.value() - tt );
	return newdate;
}

} // namespace

