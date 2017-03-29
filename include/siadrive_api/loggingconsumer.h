#ifndef _LOGGINGCONSUMER_H
#define _LOGGINGCONSUMER_H

#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class CEvent;
class SIADRIVE_EXPORTABLE CLoggingConsumer
{
public:
	CLoggingConsumer();

public:
	~CLoggingConsumer();

private:
	void ProcessEvent(const CEvent& eventData);
};

NS_END(2)
#endif //_LOGGINGCONSUMER_H