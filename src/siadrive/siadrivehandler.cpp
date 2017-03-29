#include <siadrivehandler.h>
#include <include/wrapper/cef_helpers.h>
#include <include/views/cef_browser_view.h>
#include <include/views/cef_window.h>
#include <include/cef_app.h>
#include <include/base/cef_bind.h>
#include <include/wrapper/cef_closure_task.h>

using namespace Sia;

CSiaDriveHandler* g_instance = nullptr;

CSiaDriveHandler::CSiaDriveHandler(const bool& useViews) :
	_useViews(useViews),
	_isClosing(false) 
{
	DCHECK(!g_instance);
	g_instance = this;
}

CSiaDriveHandler::~CSiaDriveHandler() 
{
	g_instance = nullptr;
}

CSiaDriveHandler* CSiaDriveHandler::GetInstance() 
{
	return g_instance;
}

void CSiaDriveHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) 
{
	CEF_REQUIRE_UI_THREAD();

	if (_useViews) 
	{
		CefRefPtr<CefBrowserView> browserView = CefBrowserView::GetForBrowser(browser);
		if (browserView) 
		{
			CefRefPtr<CefWindow> window = browserView->GetWindow();
			if (window)
				window->SetTitle(title);
		}
	}
	else 
	{
		PlatformTitleChange(browser, title);
	}
}

void CSiaDriveHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	_browserList.push_back(browser);
}

bool CSiaDriveHandler::DoClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	if (_browserList.size() == 1) 
	{
		// Set a flag to indicate that the window close should be allowed.
		_isClosing = true;
	}

	// Allow the close. For windowed browsers this will result in the OS close
	// event being sent.
	return false;
}

void CSiaDriveHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	BrowserList::iterator bit = _browserList.begin();
	for (; bit != _browserList.end(); ++bit) 
	{
		if ((*bit)->IsSame(browser)) 
		{
			_browserList.erase(bit);
			break;
		}
	}

	if (_browserList.empty()) 
	{
		CefQuitMessageLoop();
	}
}

void CSiaDriveHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	ErrorCode errorCode,
	const CefString& errorText,
	const CefString& failedUrl) 
{
	CEF_REQUIRE_UI_THREAD();

  if (errorCode != ERR_ABORTED)
  {
    std::stringstream ss;
    ss << "<html><body bgcolor=\"white\">"
      "<h2>Failed to load URL " << std::string(failedUrl) <<
      " with error " << std::string(errorText) << " (" << errorCode <<
      ").</h2></body></html>";
    frame->LoadString(ss.str(), failedUrl);
  }
}

void CSiaDriveHandler::CloseAllBrowsers(bool forceClose) 
{
	if (CefCurrentlyOn(TID_UI))
	{
    BrowserList::const_iterator it = _browserList.begin();
    for (; it != _browserList.end(); ++it)
      (*it)->GetHost()->CloseBrowser(forceClose);
	}
  else if (!_browserList.empty())
  {
		CefPostTask(TID_UI, base::Bind(&CSiaDriveHandler::CloseAllBrowsers, this, forceClose));
  }
}

#ifdef _WIN32
void CSiaDriveHandler::PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
	CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
	SetWindowText(hwnd, std::wstring(title).c_str());
}
#endif