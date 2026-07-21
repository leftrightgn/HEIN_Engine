#pragma once
#include <Message/Message.h>

namespace HEIN
{
	class IObserver
	{
	public:

		virtual void OnMessageAccepted(Message::MessageID messageID) = 0;
	};

}
