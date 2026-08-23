#pragma once
#include <Message/IObserver.h>
#include <unordered_map>
#include <memory>

namespace HEIN
{
	struct DelayedMessage
	{
		int ActorID;

		Message::MessageID messageID;

		float delayTime;

	};

	/// <summary>
	/// Singleton Publish-Subscribe message bus.
	/// Decouples systems (e.g. input handling from game logic) by allowing 
	/// observers to register for and receive events (messages) without tight coupling.
	/// </summary>
	class Messenger
	{
	private:

		static std::unique_ptr<Messenger> s_messenger;

		std::unordered_map<int, std::vector<IObserver*>> m_objects;

		float m_elapsedTime;

		std::vector<DelayedMessage> m_delayedMessages;

	public:
		IObserver* GetObject(int actorID);

		float GetElapsedTime() const { return m_elapsedTime; }

		void SetElapsedTime(const float& elapsedTime) { m_elapsedTime = elapsedTime; }

	public:

		static Messenger* GetInstance();

		static void DestroyInstance();

		/// <summary>
		/// Subscribes an observer to receive messages destined for a specific actor.
		/// </summary>
		void Register(int actorID, IObserver* observer);

		/// <summary>
		/// Removes all subscribed observers for a specific actor.
		/// </summary>
		void UnRegister(int actorID);

		/// <summary>
		/// Immediately broadcasts a message to all observers subscribed to the given actorID.
		/// </summary>
		void Notify(int actorID, Message::MessageID messageID);

		void NotifyAfterDelay(int actorID, Message::MessageID messageID, float delaySeconds);

		void UpdateDelayedMessage(float elapsedTime);

		void Update(float elapsedTime);

	private:

		Messenger(const Messenger&) = delete;
		Messenger& operator=(const Messenger&) = delete;
		Messenger(Messenger&&) = delete;
		Messenger& operator=(Messenger&&) = delete;

		Messenger();
	};
}

