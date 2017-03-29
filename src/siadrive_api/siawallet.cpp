#include <siaapi.h>

using namespace Sia::Api;

static SString SeedLangToString(const SiaSeedLanguage& lang)
{
	switch (lang)
	{
	case SiaSeedLanguage::English:
		return L"english";

	case SiaSeedLanguage::German:
		return L"german";

	case SiaSeedLanguage::Japanese:
		return L"japanese";

	default:
		throw std::exception("Seed language not implemented");
	}
}

CSiaApi::_CSiaWallet::_CSiaWallet(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
  CSiaBase(siaCurl, siaDriveConfig),
	_Created(false),
	_Locked(false),
  _Connected(false),
  _ConfirmedBalance(0),
  _UnconfirmedBalance(0)
{
}

CSiaApi::_CSiaWallet::~_CSiaWallet()
{
}

/*{
"encrypted": true,
"unlocked":  true,

"confirmedsiacoinbalance":     "123456", // hastings, big int
"unconfirmedoutgoingsiacoins": "0",      // hastings, big int
"unconfirmedincomingsiacoins": "789",    // hastings, big int

"siafundbalance":      "1",    // siafunds, big int
"siacoinclaimbalance": "9001", // hastings, big int
}*/
void CSiaApi::_CSiaWallet::Refresh(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
  bool locked = false;
  bool created = false;
  SiaCurrency unconfirmed = 0;
  SiaCurrency confirmed = 0;
  bool connected = false;
  SString address;

	json result;
	SiaCurlError cerror = GetSiaCurl().Get(L"/wallet", result);
  if (ApiSuccess(cerror))
  {
    created = result["encrypted"].get<bool>();
    locked = !result["unlocked"].get<bool>();
    confirmed = HastingsStringToSiaCurrency(result["confirmedsiacoinbalance"].get<std::string>());
    SiaCurrency sc1 = HastingsStringToSiaCurrency(result["unconfirmedincomingsiacoins"].get<std::string>());
    SiaCurrency sc2 = HastingsStringToSiaCurrency(result["unconfirmedoutgoingsiacoins"].get<std::string>());
    unconfirmed = sc1 - sc2;

    cerror = GetSiaCurl().Get(L"/wallet/address", result);
    if (ApiSuccess(cerror))
    {
      address = result["address"].get<std::string>();
    }
    connected = true;
  }

  SetCreated(created);
  SetLocked(locked);
  SetConfirmedBalance(confirmed);
  SetUnconfirmedBalance(unconfirmed);
  SetReceiveAddress(address);
  SetConnected(connected);
}

SiaApiError CSiaApi::_CSiaWallet::Create(const SiaSeedLanguage& seedLanguage, SString& seed)
{
	SiaApiError error = SiaApiError::RequestError;
	if (GetConnected())
	{
		error = SiaApiError::WalletExists;
		if (!GetCreated())
		{
			error = SiaApiError::RequestError;
			json result;
			SiaCurlError cerror = GetSiaCurl().Post(L"/wallet/init", { {L"dictionary", SeedLangToString(seedLanguage)} }, result);
			if (ApiSuccess(cerror))
			{
				error = SiaApiError::Success;
				seed = result["primaryseed"].get<std::string>();
			}
		}
	}

	return error;
}

SiaApiError CSiaApi::_CSiaWallet::Restore(const SString& seed)
{
	SiaApiError error = SiaApiError::NotImplemented;
	// TODO Future enhancement
	return error;
}

SiaApiError CSiaApi::_CSiaWallet::Lock()
{
	SiaApiError error = GetCreated() ? (GetLocked() ? SiaApiError::WalletLocked : SiaApiError::Success) : SiaApiError::WalletNotCreated;
	if (ApiSuccess(error))
	{
		error = SiaApiError::RequestError;

		json result;
		SiaCurlError cerror = GetSiaCurl().Post(L"/wallet/lock", {}, result);
		if (ApiSuccess(cerror))
		{
			error = SiaApiError::Success;
		}
	}

	return error;
}

SiaApiError CSiaApi::_CSiaWallet::Unlock(const SString& password)
{
	SiaApiError error = GetCreated() ? (GetLocked() ? SiaApiError::Success : SiaApiError::WalletUnlocked) : SiaApiError::WalletNotCreated;
	if (ApiSuccess(error))
	{
		error = SiaApiError::RequestError;

		json result;
		SiaCurlError cerror = GetSiaCurl().Post(L"/wallet/unlock", { {L"encryptionpassword", password} }, result);
		if (ApiSuccess(cerror))
		{
			error = SiaApiError::Success;
		}
	}

	return error;
}