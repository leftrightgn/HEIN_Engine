#pragma once
#include <Message/Message.h>

namespace HEIN
{
	/// <summary>
	/// Interface for objects that wish to receive events from the Messenger system.
	/// Allows components or systems to react to decoupled messages (e.g., input actions or game events).
	/// </summary>
	class IObserver
	{
	public:

		/// <summary>
		/// Callback invoked by the Messenger when a subscribed event is broadcast.
		/// </summary>
		virtual void OnMessageAccepted(Message::MessageID messageID) = 0;
	};

}
