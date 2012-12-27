#include "sde_smf_api.h"
#include "sde_smf_core.h"

using namespace Smt_SDESmf;

long	WriteSmf(const char *szFile,const vector<SmtLayerInfo> &vLyrInfos)
{
	if (strlen(szFile) == 0)
		return SMT_ERR_INVALID_PARAM;

	ofstream outfile;

	locale loc = locale::global(locale(".936"));
	outfile.open(szFile,ios::out|ios::binary);
	locale::global(std::locale(loc));

	if (!outfile.is_open())
	{
		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	//head
	SmtSDFHeadBlock hdBlk;
	outfile.write((char*)(&hdBlk),sizeof(SmtSDFHeadBlock));

	//content
	int nLayers = vLyrInfos.size();
	outfile.write((char*)(&nLayers),sizeof(int));

	for (int i = 0 ; i < nLayers ; i++)
	{
		SmtLayerInfo info = vLyrInfos[i];
		outfile.write((char*)(&info),sizeof(SmtLayerInfo));
	}
	//////////////////////////////////////////////////////////////////////////
	 
	outfile.close();

	return SMT_ERR_NONE;
}

long	ReadSmf(const char *szFile,vector<SmtLayerInfo> &vLyrInfos)
{
	if (strlen(szFile) == 0)
		return SMT_ERR_INVALID_PARAM;

	ifstream infile;

	locale loc = locale::global(locale(".936"));
	infile.open(szFile,ios::out|ios::binary);
	locale::global(std::locale(loc));

	if (!infile.is_open())
	{
		return SMT_ERR_FAILURE;
	}

	//////////////////////////////////////////////////////////////////////////
	//head
	SmtSDFHeadBlock hdBlk;
	infile.read((char*)(&hdBlk),sizeof(SmtSDFHeadBlock));

	//content
	int nLayers = 0;
	infile.read((char*)(&nLayers),sizeof(int));

	for (int i = 0 ; i < nLayers ; i++)
	{
		SmtLayerInfo info;
		infile.read((char*)(&info),sizeof(SmtLayerInfo));
		vLyrInfos.push_back(info);
	}
	//////////////////////////////////////////////////////////////////////////

	infile.close();

	return SMT_ERR_NONE;
}