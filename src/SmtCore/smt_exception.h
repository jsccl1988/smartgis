/*
File:    smt_exception.h

Desc:    Exception

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_EXCEPTION_H
#define _SMT_EXCEPTION_H

#include <sstream>

#include "smt_core.h"

namespace Smt_Core
{
	enum ExceptionCodes 
	{
		ERR_INVALIDPARAMS,
		ERR_RT_ASSERTION_FAILED,
		ERR_INTERNAL_ERROR,
		ERR_FILE_NOT_FIND
	};

	class  Exception: public std::exception
	{
	public:
		Exception(void)
		{
			;
		}

		Exception::Exception(int number, const string& description, const string& source, const char* typ, const char* file, long line):
		m_line(line),m_number(number),m_description(description),m_typeName(typ),m_source(source),m_file(file)
		{
			;
		}

		virtual ~Exception(void)throw() 
		{
			;
		}

		//
		virtual string& GetFullDescription(void) const
		{
			if (m_fullDesc.empty())
			{

				ostringstream  desc;

				desc <<  "CCL EXCEPTION(" << m_number << ":" << m_typeName << "): "
					<< m_description 
					<< " in " << m_source;

				if( m_line > 0 )
				{
					desc << " at " << m_file << " (line " << m_line << ")";
				}

				m_fullDesc = desc.str();
			}

			return m_fullDesc;
		}

		virtual long   GetLine(void) const
		{
			return m_line;
		}

		virtual const std::string GetSource(void) const
		{
			return m_source;
		}

		virtual int GetNumber(void) const
		{
			return m_number;
		}

		virtual const string GetFile(void) const
		{
			return m_file;
		}

		virtual const string GetDescription(void) const
		{
			return m_description;
		}

		const char* what(void) const
		{ 
			return GetFullDescription().c_str(); 
		}

	protected:
	private:
		long           m_line;
		int            m_number;
		string         m_typeName;
		string         m_description;
		string         m_source;
		string         m_file;
		mutable string m_fullDesc;
	};

	class ExceptionFactory
	{
	private:

		/// Private constructor, no construction
		ExceptionFactory() {}
	public:

		static Exception Create(int code,const string &desc,const string &src, const char* file, long line)
		{
			string type;
			switch(code)
			{
			case ERR_INVALIDPARAMS:
				type = "InvalidParametersException";
				break;
			case ERR_RT_ASSERTION_FAILED:
				type = "RuntimeAssertException";
				break;
			case ERR_INTERNAL_ERROR:
				type = "InternalException";
				break;
			case ERR_FILE_NOT_FIND:
				type = "FileNotFindException";
				break;
			default:
				return Exception();
				break;
			}
			return Exception(code, desc, src, type.c_str(),file, line);
		}
	};

#ifndef SMT_EXCEPT
#define SMT_EXCEPT(num, desc, src) throw ExceptionFactory::Create( \
	num,desc, src, __FILE__, __LINE__ );
#endif    
}

#endif //_SMT_EXCEPTION_H