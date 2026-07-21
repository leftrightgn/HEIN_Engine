#include "pch.h"
#include "Messenger.h"

std::unique_ptr<HEIN::Messenger> HEIN::Messenger::s_messenger = nullptr;

HEIN::Messenger::Messenger()
    :
    m_elapsedTime{},
    m_objects{},
    m_delayedMessages{}
{
}

HEIN::IObserver* HEIN::Messenger::GetObject(int actorID)
{
    std::unordered_map<int, std::vector<IObserver*>>::iterator it = m_objects.find(actorID);

    if (it != m_objects.end())
    {
        return it->second.front();
    }
    return nullptr;
}

HEIN::Messenger* HEIN::Messenger::GetInstance()
{
    if (s_messenger == nullptr)
    {
        s_messenger = std::unique_ptr<Messenger>(new Messenger());
    }
    return s_messenger.get();
}

void HEIN::Messenger::DestroyInstance()
{
    s_messenger.reset();
}

void HEIN::Messenger::Register(int actorID, IObserver* observer)
{
    m_objects[actorID].push_back(observer);
}

void HEIN::Messenger::UnRegister(int actorID)
{
    m_objects.erase(actorID);
}

void HEIN::Messenger::Notify(int actorID, Message::MessageID messageID)
{
    std::unordered_map<int, std::vector<IObserver*>>::iterator it = m_objects.find(actorID);

    if (it != m_objects.end())
    {
        for (size_t i = 0; i < it->second.size(); i++)
        {
            it->second[i]->OnMessageAccepted(messageID);
        }
    }
}

void HEIN::Messenger::NotifyAfterDelay(int actorID, Message::MessageID messageID, float delaySeconds)
{
    m_delayedMessages.push_back({ actorID, messageID, delaySeconds });
}

void HEIN::Messenger::UpdateDelayedMessage(float elapsedTime)
{
    for (std::vector<DelayedMessage>::iterator it = m_delayedMessages.begin();
        it != m_delayedMessages.end();)
    {
        it->delayTime -= elapsedTime;
        
        if (it->delayTime <= 0.0f)
        {
            Notify(it->ActorID, it->messageID);
            it = m_delayedMessages.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void HEIN::Messenger::Update(float elapsedTime)
{
    m_elapsedTime = elapsedTime;
    UpdateDelayedMessage(elapsedTime);
}
