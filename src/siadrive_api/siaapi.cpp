#include <siaapi.h>
#include <regex>
#include <siadriveconfig.h>

using namespace Sia::Api;

CSiaApi::CSiaApi(const SiaHostConfig& hostConfig, CSiaDriveConfig* siaDriveConfig) :
	_hostConfig(hostConfig),
	_siaCurl(hostConfig),
	_siaDriveConfig(siaDriveConfig),
	_wallet(new CSiaWallet(_siaCurl, siaDriveConfig)),
	_renter(new CSiaRenter(_siaCurl, siaDriveConfig)),
	_consensus(new CSiaConsensus(_siaCurl, siaDriveConfig)),
  _refreshThread(new CAutoThread(_siaCurl, _siaDriveConfig, [this] (const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) { this->Refresh(siaCurl, siaDriveConfig); }))
{
  _refreshThread->StartAutoThread();
}

CSiaApi::~CSiaApi()
{
  _refreshThread->StopAutoThread();
	//TODO Make this an option to lock on exit
	//_wallet->Lock();
}

void CSiaApi::Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
  this->_wallet->Refresh(siaCurl, siaDriveConfig);
  this->_consensus->Refresh(siaCurl, siaDriveConfig);
  this->_renter->Refresh(siaCurl, siaDriveConfig);
}

SString CSiaApi::FormatToSiaPath(SString path)
{
	if (path.Length())
	{
		std::replace(path.begin(), path.end(), '\\', '/');
		std::wregex r(L"/+");
		path = std::regex_replace(path.str(), r, L"/");

		while (path[0] == '/')
		{
			path = path.SubString(1);
		}
	}
	else
	{
		path = L"/";
	}
	

	return path;
}

SString CSiaApi::GetServerVersion() const
{
	return _siaCurl.GetServerVersion();
}

CSiaWalletPtr CSiaApi::GetWallet() const
{
	return _wallet;
}

CSiaRenterPtr CSiaApi::GetRenter() const
{
	return _renter;
}

CSiaConsensusPtr CSiaApi::GetConsensus() const
{
	return _consensus;
}

SiaHostConfig CSiaApi::GetHostConfig() const
{
	return _hostConfig;
}