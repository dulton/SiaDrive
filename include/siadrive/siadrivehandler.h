#ifndef _SIADRIVEHANDLER_H
#define _SIADRIVEHANDLER_H

#include <siacommon.h>
#include <include/cef_client.h>
#include <include/cef_load_handler.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_display_handler.h>

NS_BEGIN(Sia)
class CSiaDriveHandler : 
	public CefClient,
	public CefDisplayHandler,
	public CefLifeSpanHandler,
	public CefLoadHandler
{
public:
  explicit CSiaDriveHandler(const bool& useViews);

	~CSiaDriveHandler();

	// Provide access to the single global instance of this object.
	static CSiaDriveHandler* GetInstance();

	// CefClient methods:
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE 
	{
		return this;
	}

	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE 
	{
		return this;
	}
	
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE 
	{
		return this;
	}

	// CefDisplayHandler methods:
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) OVERRIDE;

	// CefLifeSpanHandler methods:
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;
	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) OVERRIDE;

	void CloseAllBrowsers(bool forceClose);

	bool IsClosing() const { return _isClosing; }

private:
	void PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title);
private:
	const bool _useViews;
	typedef std::list<CefRefPtr<CefBrowser> > BrowserList;
	BrowserList _browserList;

	bool _isClosing;

	IMPLEMENT_REFCOUNTING(CSiaDriveHandler);
};
NS_END(1)

#endif //_SIADRIVEHANDLER_H