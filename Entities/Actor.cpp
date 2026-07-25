#include "pch.h"
#include "Actor.h"
#include "Components/IComponent.h"
#include <Components/TransformComponent.h>
#include <Factory/ComponentFactory.h>
#include "ActorManager.h"
#include <ImGui/imgui.h>


HEIN::Actor::Actor(ActorID id, const std::wstring& tag)
	: m_id(id)
	, m_tag(tag)
	, m_type(ActorType::Default)
{
}

void HEIN::Actor::Update(float deltaTime)
{
	for (auto& comp : m_components)
	{
		comp->Update(deltaTime);
	}
}

void HEIN::Actor::LateUpdate(float deltaTime)
{
	for (auto& comp : m_components)
	{
		comp->LateUpdate(deltaTime);
	}
}



void HEIN::Actor::Draw(GameContext& gameContext, const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& proj)
{
	TransformComponent* transform = GetComponent<TransformComponent>();
	if (!transform) return;

	DirectX::SimpleMath::Matrix world = transform->GetWorldMatrix();


	for (auto& component : m_components)
	{
		component->Draw(gameContext, world, view, proj);
	}
}

void HEIN::Actor::Start()
{
	// Initialize all attached components
	for (auto& comp : m_components)
	{
		comp->Start();
	}
}

void HEIN::Actor::DrawInspector()
{
	HEIN::IComponent* compToRemove = nullptr;

	for (auto& comp : m_components)
	{
		ImGui::PushID(comp.get());
		comp->OnInspectorGUI();
		
		// ponytail: We don't want them removing TransformComponent since the engine relies heavily on it being there!
		if (comp->GetComponentName() != "TransformComponent")
		{
			if (ImGui::Button("Remove Component", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
			{
				compToRemove = comp.get();
			}
		}

		ImGui::Separator();
		ImGui::PopID();
	}

	if (compToRemove)
	{
		RemoveComponent(compToRemove);
	}
}

nlohmann::json HEIN::Actor::Serialize(ActorManager* manager)
{
	nlohmann::json actorData;

	std::string narrowTag(m_tag.begin(), m_tag.end());
	actorData["Name"] = narrowTag;

	nlohmann::json componentArray = nlohmann::json::array();

	for (auto& compPtr : m_components)
	{
		HEIN::IComponent* comp = compPtr.get();
		std::string compName = comp->GetComponentName();

		if (compName != "Unknown")
		{
			nlohmann::json compData;
			compData["Type"] = compName;
			compData["Data"] = comp->Serialize();

			componentArray.push_back(compData);
		}
	}

	actorData["Components"] = componentArray;

	nlohmann::json childrenArray = nlohmann::json::array();
	if (manager != nullptr)
	{
		for (ActorID childID : m_childrensID)
		{
			Actor* child = manager->GetActor(childID);
			if (child != nullptr)
			{
				childrenArray.push_back(child->Serialize(manager));
			}
		}
	}
	actorData["Children"] = childrenArray;

	return actorData;
}

void HEIN::Actor::Deserialize(const nlohmann::json& actorData, ActorManager* manager)
{
	if (actorData.contains("Name"))
	{
		std::string loadedName = actorData["Name"];

		m_tag = std::wstring(loadedName.begin(), loadedName.end());
	}

	if (actorData.contains("Components"))
	{
		size_t existingCompIndex = 0;
		for (const auto& compData : actorData["Components"])
		{
			std::string compType = compData["Type"];

			HEIN::IComponent* targetComp = nullptr;

			// Sequential matching allows multiple components of the same type to map correctly
			while (existingCompIndex < m_components.size())
			{
				if (m_components[existingCompIndex]->GetComponentName() == compType)
				{
					targetComp = m_components[existingCompIndex].get();
					existingCompIndex++; // Advance for next component match
					break;
				}
				existingCompIndex++;
			}

			if (targetComp != nullptr)
			{
				if (compData.contains("Data") && !compData["Data"].is_null())
				{
					targetComp->Deserialize(compData["Data"]);
				}
			}
			else
			{
				HEIN::IComponent* newComp = ComponentFactory::CreateComponent(compType, this, manager);

				if (newComp != nullptr)
				{
					if (compData.contains("Data") && !compData["Data"].is_null())
					{
						newComp->Deserialize(compData["Data"]);
					}
				}
			}
		}
	}

	if (manager != nullptr && actorData.contains("Children"))
	{
		for (const auto& childData : actorData["Children"])
		{
			std::wstring childTag = L"Unknown";
			if (childData.contains("Name"))
			{
				std::string narrowTag = childData["Name"];
				childTag = std::wstring(narrowTag.begin(), narrowTag.end());
			}

			// Try to find the child if it already exists
			HEIN::Actor* childActor = manager->GetActorByName(childTag);
			if (childActor == nullptr)
			{
				childActor = manager->CreateActor(childTag);
				childActor->SetParent(GetID());
				AddChild(childActor->GetID());
			}
			
			childActor->Deserialize(childData, manager);
		}
	}
}

void HEIN::Actor::InitializeAfterDeserialize(GameContext& gameContext)
{
	for (auto& comp : m_components)
	{
		comp->InitializeAfterDeserialize(gameContext);
	}
}
