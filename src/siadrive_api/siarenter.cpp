#include <siaapi.h>
#include <SQLiteCpp/Database.h>

using namespace Sia::Api;
/*{
  // Settings that control the behavior of the renter.
  "settings": {
    // Allowance dictates how much the renter is allowed to spend in a given
    // period. Note that funds are spent on both storage and bandwidth.
    "allowance": {  
      // Amount of money allocated for contracts. Funds are spent on both
      // storage and bandwidth.
      "funds": "1234", // hastings

      // Number of hosts that contracts will be formed with.
      "hosts":24,

      // Duration of contracts formed, in number of blocks.
      "period": 6048, // blocks

      // If the current blockheight + the renew window >= the height the
      // contract is scheduled to end, the contract is renewed automatically.
      // Is always nonzero.
      "renewwindow": 3024 // blocks
    }
  },
  
  // Metrics about how much the Renter has spent on storage, uploads, and
  // downloads.
  "financialmetrics": {
    // How much money, in hastings, the Renter has spent on file contracts,
    // including fees.
    "contractspending": "1234", // hastings

    // Amount of money spent on downloads.
    "downloadspending": "5678", // hastings

    // Amount of money spend on storage.
    "storagespending": "1234", // hastings

    // Amount of money spent on uploads.
    "uploadspending": "5678", // hastings

    // Amount of money in the allowance that has not been spent.
    "unspent": "1234" // hastings
  }
}*/

CSiaApi::_CSiaRenter::_CSiaRenter(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
	CSiaBase(siaCurl, siaDriveConfig),
	_Funds(0),
	_Hosts(0),
	_Unspent(0),
	_TotalUsedBytes(0),
	_TotalUploadProgress(100),
  _Period(0),
  _RenewWindow(0),
  _currentAllowance({ 0,0,0,0 })
{
}

CSiaApi::_CSiaRenter::~_CSiaRenter()
{
}

void CSiaApi::_CSiaRenter::Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
	json result;
	if (ApiSuccess(siaCurl.Get(L"/renter", result)))
	{
		SiaCurrency funds = HastingsStringToSiaCurrency(result["settings"]["allowance"]["funds"].get<std::string>());
		SiaCurrency unspent = HastingsStringToSiaCurrency(result["financialmetrics"]["unspent"].get<std::string>());
    std::uint64_t hosts = result["settings"]["allowance"]["hosts"].get<std::uint64_t>();
    std::uint64_t period = result["settings"]["allowance"]["period"].get<std::uint64_t>();
    std::uint64_t renewWindow = result["settings"]["allowance"]["renewwindow"].get<std::uint64_t>();
		SetFunds(funds);
		SetHosts(hosts);
		SetUnspent(unspent);
    SetRenewWindow(renewWindow);
    SetPeriod(period);
    _currentAllowance = { funds, hosts, period, renewWindow };

    if (_currentAllowance.Funds == 0)
    {
      _currentAllowance.Funds = SIA_DEFAULT_MINIMUM_FUNDS;
      _currentAllowance.Hosts = SIA_DEFAULT_HOST_COUNT;
      _currentAllowance.Period = SIA_DEFAULT_CONTRACT_LENGTH;
      _currentAllowance.RenewWindowInBlocks = SIA_DEFAULT_RENEW_WINDOW;
    }
		if (ApiSuccess(RefreshFileTree()))
    {
      CSiaFileTreePtr fileTree;
      GetFileTree(fileTree);

			auto fileList = fileTree->GetFileList();
			if (fileList.size())
			{
				std::uint64_t total = std::accumulate(std::next(fileList.begin()), fileList.end(), fileList[0]->GetFileSize(), [](const std::uint64_t& sz, const CSiaFilePtr& file)
				{
					return sz + file->GetFileSize();
				});

				std::uint32_t totalProgress = std::accumulate(std::next(fileList.begin()), fileList.end(), fileList[0]->GetUploadProgress(), [](const std::uint32_t& progress, const CSiaFilePtr& file)
				{
					return progress + min(100, file->GetUploadProgress());
				}) / static_cast<std::uint32_t>(fileList.size());

				SetTotalUsedBytes(total);
				SetTotalUploadProgress(totalProgress);
			}
			else
			{
				SetTotalUsedBytes(0);
				SetTotalUploadProgress(100);
			}
		}
		else
		{
			SetTotalUsedBytes(0);
			SetTotalUploadProgress(100);
		}
	}
	else
	{
		SetFunds(0);
		SetHosts(0);
		SetUnspent(0);
		SetTotalUsedBytes(0);
		SetTotalUploadProgress(100);
    SetPeriod(0);
    SetRenewWindow(0);
    _currentAllowance = { 0,0,0,0 };
	}
}

SiaApiError CSiaApi::_CSiaRenter::RefreshFileTree( )
{
  SiaApiError ret;
  CSiaFileTreePtr tempTree(new CSiaFileTree(GetSiaCurl(), &GetSiaDriveConfig()));
  json result;  
  SiaCurlError cerror = GetSiaCurl().Get(L"/renter/files", result);
  if (ApiSuccess(cerror))
  {
    tempTree->BuildTree(result);
    {
      std::lock_guard<std::mutex> l(_fileTreeMutex);
      _fileTree = tempTree;
    }
  }
  else
  {
    ret = { SiaApiErrorCode::RequestError, cerror.GetReason() };
  }

  return ret;
}

SiaApiError CSiaApi::_CSiaRenter::FileExists(const SString& siaPath, bool& exists) const
{
	CSiaFileTreePtr siaFileTree;
	SiaApiError ret = GetFileTree(siaFileTree);
	if (ApiSuccess(ret))
	{
		exists = siaFileTree->FileExists(siaPath);
	}

	return ret;
}

SiaApiError CSiaApi::_CSiaRenter::DownloadFile(const SString& siaPath, const SString& location) const
{
  SiaApiError ret;
	json result;
  auto cerror = GetSiaCurl().Get(L"/renter/download/" + siaPath, { { L"destination", location } }, result);
	if (!ApiSuccess(cerror))
	{
    ret = { SiaApiErrorCode::RequestError, cerror.GetReason() };
	}

	return ret;
}

SiaApiError CSiaApi::_CSiaRenter::GetFileTree(CSiaFileTreePtr& siaFileTree) const
{
  siaFileTree = _fileTree;
  if (!siaFileTree)
  {
    siaFileTree.reset(new CSiaFileTree(GetSiaCurl(), &GetSiaDriveConfig()));
  }

	return SiaApiErrorCode::Success;
}

SiaRenterAllowance CSiaApi::_CSiaRenter::GetAllowance() const
{
  return _currentAllowance;
}

SiaApiError CSiaApi::_CSiaRenter::SetAllowance(const SiaRenterAllowance& renterAllowance)
{
  SiaApiError ret;

  json result;
  auto cerror = GetSiaCurl().Post(L"/renter", 
  { 
    {       "funds", SiaCurrencyToHastingsString(renterAllowance.Funds) },
    {       "hosts", SString::FromUInt64(renterAllowance.Hosts) },
    {      "period", SString::FromUInt64(renterAllowance.Period) },
    { "renewwindow", SString::FromUInt64 (renterAllowance.RenewWindowInBlocks) }
  }, result);
  if (!ApiSuccess(cerror))
  {
    ret = { SiaApiErrorCode::RequestError, cerror.GetReason() };
  }

  return ret;
}