#include <siadriveapp.h>
#include <include/views/cef_browser_view.h>
#include <include/views/cef_window.h>
#include <include/wrapper/cef_helpers.h>
#include <include/wrapper/cef_closure_task.h>
#include <autothread.h>
#include <siaapi.h>
#include <siacurl.h>
#include <siadriveconfig.h>
#include "siadrivehandler.h"
#include <siadokandrive.h>
#include <eventsystem.h>

using namespace Sia;
using namespace Sia::Api;

class FunctionHandler : 
  public CefV8Handler 
{
public:
  FunctionHandler(const CSiaApi& siaApi, 
                  bool& appStarted, 
                  std::unique_ptr<CSiaDriveConfig>& siaDriveConfig, 
                  std::unique_ptr<Api::Dokan::CSiaDokanDrive>& siaDrive, 
                  std::function<void()> shutdownCallback, 
                  std::function<void(CefRefPtr<CefV8Context> context)> refreshCallback) :
    _siaApi(siaApi),
    _siaDriveConfig(siaDriveConfig),
    _siaDrive(siaDrive),
    _appStarted(appStarted),
    _shutdownCallback(shutdownCallback), 
    _refreshCallback(refreshCallback)
  { 
  }

private:
  const CSiaApi& _siaApi;
  std::unique_ptr<CSiaDriveConfig>& _siaDriveConfig;
  std::unique_ptr<Api::Dokan::CSiaDokanDrive>& _siaDrive;
  bool& _appStarted;
  std::function<void()> _shutdownCallback;
  std::function<void(CefRefPtr<CefV8Context> context)> _refreshCallback;

private:
  void MountCallback(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> cb) const
  {
    CefV8ValueList args;
    args.push_back(CefV8Value::CreateBool(true));
    if (!args[0]->GetBoolValue())
    {
      args.push_back(CefV8Value::CreateString("Failed to mount"));
    }
    cb->ExecuteFunctionWithContext(context, nullptr, args);
  }

  void UnmountCallback(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> cb) const
  {
    CefV8ValueList args;
    args.push_back(CefV8Value::CreateBool(true));
    if (!args[0]->GetBoolValue())
    {
      args.push_back(CefV8Value::CreateString("Failed to unmount"));
    }
    cb->ExecuteFunctionWithContext(context, nullptr, args);
  }

  void UnlockCallback(SiaApiError error, CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> cb, const SString& password) const
  {
    CefV8ValueList args;
    args.push_back(CefV8Value::CreateBool(ApiSuccess(error)));
    if (!args[0]->GetBoolValue())
    {
      args.push_back(CefV8Value::CreateString("Failed to unlock wallet"));
    }
    cb->ExecuteFunctionWithContext(context, nullptr, args);
  }
  
  void CreateWalletCallback(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> cb) const
  {
    SString seed;
    SiaApiError error = _siaApi.GetWallet()->Create(SiaSeedLanguage::English, seed);

    CefV8ValueList args;
    args.push_back(CefV8Value::CreateBool(ApiSuccess(error)));
    if (args[0]->GetBoolValue())
    {
      args.push_back(CefV8Value::CreateString(seed.str()));
    }
    else
    {
      args.push_back(CefV8Value::CreateString("Failed to create new wallet"));
    }
    cb->ExecuteFunctionWithContext(context, nullptr, args);
  }

  void DriveMountEndedCallback(CefRefPtr<CefV8Context> context)
  {
    context->Enter();
    if (_appStarted)
    {
      auto global = context->GetGlobal();
      auto uiUpdate = global->GetValue("uiUpdate");
      auto notifyDriveUnmounted = uiUpdate->GetValue("notifyDriveUnmounted");;

      CefV8ValueList args;
      notifyDriveUnmounted->ExecuteFunction(nullptr, args);
    }
    context->Exit();
  }

public:
  virtual bool Execute(const CefString& name,
    CefRefPtr<CefV8Value> object,
    const CefV8ValueList& arguments,
    CefRefPtr<CefV8Value>& retval,
    CefString& exception) OVERRIDE 
  {
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    if (name == "unlockWallet") 
    {
      retval = CefV8Value::CreateBool(true);
      SString password = arguments[0]->GetStringValue().ToWString();
      CefRefPtr<CefV8Value> cb = arguments[1];

      // Don't hang UI while unlocking
      std::thread([this, context, password, cb]()
      {
        CefPostTask(TID_RENDERER, base::Bind(&FunctionHandler::UnlockCallback, this, _siaApi.GetWallet()->Unlock(password), context, cb, password));
      }).detach();

      return true;
    }
    else if (name == "createWallet")
    {
      retval = CefV8Value::CreateBool(true);
      CefRefPtr<CefV8Value> cb = arguments[0];

      CefPostTask(TID_RENDERER, base::Bind(&FunctionHandler::CreateWalletCallback, this, context, cb));
      return true;
    }
    else if (name == "startApp")
    {
      _appStarted = true;
      _refreshCallback(context);
      return true;
    }
    else if (name == "stopApp")
    {
      _appStarted = false;
      return true;
    }
    else if (name == "mountDrive")
    {
      CEventSystem::EventSystem.AddOneShotEventConsumer([this, context](const CEvent& event) -> bool
      {
        if (dynamic_cast<const Dokan::DriveMountEnded*>(&event) != nullptr)
        {
          CefPostTask(TID_RENDERER, base::Bind(&FunctionHandler::DriveMountEndedCallback, this, context));
          return true;
        }
        return false;
      });

      retval = CefV8Value::CreateBool(true);
      SString drive = arguments[0]->GetStringValue().ToWString();
      CefRefPtr<CefV8Value> cb = arguments[1];
      std::thread([this, drive, context, cb]()
      {
        _siaDrive->Mount(drive[0], _siaDriveConfig->GetCacheFolder(), 0);
        CefPostTask(TID_RENDERER, base::Bind(&FunctionHandler::MountCallback, this, context, cb));
      }).detach();
      return true;
    }
    else if (name == "unmountDrive")
    {
      retval = CefV8Value::CreateBool(true);
      CefRefPtr<CefV8Value> cb = arguments[0];
      std::thread([this, context, cb]()
      {
        _siaDrive->Unmount();
        CefPostTask(TID_RENDERER, base::Bind(&FunctionHandler::UnmountCallback, this, context, cb));
      }).detach();

      return true;
    }
    else if (name == "shutdown")
    {
      _shutdownCallback();
      return true;
    }
    else if (name == "setRenterSettings")
    {
      CefRefPtr<CefV8Value> vAllowance = arguments[0];
      CefRefPtr<CefV8Value> cb = arguments[1];
      SiaRenterAllowance allowance({ 
        SiaCurrency(vAllowance->GetValue("Funds")->GetStringValue().ToWString()), 
        SString::ToUInt32(vAllowance->GetValue("Hosts")->GetStringValue().ToWString()), 
        SString::ToUInt32(vAllowance->GetValue("Period")->GetStringValue().ToWString()), 
        SString::ToUInt32(vAllowance->GetValue("RenewWindowInBlocks")->GetStringValue().ToWString())
      });
      auto ret = _siaApi.GetRenter()->SetAllowance(allowance);

      CefV8ValueList args;
      args.push_back(CefV8Value::CreateBool(ApiSuccess(ret)));
      args.push_back(CefV8Value::CreateString(ret.GetReason().str()));
      cb->ExecuteFunctionWithContext(context, nullptr, args);
      return true;
    }
    // Function does not exist.
    return false;
  }

  IMPLEMENT_REFCOUNTING(FunctionHandler);
};

class SimpleWindowDelegate : 
	public CefWindowDelegate 
{
public:
	explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browserView) : 
    _browserView(browserView)
	{
	}

	void OnWindowCreated(CefRefPtr<CefWindow> window) OVERRIDE 
	{
		window->AddChildView(_browserView);
		window->Show();

		_browserView->RequestFocus();
	}

	void OnWindowDestroyed(CefRefPtr<CefWindow> window) OVERRIDE 
	{
		_browserView = nullptr;
	}

	bool CanClose(CefRefPtr<CefWindow> window) OVERRIDE 
	{
		CefRefPtr<CefBrowser> browser = _browserView->GetBrowser();
		return (browser) ? browser->GetHost()->TryCloseBrowser() : true;
	}

private:
	CefRefPtr<CefBrowserView> _browserView;

	IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
	DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

CSiaDriveApp::CSiaDriveApp() 
{
  _siaDriveConfig.reset(new CSiaDriveConfig());
  SiaHostConfig hostConfig;
  hostConfig.HostName = _siaDriveConfig->GetHostNameOrIp();
  hostConfig.HostPort = _siaDriveConfig->GetHostPort();
  hostConfig.RequiredVersion = COMPAT_SIAD_VERSION;
  _siaCurl.reset(new CSiaCurl(hostConfig));
  _siaApi.reset(new CSiaApi(hostConfig, _siaDriveConfig.get()));
}

CSiaDriveApp::~CSiaDriveApp()
{
  _siaApi.reset(nullptr);
  _siaCurl.reset(nullptr);
}

void CSiaDriveApp::ExecuteSetter(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> obj, const SString& method, const SString& value)
{
  ExecuteSetter(context, obj, method, CefV8Value::CreateString(value.str()));
}

void CSiaDriveApp::ExecuteSetter(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> obj, const SString& method, CefRefPtr<CefV8Value> value)
{
  CefRefPtr<CefV8Value> setConfirmed = obj->GetValue(method.str());
  CefV8ValueList args;
  args.push_back(value);
  setConfirmed->ExecuteFunctionWithContext(context, nullptr, args);
}

void CSiaDriveApp::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context)
{
  CEF_REQUIRE_RENDERER_THREAD();

  CefRefPtr<CefV8Value> global = context->GetGlobal();
  CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(nullptr, nullptr);
  obj->SetValue("isWalletLocked", CefV8Value::CreateBool(_siaApi->GetWallet()->GetLocked()), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("isWalletConfigured", CefV8Value::CreateBool(_siaApi->GetWallet()->GetCreated()), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("isOnline", CefV8Value::CreateBool(_siaApi->GetWallet()->GetConnected()), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("clientVersion", CefV8Value::CreateString(SIDRIVE_VERSION_STRING), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("allocatedRenterFunds", CefV8Value::CreateString(_siaApi->GetRenter()->GetFunds().ToString()), V8_PROPERTY_ATTRIBUTE_NONE);
  CefRefPtr<CefV8Value> defaultFunds = CefV8Value::CreateObject(nullptr, nullptr);
  defaultFunds->SetValue("Funds", CefV8Value::CreateString(SIA_DEFAULT_MINIMUM_FUNDS.ToString()), V8_PROPERTY_ATTRIBUTE_NONE);
  defaultFunds->SetValue("Hosts", CefV8Value::CreateString(SString::FromUInt32(SIA_DEFAULT_HOST_COUNT).str()), V8_PROPERTY_ATTRIBUTE_NONE);
  defaultFunds->SetValue("Period", CefV8Value::CreateString(SString::FromUInt32(SIA_DEFAULT_CONTRACT_LENGTH).str()), V8_PROPERTY_ATTRIBUTE_NONE);
  defaultFunds->SetValue("RenewWindowInBlocks", CefV8Value::CreateString(SString::FromUInt32(SIA_DEFAULT_RENEW_WINDOW).str()), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("defaultRenterSettings", defaultFunds, V8_PROPERTY_ATTRIBUTE_NONE);
  global->SetValue("uiState", obj, V8_PROPERTY_ATTRIBUTE_NONE);

  CefRefPtr<FunctionHandler> handler(new FunctionHandler(*_siaApi, _appStarted, _siaDriveConfig, _siaDrive, [this]() {this->ShutdownServices(); }, [this](CefRefPtr<CefV8Context> context) {this->SiaApiRefreshCallback(context, *_siaCurl, _siaDriveConfig.get()); }));
  obj = CefV8Value::CreateObject(nullptr, nullptr);
  obj->SetValue("unlockWallet", CefV8Value::CreateFunction("unlockWallet", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("createWallet", CefV8Value::CreateFunction("createWallet", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("startApp", CefV8Value::CreateFunction("startApp", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("stopApp", CefV8Value::CreateFunction("stopApp", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("mountDrive", CefV8Value::CreateFunction("mountDrive", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("unmountDrive", CefV8Value::CreateFunction("unmountDrive", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("shutdown", CefV8Value::CreateFunction("shutdown", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  obj->SetValue("setRenterSettings", CefV8Value::CreateFunction("setRenterSettings", handler), V8_PROPERTY_ATTRIBUTE_NONE);
  global->SetValue("appActions", obj, V8_PROPERTY_ATTRIBUTE_NONE);
  
  _refreshThread.reset(new CAutoThread(*_siaCurl, _siaDriveConfig.get(), [this, context](const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
  {
    this->SiaApiRefreshCallback(context, siaCurl, siaDriveConfig);
  }));
  _refreshThread->StartAutoThread();
}

void CSiaDriveApp::OnContextInitialized() 
{
	CEF_REQUIRE_UI_THREAD();

	CefRefPtr<CefCommandLine> commandLine = CefCommandLine::GetGlobalCommandLine();
  
#if defined(OS_WIN) || defined(OS_LINUX)
	const bool useViews = commandLine->HasSwitch("use-views");
#else
	const bool useViews = false;
#endif

  CefRefPtr<CSiaDriveHandler> handler(new CSiaDriveHandler(useViews));
	CefBrowserSettings browserSettings;
	
	SString url = "file:///./htdocs/index.html";
	if (useViews) 
	{
		CefRefPtr<CefBrowserView> browserView = CefBrowserView::CreateBrowserView(handler, url.str(), browserSettings, nullptr, nullptr);
		CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(browserView));
	}
	else 
	{
		CefWindowInfo windowInfo;
#ifdef _WIN32
		windowInfo.SetAsPopup(nullptr, "SiaDrive");
#endif
    windowInfo.width = 800;
    windowInfo.height = 675;

		CefBrowserHost::CreateBrowser(windowInfo, handler, url.str(), browserSettings, nullptr);
	}
}

void CSiaDriveApp::ShutdownServices()
{
  if (_refreshThread)
  {
    _appStarted = false;
    _refreshThread->StopAutoThread();
    _refreshThread.reset(nullptr);
  }

  if (_siaDrive)
  {
    _siaDrive->Unmount();
    _siaDrive.reset(nullptr);
  }
}

void CSiaDriveApp::SiaApiRefreshCallback(CefRefPtr<CefV8Context> context, const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
  if (CefCurrentlyOn(TID_RENDERER))
  {
    context->Enter();
    auto global = context->GetGlobal();
    bool wasOnline = global->GetValue("uiState")->GetValue("isOnline")->GetBoolValue();
    bool isOnline = _siaApi->GetWallet()->GetConnected();
   
    if (wasOnline != isOnline)
    {
      _appStarted = false;
      context->GetBrowser()->Reload();
    }
    else if (_appStarted)
    {
#ifdef _WIN32
      if (!_siaDrive)
      {
        _siaDrive.reset(new Dokan::CSiaDokanDrive(*_siaApi, _siaDriveConfig.get()));
      }
#else
      a
#endif

      auto uiActions = global->GetValue("uiUpdate");
      auto renterActions = uiActions->GetValue("Renter");
      auto walletActions = uiActions->GetValue("Wallet");

      // Update server version
      ExecuteSetter(context, uiActions, "setServerVersion", _siaApi->GetServerVersion());

      // Display wallet data
      auto confirmedBalance = _siaApi->GetWallet()->GetConfirmedBalance();
      auto unconfirmedBalance = _siaApi->GetWallet()->GetUnconfirmedBalance();
      auto totalBalance = confirmedBalance + unconfirmedBalance;
      ExecuteSetter(context, walletActions, "setConfirmedBalance", SiaCurrencyToString(confirmedBalance));
      ExecuteSetter(context, walletActions, "setUnconfirmedBalance", SiaCurrencyToString(unconfirmedBalance));
      ExecuteSetter(context, walletActions, "setTotalBalance", SiaCurrencyToString(totalBalance));

      if (_walletReceiveAddress.IsNullOrEmpty())
      {
        _walletReceiveAddress = _siaApi->GetWallet()->GetReceiveAddress();
        ExecuteSetter(context, walletActions, "setReceiveAddress", _walletReceiveAddress);
      }

      // Funding
      SiaCurrency allocatedFunds = _siaApi->GetRenter()->GetFunds();
      SiaCurrency unspentFunds = _siaApi->GetRenter()->GetUnspent();
      ExecuteSetter(context, renterActions, "setAllocatedFunds", SiaCurrencyToString(allocatedFunds));
      ExecuteSetter(context, renterActions, "setUsedFunds", SiaCurrencyToString(allocatedFunds - unspentFunds));
      ExecuteSetter(context, renterActions, "setAvailableFunds", SiaCurrencyToString(unspentFunds));
      ExecuteSetter(context, renterActions, "setHostCount", SString::FromUInt64(_siaApi->GetRenter()->GetHosts()));
      global->GetValue("uiState")->SetValue("allocatedRenterFunds", CefV8Value::CreateString(SiaCurrencyToString(allocatedFunds).str()), V8_PROPERTY_ATTRIBUTE_NONE);

      // Space
      SiaCurrency totalUsedGb = _siaApi->GetRenter()->GetTotalUsedBytes() ? _siaApi->GetRenter()->GetTotalUsedBytes() / (1024.0 * 1024.0 * 1024.0) : 0.0;
			auto totalAvailable = (totalUsedGb / (allocatedFunds - unspentFunds)) * allocatedFunds;
      auto totalRemainGb = totalAvailable - totalUsedGb;
      ExecuteSetter(context, renterActions, "setEstimatedSpace", SiaCurrencyToGB(totalAvailable));
      ExecuteSetter(context, renterActions, "setAvailableSpace", SiaCurrencyToGB(totalRemainGb));
      ExecuteSetter(context, renterActions, "setUsedSpace", SiaCurrencyToGB(totalUsedGb));

      // Upload Progress
      /*
			SetRenterTotalUploadProgress(_siaApi->GetRenter()->GetTotalUploadProgress());
      */

      // Mount
      auto uiUpdate = global->GetValue("uiUpdate");
      auto drives = GetAvailableDrives();
      auto driveList = CefV8Value::CreateArray(drives.size());
      for (size_t i = 0; i < drives.size(); i++)
      {
        driveList->SetValue(i, CefV8Value::CreateString((drives[i] + ":\\").str()));
      }
      ExecuteSetter(context, uiUpdate, "setAvailableDriveLetters", driveList);
      
      // Renter settings
      auto allowance = _siaApi->GetRenter()->GetAllowance();
      auto obj = global->CreateObject(nullptr, nullptr);
      obj->SetValue("Funds", CefV8Value::CreateString(SiaCurrencyToString(allowance.Funds).str()), V8_PROPERTY_ATTRIBUTE_NONE);
      obj->SetValue("Hosts", CefV8Value::CreateString(SString::FromUInt64(allowance.Hosts).str()), V8_PROPERTY_ATTRIBUTE_NONE);
      obj->SetValue("Period", CefV8Value::CreateString(SString::FromUInt64(allowance.Period).str()), V8_PROPERTY_ATTRIBUTE_NONE);
      obj->SetValue("RenewWindowInBlocks", CefV8Value::CreateString(SString::FromUInt64(allowance.RenewWindowInBlocks).str()), V8_PROPERTY_ATTRIBUTE_NONE);
      ExecuteSetter(context, renterActions, "setAllowance", obj);

      // Display block height
      ExecuteSetter(context, uiActions, "setBlockHeight", SString::FromUInt64(_siaApi->GetConsensus()->GetHeight()));
    }
    context->Exit();
  }
  else
  {
    CefPostTask(TID_RENDERER, base::Bind(&CSiaDriveApp::SiaApiRefreshCallback, this, context, siaCurl, siaDriveConfig));
  }
}
