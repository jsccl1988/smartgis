/*
File:    sde_shpsupport.h

Desc:    Shape 支持

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SHPSUPPORT_H
#define _SDE_SHPSUPPORT_H

namespace Smt_SDESmf
{
	struct DBFHeader
	{
		char m_nValid;
		char m_aDate[3];
		char m_nNumRecords[4];
		char m_nHeaderBytes[2];
		char m_nRecordBytes[2];
		char m_nReserved1[3];
		char m_nReserved2[13];
		char m_nReserved3[4];
	};

	struct DBFFIELDDescriptor
	{
		char m_sName[10];//应该为char m_sName[11]
		char m_nType;
		char m_nAddress[4];
		char m_nFieldLength;
		char m_nFieldDecimal;
		char m_nReserved1[2];
		char m_nWorkArea;
		char m_nReserved2[2];
		char m_nSetFieldsFlag;
		char m_nReserved3[8];
	};
}

#endif //_SDE_SHPSUPPORT_H