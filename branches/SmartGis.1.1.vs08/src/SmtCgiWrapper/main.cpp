#include <iostream>
#include "msvr_cgiwrapper.h"
#include "webApplib.h"

using namespace Smt_CgiWrapper;
int main(int argc, char* argv[])
{
	//locale loc = locale::global(locale(".936"));
	webapp::Cgi  cgi;
	//locale::global(std::locale(loc));

	SmtCgiWrapper cgiWrapper;
	if (SMT_ERR_NONE == cgiWrapper.Init() &&
		SMT_ERR_NONE == cgiWrapper.Create())
	{
		map<string,string> mapKey2Value = cgi.dump();

		if (mapKey2Value.size() == 0)
		{//调试
			/*mapKey2Value["BBOX"] = "-32.6953125,-14.94140625,87.7734375,49.62890625";
			mapKey2Value["FORMAT"] = "image/png";
			mapKey2Value["HEIGHT"] = "551";
			mapKey2Value["LAYERS"] = "南通行政区,南通道路";
			mapKey2Value["REQUEST"] = "GetMap";
			mapKey2Value["SERVICE"] = "WMS";
			mapKey2Value["SRS"] = "EPSG:4326";
			mapKey2Value["STYLES"] = "NULL";
			mapKey2Value["TRANSPARENT"] = "TRUE";
			mapKey2Value["VERSION"] = "1.1.1";
			mapKey2Value["WIDTH"] = "1028";
			mapKey2Value["MSVR"] = "msvr2";*/

			mapKey2Value["MSVRNAME"] = "msvr0";
			mapKey2Value["REQUEST"] = "GetCapabilities";
			mapKey2Value["SERVICE"] = "WMS";
			mapKey2Value["VERSION"] = "1.1.1";
		}

		map<string,string>::iterator iter = mapKey2Value.begin();
		while (iter != mapKey2Value.end())
		{
			if (iter->second == "")
				iter->second = string("NULL");
	
			++iter;
		}

		cgiWrapper.Process(mapKey2Value);
		cgiWrapper.Destroy();
	}

	return 0;
}