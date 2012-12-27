/*
File:    sde_smf.h

Desc:    SMF	文件数据库

Version: Version 1.0

Writter:  陈春亮

Date:    2011.11.23

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SMF_H
#define _SDE_SMF_H

#include "smt_core.h"
#include "smt_bas_struct.h"

#define				SMT_FDO_MARK_LENGTH							50
#define				SMT_FDO_TABLE_NAME_LENGTH					50
#define				SMT_FDO_FIELD_NAME_LENGTH					50
#define				SMT_FDO_FIELD_VALUE_LENGTH_SMALL			200
#define				SMT_FDO_FIELD_VALUE_LENGTH_BIG				2000

#define				SMT_FDO_MARK								("Smt SDF Version1.0")

const	string		C_STR_SYS_TABLE_SCHAME						= "Smt_SYS_Table_Schame";
const	string		C_STR_SYS_TABLE_USER						= "Smt_SYS_Table_User";

namespace Smt_SDESmf
{
	struct SmtSDFBinBlock
	{
		ulong			size;							//大小
		uchar			*binary;						//内存
		SmtSDFBinBlock():size(0)
			,binary(NULL)
		{
		
		}

		~SmtSDFBinBlock()
		{
			SMT_SAFE_DELETE_A(binary);
		}
	};
	/////////////////////////////////////////////////////////////////////////////////////////////////
	struct SmtSDFHeadBlock
	{
		char			head[SMT_FDO_MARK_LENGTH];		//"Smt SDF Version1.0"
		ulong			sys_block_address;				//定位到系统信息数据块
		ulong			reserved;						//系统保留

		SmtSDFHeadBlock():sys_block_address(0)
			,reserved(0)
		{
			strcpy(head,"Smt SDF Version1.0");
		}
	};

	struct SmtSDFFieldInfo
	{
		uchar			name[SMT_FDO_FIELD_NAME_LENGTH];//名称
		uint			type;							//基本类型，int,float

		SmtSDFFieldInfo():type(0)
		{
			name[0] = '\0';
		}
	};

	struct SmtSDFRecordInfo
	{
		ulong			record_address;					//记录物理地址
		ulong			record_size;					//记录物理大小
		ulong			reserved;						//系统保留
		SmtSDFRecordInfo():record_address(0)
			,record_size(0)
			,reserved(0)
		{

		}
	};

	struct SmtSDFTableInfo
	{//写SysBlock中和每张表头部分
		char			name[SMT_FDO_TABLE_NAME_LENGTH];//表名称
		ulong			field_num;						//字段个数
		SmtSDFBinBlock	field_infos;					//(field_num *sizeof(SmtSDFFieldInfo))

		//表字段信息
		ulong			record_num;						//记录个数
		SmtSDFBinBlock	record_infos;					//(record_num *sizeof(SmtSDFRecordInfo))
		//表字段信息

		uchar			have_spidx;						//是否有空间索引
		ulong			spidx_block_address;			//定位到索引数据块	
		ulong			spidx_block_size;				//索引数据块长度

		ulong			data_block_address;				//定位到表数据块	
		ulong			data_block_size;				//表数据块长度
		ulong			reserved;						//系统保留

		SmtSDFTableInfo():field_num(0)
			,record_num(0)
			,have_spidx(false)
			,spidx_block_address(0)
			,spidx_block_size(0)
			,data_block_address(0)
			,data_block_size(0)
			,reserved(0)
		{
			name[0] = '\0';
		}
	};

	struct SmtSDFSysBlock
	{//表信息
		ulong			table_num;						//表个数
		SmtSDFBinBlock	table_infos;					//(size = table_num*sizeof(SmtSDFTableInfo))//表信息
		ulong			reserved;						//系统保留

		SmtSDFSysBlock():table_num(0)
			,reserved(0)
		{

		}
	};

	struct SmtSDFField
	{
		uchar			name[SMT_FDO_FIELD_NAME_LENGTH];//名称
		uint			type;							//基本类型，int,float... 
		SmtSDFBinBlock	value;							//字段值

		SmtSDFField():type(0)
		{
			name[0] = '\0';
		}
	};

	struct SmtSDFRecord
	{
		SmtSDFBinBlock	values;

		SmtSDFRecord()
		{

		}
	};

	struct SmtTableRecord
	{
		SmtTableRecord()
		{

		}
	};

	struct SmtFeatureRecord
	{
		ulong			reacord_size;					//(sizeof(ulong)+…)
		ulong			id;
		uchar			state;							//D(elete),M(odify)
		float 			xmin,ymin ,xmax,ymax;

		//geom.
		ulong			type;
		ulong			point_num;
		ulong			point_size;
		SmtSDFBinBlock	geom;							//(size = point_num * point_size)

		//style
		SmtSDFBinBlock	style;							//(size = sizeof(SmtStyle ))

		//att
		ulong			field_num;						//字段个数，参照相应表字段信息
		SmtSDFBinBlock	fields;							//(field_num *size(SmtSDFField))//记录字段信息

		SmtFeatureRecord():reacord_size(0)
			,id(0)
			,state('D')
			,xmin(0),ymin(0),xmax(0),ymax(0)
			,type(0),point_num(0),point_size(0)
			,field_num(0)
		{

		}
	};

	/*
	1.	SmtFdoConnection
	数据源信息，及管理
	Connect (),DisConnect (),OpenSchema(),CreateTable(),DropTable()
	2.	SmtFdoRecordSet
	数据集信息，及管理
	CreateSpatialIndex(),Query(),
	MoveFirst(),MoveNext(),MoveLast(),Delete(),IsEnd(),DeleteAll(),
	Append(),Remove(),Update();
	*/
}
#endif //_SDE_SMF_H