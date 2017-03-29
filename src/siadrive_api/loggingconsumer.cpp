#include <loggingconsumer.h>
#include <eventsystem.h>
#include <filepath.h>

using namespace Sia::Api;

CLoggingConsumer::CLoggingConsumer()
{
	CEventSystem::EventSystem.AddEventConsumer([=](const CEvent& event) {this->ProcessEvent(event); });
}


CLoggingConsumer::~CLoggingConsumer()
{
}

void CLoggingConsumer::ProcessEvent(const CEvent& eventData)
{
  // TODO Implement rolling/max size and timestamp
  FilePath logPath("logs\\siadrive.log");
  logPath.MakeAbsolute();
  FilePath(logPath).RemoveFileName().CreateDirectory();

  FILE* logFile;
	if (fopen_s(&logFile, SString::ToUtf8(static_cast<SString>(logPath)).c_str(), "a") == 0)
	{
    fprintf_s(logFile, SString::ToUtf8(eventData.GetSingleLineMessage() + "\n").c_str());
    fclose(logFile);
	}
}