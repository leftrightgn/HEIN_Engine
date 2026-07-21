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

		void Register(int actorID, IObserver* observer);

		void UnRegister(int actorID);

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

