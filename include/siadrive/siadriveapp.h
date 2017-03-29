#ifndef _SIADRIVEAPP_H
#define _SIADRIVEAPP_H

#include <siacommon.h>
#include <include/cef_app.h>

NS_BEGIN(Sia)
namespace Api
{
  class CAutoThread;
  class CSiaApi;
  class CSiaCurl;
  class CSiaDriveConfig;
#ifdef _WIN32
  namespace Dokan
  {
    class CSiaDokanDrive;
  }
#else
  a
#endif
}

class CSiaDriveApp : 
	public CefApp,
	public CefRenderProcessHandler,
	public CefBrowserProcessHandler
{
public:
	CSiaDriveApp();

  virtual ~CSiaDriveApp();

private:
  std::unique_ptr<Api::CAutoThread> _refreshThread;
  std::unique_ptr<Api::CSiaApi> _siaApi;
  std::unique_ptr<Api::CSiaCurl> _siaCurl;
  std::unique_ptr<Api::CSiaDriveConfig> _siaDriveConfig;
#ifdef _WIN32
  std::unique_ptr<Api::Dokan::CSiaDokanDrive> _siaDrive;
#else
  a
#endif
  bool _appStarted = false;
  SString _walletReceiveAddress;

public:
	// CefApp methods:
	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() OVERRIDE
	{
		return this;
	}

  virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() OVERRIDE 
  {
    return this;
  }

	// CefBrowserProcessHandler methods:
	virtual void OnContextInitialized() OVERRIDE;
  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) OVERRIDE;

private:
  static void ExecuteSetter(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> obj, const SString& method, const SString& value);
  static void ExecuteSetter(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> obj, const SString& method, CefRefPtr<CefV8Value> value);
  void ShutdownServices();
  void SiaApiRefreshCallback(CefRefPtr<CefV8Context> context, const Api::CSiaCurl& siaCurl, Api::CSiaDriveConfig* siaDriveConfig);

private:
	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(CSiaDriveApp);
};
NS_END(1)

#endif //_SIADRIVEAPP_H