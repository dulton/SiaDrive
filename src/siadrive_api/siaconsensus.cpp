#include <siaapi.h>

using namespace Sia::Api;

CSiaApi::_CSiaConsensus::_CSiaConsensus(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
	CSiaBase(siaCurl, siaDriveConfig),
	_Height(0),
	_Synced(false),
	_CurrentBlock(L"")
{
}

CSiaApi::_CSiaConsensus::~_CSiaConsensus()
{
}

void CSiaApi::_CSiaConsensus::Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
	json result;
	if (ApiSuccess(siaCurl.Get(L"/consensus", result)))
	{
		SetHeight(result["height"].get<std::uint64_t>());
		SetSynced(result["synced"].get<bool>());
		SetCurrentBlock(result["currentblock"].get<std::string>());
	}
	else
	{
		SetHeight(0);
		SetSynced(false);
		SetCurrentBlock(L"");
	}
}