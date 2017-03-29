#include <debugconsumer.h>
#include <eventsystem.h>

using namespace Sia::Api;

CDebugConsumer::CDebugConsumer()
{
	CEventSystem::EventSystem.AddEventConsumer([this](const CEvent& event) { this->ProcessEvent(event); });
}

CDebugConsumer::~CDebugConsumer()
{
}

void CDebugConsumer::ProcessEvent(const CEvent& eventData)
{
#ifdef _WIN32
	OutputDebugString(eventData.GetSingleLineMessage().str().c_str());
	OutputDebugString(L"\n");
#else
  a
#endif
}
