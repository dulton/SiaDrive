#ifndef _EVENTSYSTEM_H
#define _EVENTSYSTEM_H
#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

enum class EventLevel
{
  Error,
  Normal,
  Debug
};

class SIADRIVE_EXPORTABLE CEvent
{
public:
  CEvent(const EventLevel& eventLevel = EventLevel::Normal) :
    _eventLevel(eventLevel)
  {}

	virtual ~CEvent() {}

private:
  const EventLevel _eventLevel;

public:
  const EventLevel& GetEventLevel() const { return _eventLevel; }
	virtual SString GetSingleLineMessage() const = 0;
	virtual std::shared_ptr<CEvent> Clone() const = 0;
};

typedef std::shared_ptr<CEvent> CEventPtr;

#define CreateSystemEvent(E) CEventPtr(new E)
#define CreateSystemEventConsumer(E) [=](const CEvent&) -> void { E(e); }

// Singleton
class SIADRIVE_EXPORTABLE CEventSystem
{
private:
	CEventSystem();

private:
	~CEventSystem();

public:
	// Singleton setup
	CEventSystem(const CEventSystem&) = delete;
	CEventSystem(CEventSystem&&) = delete;
	CEventSystem& operator=(CEventSystem&&) = delete;
	CEventSystem& operator=(const CEventSystem&) = delete;

private:
#ifdef _WIN32
	HANDLE _stopEvent;
#endif
	std::deque<CEventPtr> _eventQueue;
  std::deque<std::function<void(const CEvent&)>> _eventConsumers;
  std::deque<std::function<bool(const CEvent&)>> _oneShotEventConsumers;
	std::mutex _eventMutex;
	std::unique_ptr<std::thread> _processThread;
  std::mutex _oneShotMutex;

public:
	static CEventSystem EventSystem;

private:
	void ProcessEvents();

public:
  void AddEventConsumer(std::function<void(const CEvent&)> consumer);
  void AddOneShotEventConsumer(std::function<bool(const CEvent&)> consumer);
	void NotifyEvent(CEventPtr eventData);
	void Start();
	void Stop();
};
NS_END(2)
#endif //_EVENTSYSTEM_H