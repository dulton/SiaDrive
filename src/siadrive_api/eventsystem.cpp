#include <eventsystem.h>

using namespace Sia::Api;

CEventSystem CEventSystem::EventSystem;

CEventSystem::CEventSystem() :
#ifdef _WIN32
	_stopEvent(INVALID_HANDLE_VALUE)
#else
  a
#endif
{
}

CEventSystem::~CEventSystem()
{
	Stop();
#ifdef _WIN32
	::CloseHandle(_stopEvent);
#else
  a
#endif
}

void CEventSystem::ProcessEvents()
{
#ifdef _WIN32
  while (::WaitForSingleObject(_stopEvent, 10) == WAIT_TIMEOUT)
  {
    CEventPtr eventData;
    do
    {
      {
        std::lock_guard<std::mutex> l(_eventMutex);
        if (_eventQueue.size())
        {
          eventData = _eventQueue.front();
          _eventQueue.pop_front();
        }
        else
        {
          eventData.reset();
        }
      }

      if (eventData)
      {
        for (auto& v : _eventConsumers)
        {
          v(*eventData);
        }
        {
          std::lock_guard<std::mutex> l(_oneShotMutex);
          std::deque<std::function<bool(const CEvent&)>> consumerList;
          for (auto& v : _oneShotEventConsumers)
          {
            if (!v(*eventData))
            {
              consumerList.push_back(v);
            }
          }
          _oneShotEventConsumers = consumerList;
        }
      }
    } while (eventData);
  }
#else
  a
#endif
}

void CEventSystem::NotifyEvent(CEventPtr eventData)
{
	std::lock_guard<std::mutex> l(_eventMutex);
	if (_eventConsumers.size() || _oneShotEventConsumers.size())
	{
		_eventQueue.push_back(eventData);
	}
}

void CEventSystem::AddEventConsumer(std::function<void(const CEvent&)> consumer)
{
	if (!_processThread)
	{
		_eventConsumers.push_back(consumer);
	}
}

void CEventSystem::AddOneShotEventConsumer(std::function<bool(const CEvent&)> consumer)
{
  std::lock_guard<std::mutex> l(_oneShotMutex);
	_oneShotEventConsumers.push_back(consumer);
}

void CEventSystem::Start()
{
	if (!_processThread)
	{
#ifdef _WIN32
		_stopEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
#else
    a
#endif
		_processThread.reset(new std::thread([this]() {ProcessEvents(); }));
	}
}

void CEventSystem::Stop()
{
	if (_processThread)
	{
#ifdef _WIN32
		::SetEvent(_stopEvent);
#else
    a
#endif
		_processThread->join();
		_processThread.reset();
	}
}