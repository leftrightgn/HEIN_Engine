#include "pch.h"
#include "ActorManager.h"
#include <Components/TransformComponent.h>


// LifeCycle
HEIN::Actor* HEIN::ActorManager::CreateActor(const std::wstring& tag)
{
    ActorID newID = m_nextID++;
    auto newActor = std::make_unique<Actor>(newID, tag);

    Actor* ptr = newActor.get();
    m_actors[newID] = std::move(newActor);

    return ptr;
}

void HEIN::ActorManager::DestroyID(ActorID id)
{
    // Check if it's already in the pending list to prevent infinite recursion
    if (std::find(m_pendingDestorys.begin(), m_pendingDestorys.end(), id) != m_pendingDestorys.end())
    {
        return;
    }

    // Don't destroy immediately Put in the pending lists
    m_pendingDestorys.push_back(id);

    // Cascade destroy to any actor that is owned by or a child of this actor
    for (const auto& pair : m_actors)
    {
        if (pair.second->GetOwnerID() == id || pair.second->GetParentID() == id)
        {
            DestroyID(pair.second->GetID());
        }
    }
}

HEIN::Actor* HEIN::ActorManager::GetActor(ActorID id)
{
    auto it = m_actors.find(id);
    if (it != m_actors.end())
    {
        return it->second.get();
    }
    return nullptr;
}

HEIN::Actor* HEIN::ActorManager::GetActorByName(const std::wstring& name)
{
    for (const std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& pair : m_actors)
    {
        
        if (pair.second->GetTag() == name)
        {
            return pair.second.get();
        }
    }
    return nullptr;
}

// Game Loop
void HEIN::ActorManager::UpdateAll(float deltaTime)
{
    for (auto& pair : m_actors)
    {
        pair.second->Update(deltaTime);
    }
}

void HEIN::ActorManager::LateUpdateAll(float deltaTime)
{
    for (auto& pair : m_actors)
    {
        pair.second->LateUpdate(deltaTime);
    }
}

void HEIN::ActorManager::UpdateAllHierarchies()
{
    for (auto& pair : m_actors)
    {
        Actor* actor = pair.second.get();

        if (actor->GetParentID() == HEIN::INVALID_ACTOR_ID)
        {
            CascadeTransforms(actor->GetID());
        }
    }
}

void HEIN::ActorManager::DrawAll(
    GameContext& gameContext,
    const DirectX::SimpleMath::Matrix& view, 
    const DirectX::SimpleMath::Matrix& proj
)
{
    for (std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& pair : m_actors)
    {
        pair.second->Draw(gameContext, view, proj);
    }
}

void HEIN::ActorManager::CleanUpDestroyedActors()
{
    for (ActorID deadID : m_pendingDestorys)
    {
        m_actors.erase(deadID);
    }
    m_pendingDestorys.clear();
}

nlohmann::json HEIN::ActorManager::Serialize()
{
    nlohmann::json sceneData;
    nlohmann::json actorsArray = nlohmann::json::array();

    for (auto& pair : m_actors)
    {
        HEIN::Actor* actor = pair.second.get();

        if (actor != nullptr)
        {
            actorsArray.push_back(actor->Serialize());
        }

    }
    sceneData["Actors"] = actorsArray;

    return sceneData;
}

void HEIN::ActorManager::Deserialize(const nlohmann::json& sceneData)
{
    if (sceneData.contains("Actors"))
    {
        for (const auto& actorData : sceneData["Actors"])
        {
            std::wstring actorTag = L"Unknown";

            if (actorData.contains("Name"))
            {
                std::string narrowTag = actorData["Name"];

                actorTag = std::wstring(narrowTag.begin(), narrowTag.end());
            }

            HEIN::Actor* newActor = CreateActor(actorTag);

            if (newActor != nullptr)
            {
                newActor->Deserialize(actorData);
            }
        }
        
    }
}

void HEIN::ActorManager::InitializeAfterDeserialize(GameContext& gameContext)
{
    for (auto& pair : m_actors)
    {
        pair.second->InitializeAfterDeserialize(gameContext);
    }
}

void HEIN::ActorManager::ClearAllActors()
{
    m_actors.clear();
    m_pendingDestorys.clear(); 
}

void HEIN::ActorManager::CascadeTransforms(ActorID parentID)
{
    Actor* parent = GetActor(parentID);
    if (parent == nullptr) return;

    HEIN::TransformComponent* parentTransform = parent->GetComponent<HEIN::TransformComponent>();
    if (parentTransform == nullptr) return;

    // Get the parent Final world Matrix
    DirectX::SimpleMath::Matrix parentWorld = parentTransform->GetWorldMatrix();

    const std::vector<ActorID>& children = parent->GetChildren();
    // Give the matrix to every child 
    for (size_t i = 0; i < children.size(); ++i)
    {
        Actor* child = GetActor(children[i]);
        if (child != nullptr)
        {
            HEIN::TransformComponent* childTransform = child->GetComponent<HEIN::TransformComponent>();

            if (childTransform != nullptr)
            {
                childTransform->SetParentMatrix(parentWorld);
            }

            CascadeTransforms(child->GetID());
        }
    }
}
