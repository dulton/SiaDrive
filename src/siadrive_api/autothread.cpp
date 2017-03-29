#include <autothread.h>
#include <siacurl.h>

using namespace Sia::Api;

CAutoThread::CAutoThread(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig) :
	CAutoThread(siaCurl, siaDriveConfig, nullptr)
{
}

CAutoThread::CAutoThread(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, std::function<void(const CSiaCurl&, CSiaDriveConfig*)> autoThreadCallback) :
	_siaCurl(new CSiaCurl(siaCurl)),
	_siaDriveConfig(siaDriveConfig),
#ifdef _WIN32
	_stopEvent(::CreateEvent(nullptr, FALSE, FALSE, nullptr)),
#endif
	_AutoThreadCallback(autoThreadCallback)
{
}

CAutoThread::~CAutoThread()
{
	StopAutoThread();
#ifdef _WIN32
	::CloseHandle(_stopEvent);
#endif
}

SiaHostConfig CAutoThread::GetHostConfig() const
{
	return _siaCurl->GetHostConfig();
}

void CAutoThread::AutoThreadCallback(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig)
{
	if (_AutoThreadCallback)
	{
		_AutoThreadCallback(siaCurl, siaDriveConfig);
	}
}

void CAutoThread::StartAutoThread()
{
	std::lock_guard<std::mutex> l(_startStopMutex);
	if (!_thread)
	{
		_thread.reset(new std::thread([this]() {
#ifdef _WIN32
			do
			{
				AutoThreadCallback(*_siaCurl, _siaDriveConfig);
			} while (::WaitForSingleObject(_stopEvent, 2000) == WAIT_TIMEOUT);
#endif
		}));
	}
}

void CAutoThread::StopAutoThread()
{
	std::lock_guard<std::mutex> l(_startStopMutex);
	if (_thread)
	{
#ifdef _WIN32
		::SetEvent(_stopEvent);
#endif
		_thread->join();
		_thread.reset(nullptr);
	}
}