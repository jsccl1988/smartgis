/*
File:    sde_smf.h

Desc:    SMF	�ļ����ݿ�

Version: Version 1.0

Writter:  �´���

Date:    2011.11.23

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SMF_H
#define _SDE_SMF_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"

#define				SMT_FDO_MARK_LENGTH							50
#define				SMT_FDO_TABLE_NAME_LENGTH					50
#define				SMT_FDO_FIELD_NAME_LENGTH					50
#define				SMT_FDO_FIELD_VALUE_LENGTH_SMALL			200
#define				SMT_FDO_FIELD_VALUE_LENGTH_BIG				2000

#define				SMT_FDO_MARK								("Smt SDF Version1.0")

const	string		C_STR_SYS_TABLE_SCHAME						= "Table_Schame";
const	string		C_STR_SYS_TABLE_USER						= "Table_User";

namespace sdb
{
	struct SmtSDFBinBlock
	{
		ulong			size;							//��С
		uchar			*binary;						//�ڴ�
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
		ulong			sys_block_address;				//��λ��ϵͳ��Ϣ���ݿ�
		ulong			reserved;						//ϵͳ����

		SmtSDFHeadBlock():sys_block_address(0)
			,reserved(0)
		{
			strcpy(head,"Smt SDF Version1.0");
		}
	};

	struct SmtSDFFieldInfo
	{
		uchar			name[SMT_FDO_FIELD_NAME_LENGTH];//����
		uint			type;							//�������ͣ�int,float

		SmtSDFFieldInfo():type(0)
		{
			name[0] = '\0';
		}
	};

	struct SmtSDFRecordInfo
	{
		ulong			record_address;					//��¼������ַ
		ulong			record_size;					//��¼������С
		ulong			reserved;						//ϵͳ����
		SmtSDFRecordInfo():record_address(0)
			,record_size(0)
			,reserved(0)
		{

		}
	};

	struct SmtSDFTableInfo
	{//дSysBlock�к�ÿ�ű�ͷ����
		char			name[SMT_FDO_TABLE_NAME_LENGTH];//������
		ulong			field_num;						//�ֶθ���
		SmtSDFBinBlock	field_infos;					//(field_num *sizeof(SmtSDFFieldInfo))

		//���ֶ���Ϣ
		ulong			record_num;						//��¼����
		SmtSDFBinBlock	record_infos;					//(record_num *sizeof(SmtSDFRecordInfo))
		//���ֶ���Ϣ

		uchar			have_spidx;						//�Ƿ��пռ�����
		ulong			spidx_block_address;			//��λ���������ݿ�	
		ulong			spidx_block_size;				//�������ݿ鳤��

		ulong			data_block_address;				//��λ�������ݿ�	
		ulong			data_block_size;				//�����ݿ鳤��
		ulong			reserved;						//ϵͳ����

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
	{//����Ϣ
		ulong			table_num;						//������
		SmtSDFBinBlock	table_infos;					//(size = table_num*sizeof(SmtSDFTableInfo))//����Ϣ
		ulong			reserved;						//ϵͳ����

		SmtSDFSysBlock():table_num(0)
			,reserved(0)
		{

		}
	};

	struct SmtSDFField
	{
		uchar			name[SMT_FDO_FIELD_NAME_LENGTH];//����
		uint			type;							//�������ͣ�int,float... 
		SmtSDFBinBlock	value;							//�ֶ�ֵ

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
		ulong			reacord_size;					//(sizeof(ulong)+��)
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
		ulong			field_num;						//�ֶθ�����������Ӧ���ֶ���Ϣ
		SmtSDFBinBlock	fields;							//(field_num *size(SmtSDFField))//��¼�ֶ���Ϣ

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
	����Դ��Ϣ��������
	Connect (),DisConnect (),OpenSchema(),CreateTable(),DropTable()
	2.	SmtFdoRecordSet
	���ݼ���Ϣ��������
	CreateSpatialIndex(),Query(),
	MoveFirst(),MoveNext(),MoveLast(),Delete(),IsEnd(),DeleteAll(),
	Append(),Remove(),Update();
	*/
}
#endif //_SDE_SMF_H