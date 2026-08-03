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
    // Pass 1: Draw all 3D components for all actors
    for (std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& pair : m_actors)
    {
        pair.second->Draw(gameContext, view, proj);
    }

    // Pass 2: Draw all 2D UI components on top of 3D geometry
    for (std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& pair : m_actors)
    {
        pair.second->Draw2D(gameContext);
    }
}

void HEIN::ActorManager::DeleteActor(ActorID id)
{
    DestroyID(id);
    CleanUpDestroyedActors();
}

void HEIN::ActorManager::CleanUpDestroyedActors()
{
    if (m_pendingDestorys.empty()) return;

    for (ActorID deadID : m_pendingDestorys)
    {
        Actor* deadActor = GetActor(deadID);
        if (deadActor != nullptr)
        {
            ActorID parentID = deadActor->GetParentID();
            if (parentID != HEIN::INVALID_ACTOR_ID)
            {
                Actor* parent = GetActor(parentID);
                if (parent != nullptr)
                {
                    parent->RemoveChild(deadID);
                }
            }
        }
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

        // ponytail: Only serialize ROOT actors. Children are serialized recursively by their parents!
        if (actor != nullptr && actor->GetParentID() == HEIN::INVALID_ACTOR_ID)
        {
            actorsArray.push_back(actor->Serialize(this));
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

            HEIN::Actor* targetActor = GetActorByName(actorTag);
            
            if (targetActor == nullptr)
            {
                targetActor = CreateActor(actorTag);
            }

            if (targetActor != nullptr)
            {
                targetActor->Deserialize(actorData, this);
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

HEIN::Actor* HEIN::ActorManager::DuplicateActor(Actor* sourceActor, GameContext& gameContext, ActorID newParentID)
{
    if (sourceActor == nullptr) return nullptr;

    // Generate unique name for copy
    std::wstring baseName = sourceActor->GetTag();
    std::wstring copyName = baseName + L"_Copy";
    int count = 1;
    while (GetActorByName(copyName) != nullptr)
    {
        copyName = baseName + L"_Copy" + std::to_wstring(count++);
    }

    HEIN::Actor* newActor = CreateActor(copyName);
    if (!newActor) return nullptr;

    newActor->SetActorType(sourceActor->GetActorType());

    // Serialize source actor (WITHOUT children in the JSON, we duplicate children recursively)
    nlohmann::json actorJson = sourceActor->Serialize(nullptr);
    actorJson["Name"] = std::string(copyName.begin(), copyName.end());

    newActor->Deserialize(actorJson, this);

    // Parent assignment:
    ActorID parentToSet = (newParentID != INVALID_ACTOR_ID) ? newParentID : sourceActor->GetParentID();
    if (parentToSet != INVALID_ACTOR_ID)
    {
        Actor* parent = GetActor(parentToSet);
        if (parent)
        {
            newActor->SetParent(parent->GetID());
            parent->AddChild(newActor->GetID());
        }
    }

    // If this is the root duplicated object, offset position slightly so it does not perfectly overlap
    if (newParentID == INVALID_ACTOR_ID)
    {
        HEIN::TransformComponent* trans = newActor->GetComponent<HEIN::TransformComponent>();
        if (trans)
        {
            DirectX::SimpleMath::Vector3 pos = trans->GetPosition();
            pos.x += 1.0f;
            pos.z += 1.0f;
            trans->SetPosition(pos);
        }
    }

    // Recursively duplicate any children of sourceActor
    for (ActorID childID : sourceActor->GetChildren())
    {
        Actor* child = GetActor(childID);
        if (child)
        {
            DuplicateActor(child, gameContext, newActor->GetID());
        }
    }

    newActor->InitializeAfterDeserialize(gameContext);
    newActor->Start();

    return newActor;
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
