Module coupling is little, the system is divided into the UI component modules (view,function box,directory tree), rendering module (map rendering, 3D model rendering), interactive tool module (edit, view, analysis tools), data source module (heterogeneous data storage reunification) ,WebGIS module, plug-in system modules (scalable third-party function modules). Two-dimensional view using the GDI double buffering graphics, and multi-threaded graphics technology, GIS core library and basic library part are implemented by c++ and the UI part by MFC.So ease to maintenance, expansion and cross-platform portability.

SmartGIS系统模块耦合度较小，系统分为UI部件模块（视图、功能箱、目录树）、渲染模块（地图渲染、3D模型渲染）、交互工具模块（编辑、浏览、分析等工具）、数据源模块（异构数据的存储统一）、WebGIS模块（MapServer、WebServer）、插件系统模块（可扩展第三方功能模块）等。其中二维视图采用了GDI双缓冲绘图、多线程绘图等技术，GIS核心库及基础库用c++实现，UI部分用MFC实现，便于维护、扩充和跨平台移植；

1.3D渲染部分采用3D渲染引擎设计技术，抽象出渲染驱动接口，使用bridge模式将接口与实现分离，并利用OpenGL实现驱动接口。采用OO思想将三维对象抽象出来，并提供常用3D模型对象库，通过八叉树对模型进行管理，渲染地形、流水、点云及GIS三维几何体等对象；

2.Web地图发布部分基于线程池技术实现多线程并发地图服务，服务器端将地图服务和Web服务分开，两者通过Socket进行跨进程通信。地图服务基于OGC标准实现地图服务访问接口，客户端浏览器使用JavaScript（OpenLayers）脚本进行测试。


说明：
1.最新代码编译，获得最新代码，将本文件解压至..\branches\SmartGis.1.1.vs08下。打开vs2008\SmartGIS.sln解决方案即可编译源代码
2.bin目录下 SmartGIS.exe可直接运行
3.SmartGIS web地图发布，已经能够实现web地图的发布，包括瓦片切图，wms和wts服务。apache web服务器配置目前仍未实现定制，仍需用户自己修改配置文件，因此web地图部分不用户可暂时不管，但源码部分任然提供。