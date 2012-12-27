/** \mainpage Web Application Library
 *
 * WebAppLib是一系列WEB开发的C++类库。 设计目的是通过提供使用简单方便、相对独立的C++类和函数来简化CGI程序开发过程中的常见操作，提高开发效率，降低系统维护与改进的难度，适用于中等以上规模WEB系统开发<br><br>
 *
 * WebAppLib所有的类、函数、变量都声明于webapp命名空间内，由以下部分组成：<br>
 * <b>String</b> : 继承并兼容与std::string的字符串类，增加了开发中常用的字符串处理函数；<br>
 * <b>Cgi</b> : 支持文件上传的CGI参数读取类；<br>
 * <b>Cookie</b> : HTTP Cookie设置与读取类；<br>
 * <b>Template</b> : 支持在模板中嵌入条件跳转、循环输出脚本的 HTML 模板类；<br>
 * <b>HttpClient</b> : HTTP/1.1通信协议客户端类；<br>
 * <b>DateTime</b> : 日期时间运算、格式化输出类；<br>
 * <b>TextFile</b> : 固定分隔符文本文件读取解析类；<br>
 * <b>ConfigFile</b> : INI格式配置文件解析类；<br>
 * <b>Encode</b> : 字符串编码解码函数库；<br>
  * <b>Utility</b> : 系统调用与工具函数库<br>
 * 类库详细使用说明可参见类库参考手册 help.chm<br>
 *
  *
 * <center>
 * <a href="readme.html">Readme</a><br>
 * pilot.cn@gmail.com
 */

/// \file webapplib.h
/// 开发库头文件集合

#ifndef _WEBAPPLIB_H_
#define _WEBAPPLIB_H_ 

#include "waString.h"
#include "waCgi.h"

#include "waDateTime.h"
#include "waTemplate.h"
#include "waHttpClient.h"
#include "waEncode.h"
#include "waUtility.h"
#include "waTextFile.h"
#include "waConfigFile.h"

#endif //_WEBAPPLIB_H_ 

