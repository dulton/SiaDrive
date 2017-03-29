#ifndef _AUTOTHREAD_H
#define _AUTOTHREAD_H

#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class CSiaDriveConfig;
class CSiaCurl;
class SIADRIVE_EXPORTABLE CAutoThread
{
public:
	CAutoThread(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);
	CAutoThread(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig, std::function<void(const CSiaCurl&, CSiaDriveConfig*)> autoThreadCallback);

public:
	virtual ~CAutoThread();

private:
	std::unique_ptr<CSiaCurl> _siaCurl;
	CSiaDriveConfig* _siaDriveConfig;
#ifdef _WIN32
	HANDLE _stopEvent;
#endif
	std::unique_ptr<std::thread>  _thread;
	std::mutex _startStopMutex;
	std::function<void(const CSiaCurl&, CSiaDriveConfig*)> _AutoThreadCallback;

protected:
	virtual void AutoThreadCallback(const CSiaCurl& siaCurl, CSiaDriveConfig* siaDriveConfig);

public:
	SiaHostConfig GetHostConfig() const;
	void StartAutoThread();
	void StopAutoThread();
};

NS_END(2)
#endif //_AUTOTHREAD_H