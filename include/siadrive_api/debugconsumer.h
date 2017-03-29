#ifndef _DEBUGCONSUMER_H
#define _DEBUGCONSUMER_H

#include <siacommon.h>

NS_BEGIN(Sia)
NS_BEGIN(Api)

class CEvent;

class SIADRIVE_EXPORTABLE CDebugConsumer
{
public:
	CDebugConsumer();

public:
	~CDebugConsumer();

private:
	void ProcessEvent(const CEvent& eventData);
};

NS_END(2);
#endif //_DEBUGCONSUMER_H