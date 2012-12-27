/*
File:    smt_core.h

Desc:    SmartGis ,并发地图服务

Version: Version 1.0

Writter:  陈春亮

Date:    2012.8.14

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _MSVR_CORE_H
#define _MSVR_CORE_H

#include "smt_core.h"

namespace Smt_MapServer
{
	//msvr 
	const string c_str_msvr_request						= "REQUEST";				//请求
	const string c_str_msvr_service						= "SERVICE";				//服务
	const string c_str_msvr_version						= "VERSION";				//版本号
	const string c_str_msvr_name						= "MSVRNAME";				//地图服务名称

	//wms
	const string c_str_msvr_wms							= "WMS";	
	const string c_str_msvr_wms_getcapabilities			= "GetCapabilities";	
	const string c_str_msvr_wms_getfeatureinfo			= "GetFeatureInfo";	
	const string c_str_msvr_wms_getmap					= "GetMap";	

	const string c_str_msvr_wms_layers					= "LAYERS";		
	const string c_str_msvr_wms_styles					= "STYLES";		
	const string c_str_msvr_wms_bbox					= "BBOX";		
	const string c_str_msvr_wms_width					= "WIDTH";		
	const string c_str_msvr_wms_height					= "HEIGHT";		
	const string c_str_msvr_wms_srs						= "SRS";		
	const string c_str_msvr_wms_format					= "FORMAT";	
	const string c_str_msvr_wms_transparent				= "TRANSPARENT";
	const string c_str_msvr_wms_bgcolor					= "BGCOLOR";

	const string c_str_msvr_wms_feature_count			= "FEATURE_COUNT";
	const string c_str_msvr_wms_info_format				= "INFO_FORMAT";
	const string c_str_msvr_wms_query_layers			= "QUERY_LAYERS";
	const string c_str_msvr_wms_x						= "X";
	const string c_str_msvr_wms_y						= "Y";
	const string c_str_msvr_wms_i						= "I";
	const string c_str_msvr_wms_j						= "J";

	//wmts
	const string c_str_msvr_wmts						= "WMTS";	
	const string c_str_msvr_wmts_getcapabilities		= "GetCapabilities";
	const string c_str_msvr_wmts_getfeatureinfo			= "GetFeatureInfo";	
	const string c_str_msvr_wmts_gettile				= "GetTile";	

	const string c_str_msvr_wmts_gettile_level			= "TILEMATRIX";			
	const string c_str_msvr_wmts_gettile_col			= "TILECOL";				
	const string c_str_msvr_wmts_gettile_row			= "TILEROW";

	const string c_str_msvr_wmts_layers					= "LAYERS";		
	const string c_str_msvr_wmts_styles					= "STYLES";	
	const string c_str_msvr_wmts_srs					= "SRS";		
	const string c_str_msvr_wmts_format					= "FORMAT";	
	const string c_str_msvr_wmts_transparent			= "TRANSPARENT";
	const string c_str_msvr_wmts_bgcolor				= "BGCOLOR";

	//wfs
	const string c_str_msvr_wfs							= "WFS";	
	const string c_str_msvr_wfs_getcapabilities			= "GetCapabilities";	
	const string c_str_msvr_wfs_getlayer				= "GetLayer";			
	const string c_str_msvr_wfs_getfeature				= "GetFeature";		
}

#define			MAPSEVER_LOG_NAME						"MSVR"
#define			MAPSEVER_DEVICE_LOG_NAME				"MSVR_DEVICE"

#define			MSVR_RSP_TEXT							1					//文本
#define			MSVR_RSP_MIME							2					//普通二进制
#define			MSVR_RSP_BIN							3					//MIME
#define			MSVR_RSP_END							4					//结束标志

#define			MSVR_BUF_LENGTH_1K						((1<<10)-1)
#define			MSVR_BUF_LENGTH_2K						((1<<11)-1)
#define			MSVR_BUF_LENGTH_4K						((1<<12)-1)
#define			MSVR_BUF_LENGTH_8K						((1<<13)-1)
#define			MSVR_BUF_LENGTH_16K						((1<<14)-1)
#define			MSVR_BUF_LENGTH_32K						((1<<15)-1)
#define			MSVR_BUF_LENGTH_64K						((1<<16)-1)
#define			MSVR_BUF_LENGTH_128K					((1<<17)-1)

#define			MSVR_VERSION_1_0_0						0					//1.0.0
#define			MSVR_VERSION_1_1_1						1					//1.1.1
#define			MSVR_VERSION_1_3_0						2					//1.3.0


#endif //_MSVR_CORE_H