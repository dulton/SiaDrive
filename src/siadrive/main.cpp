// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <siacommon.h>  
#include <siadriveapp.h>
#include <include/cef_app.h>
#include <eventsystem.h>
#include <debugconsumer.h>
#include <loggingconsumer.h>

using namespace Sia;
using namespace Sia::Api;

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR    lpCmdLine,
                      int       nCmdShow) 
{
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
#ifdef _DEBUG
  CDebugConsumer debugConsumer;
  CLoggingConsumer loggingConsumer(EventLevel::Debug);
#else
  CLoggingConsumer loggingConsumer(EventLevel::Release);
#endif
  CEventSystem::EventSystem.Start();
  CefEnableHighDPISupport();

  CefMainArgs mainArgs(hInstance);

	CefRefPtr<CSiaDriveApp> app(new CSiaDriveApp);
  int exitCode = CefExecuteProcess(mainArgs, app, nullptr);
  if (exitCode >= 0) {
    return exitCode;
  }
  CefSettings settings;
  settings.no_sandbox = true;
#ifdef _DEBUG
  settings.remote_debugging_port = 8080;
  settings.single_process = true;
  settings.log_severity = LOGSEVERITY_VERBOSE;
#else
  settings.log_severity = LOGSEVERITY_DISABLE;
#endif
  CefInitialize(mainArgs, settings, app, nullptr);
  
  CefRunMessageLoop();
  CefShutdown();
  CEventSystem::EventSystem.Stop();

  return 0;;
}
