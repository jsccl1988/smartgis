Module coupling is little, the system is divided into the UI component modules (view,function box,directory tree), rendering module (map rendering, 3D model rendering), interactive tool module (edit, view, analysis tools), data source module (heterogeneous data storage reunification) ,WebGIS module, plug-in system modules (scalable third-party function modules). Two-dimensional view using the GDI double buffering graphics, and multi-threaded graphics technology, GIS core library and basic library part are implemented by c++ and the UI part by MFC.So ease to maintenance, expansion and cross-platform portability.

SmartGISϵͳģ����϶Ƚ�С��ϵͳ��ΪUI����ģ�飨��ͼ�������䡢Ŀ¼��������Ⱦģ�飨��ͼ��Ⱦ��3Dģ����Ⱦ������������ģ�飨�༭������������ȹ��ߣ�������Դģ�飨�칹���ݵĴ洢ͳһ����WebGISģ�飨MapServer��WebServer�������ϵͳģ�飨����չ����������ģ�飩�ȡ����ж�ά��ͼ������GDI˫�����ͼ�����̻߳�ͼ�ȼ�����GIS���Ŀ⼰��������c++ʵ�֣�UI������MFCʵ�֣�����ά��������Ϳ�ƽ̨��ֲ��

1.3D��Ⱦ���ֲ���3D��Ⱦ������Ƽ������������Ⱦ�����ӿڣ�ʹ��bridgeģʽ���ӿ���ʵ�ַ��룬������OpenGLʵ�������ӿڡ�����OO˼�뽫��ά���������������ṩ����3Dģ�Ͷ���⣬ͨ���˲�����ģ�ͽ��й�����Ⱦ���Ρ���ˮ�����Ƽ�GIS��ά������ȶ���

2.Web��ͼ�������ֻ����̳߳ؼ���ʵ�ֶ��̲߳�����ͼ���񣬷������˽���ͼ�����Web����ֿ�������ͨ��Socket���п����ͨ�š���ͼ�������OGC��׼ʵ�ֵ�ͼ������ʽӿڣ��ͻ��������ʹ��JavaScript��OpenLayers���ű����в��ԡ�


˵����
1.���´�����룬������´��룬�����ļ���ѹ��..\branches\SmartGis.1.1.vs08�¡���vs2008\SmartGIS.sln����������ɱ���Դ����
2.binĿ¼�� SmartGIS.exe��ֱ������
3.SmartGIS web��ͼ�������Ѿ��ܹ�ʵ��web��ͼ�ķ�����������Ƭ��ͼ��wms��wts����apache web����������Ŀǰ��δʵ�ֶ��ƣ������û��Լ��޸������ļ������web��ͼ���ֲ��û�����ʱ���ܣ���Դ�벿����Ȼ�ṩ��