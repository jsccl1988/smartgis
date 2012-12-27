/// \file waUtility.h
/// ϵͳ���ù��ߺ���ͷ�ļ�
/// ����ϵͳ���ú͹��ߺ���
/// ������ webapp::String, webapp::DateTime

#ifndef _WEBAPPLIB_UTILITY_H_
#define _WEBAPPLIB_UTILITY_H_ 

#include <string>

#include "smt_core.h"

using namespace std;

/// Web Application Library namaspace
namespace webapp {

/// \defgroup waUtility waUtilityϵͳ���ù��ߺ�����

// ȫ�ǰ���ַ�ת����
#define SDBC_TABLE_SIZE		172
static const char SBC_TABLE[SDBC_TABLE_SIZE][3] = {
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"һ", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"Ҽ", "��", "��", "��", "��", "½", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��" ,"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��" ,"��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��",
	"��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��", "��"
};
static const char DBC_TABLE[SDBC_TABLE_SIZE] = {
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'-', '=', '[', ']', ',', ';', '`', '\'','`', '\'',',', '.', '/', '~', '!', '.', '#',
	'$', '%', '^', '&', '*', '*', '(', ')', '-', '+', '{', '}', '|', ':', '<', '>', '?',
	'.', '\\','.', '@', '$', '_', '"', '"', '"', '<', '>', ' ', '<', '>', '[', ']'
};

/// \ingroup waUtility 
/// \enum ��ȡ���ĺ�������ѡ��
enum extract_option {
	/// ����Ӣ����ĸ
	EXTRACT_ALPHA	= 2,	
	/// ���˰���������
	EXTRACT_DIGIT	= 4,
	/// ���˰�Ǳ��	
	EXTRACT_PUNCT	= 8,	
	/// ���˿հ�  
	EXTRACT_SPACE	= 16,	
	/// ����HTML����
	EXTRACT_HTML	= 32	
};

/// \ingroup waUtility 
/// \def EXTRACT_ALL 
/// ��ȡ���ĺ�������ѡ�����ȫ������ĸ�����֡���㡢�հס�HTML��
#define EXTRACT_ALL	(EXTRACT_ALPHA|EXTRACT_DIGIT|EXTRACT_PUNCT|EXTRACT_SPACE|EXTRACT_HTML)

/// �����ַ���HASHֵ������DJB HASH�㷨
unsigned int SMT_EXPORT_API string_hash( const string &str );

/// ��ȡHTML��������
string		SMT_EXPORT_API extract_html( const string &html );

/// ȫ�ǰ���ַ�ת������ȡ����
string		SMT_EXPORT_API extract_text( const string &text, const int option=EXTRACT_ALL, 
	const size_t len=0 );

/// ׷����־��¼
void		SMT_EXPORT_API file_logger( const string &file, const char *format, ... );

/// ׷����־��¼
void		SMT_EXPORT_API file_logger( FILE *fp, const char *format, ... );

} // namespace

#if !defined(Export_SmtNetCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtNetCoreD.lib")
#       else
#          pragma comment(lib,"SmtNetCore.lib")
#	    endif  
#endif

#endif //_WEBAPPLIB_UTILITY_H_

