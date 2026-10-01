#include "pch.h"
#include "Actor.h"
#include "Components/IComponent.h"
#include <Components/TransformComponent.h>
#include <Components/SkinnedModelComponent.h>
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
	// Route update tick to all attached components sequentially
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
		if (!component->Is2D())
		{
			component->Draw(gameContext, world, view, proj);
		}
	}
}

void HEIN::Actor::Draw2D(GameContext& gameContext)
{
	for (auto& component : m_components)
	{
		if (component->Is2D())
		{
			component->Draw2D(gameContext);
		}
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

void HEIN::Actor::DrawInspector(GameContext& gameContext)
{
	HEIN::IComponent* compToRemove = nullptr;

	std::vector<HEIN::SkinnedModelComponent*> skinnedModels = GetComponents<HEIN::SkinnedModelComponent>();
	if (skinnedModels.size() > 1)
	{
		ImGui::Separator();
		ImGui::Text("Skinned Model Switcher (Key: M)");
		ImGui::Text("Active Model: [%d / %d]", m_activeSkinnedModelIndex + 1, static_cast<int>(skinnedModels.size()));
		if (ImGui::Button("Toggle Active Model"))
		{
			ToggleSkinnedModel();
		}
		ImGui::Separator();
	}

	for (auto& comp : m_components)
	{
		ImGui::PushID(comp.get());
		comp->OnInspectorGUI(gameContext);
		
		// it is important to prevent removing TransformComponent since the engine relies heavily on it being there!
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


HEIN::SkinnedModelComponent* HEIN::Actor::GetActiveSkinnedModel()
{
	std::vector<HEIN::SkinnedModelComponent*> models = GetComponents<HEIN::SkinnedModelComponent>();
	if (models.empty()) return nullptr;

	// Check if any model is explicitly marked visible
	for (size_t i = 0; i < models.size(); ++i)
	{
		if (models[i] && models[i]->IsVisible())
		{
			m_activeSkinnedModelIndex = static_cast<int>(i);
			return models[i];
		}
	}

	if (m_activeSkinnedModelIndex >= 0 && m_activeSkinnedModelIndex < static_cast<int>(models.size()))
	{
		return models[m_activeSkinnedModelIndex];
	}
	return models[0];
}

void HEIN::Actor::SetActiveSkinnedModelIndex(int index)
{
	std::vector<HEIN::SkinnedModelComponent*> models = GetComponents<HEIN::SkinnedModelComponent>();
	if (models.empty() || index < 0 || index >= static_cast<int>(models.size())) return;

	m_activeSkinnedModelIndex = index;
	for (int i = 0; i < static_cast<int>(models.size()); ++i)
	{
		if (models[i])
		{
			models[i]->SetVisible(i == index);
		}
	}
}

void HEIN::Actor::ToggleSkinnedModel()
{
	std::vector<HEIN::SkinnedModelComponent*> models = GetComponents<HEIN::SkinnedModelComponent>();
	if (models.empty()) return;

	if (models.size() == 1)
	{
		// Single model: toggle root motion directly
		bool current = models[0]->IsRootMotionEnabled();
		models[0]->SetEnableRootMotion(!current);
		char buf[128];
		sprintf_s(buf, "[ModelToggle] Single model: Root Motion toggled to %s\n", !current ? "ENABLED" : "DISABLED");
		OutputDebugStringA(buf);
		return;
	}

	// Multiple models: toggle active model index
	m_activeSkinnedModelIndex = (m_activeSkinnedModelIndex + 1) % static_cast<int>(models.size());
	for (int i = 0; i < static_cast<int>(models.size()); ++i)
	{
		if (models[i])
		{
			models[i]->SetVisible(i == m_activeSkinnedModelIndex);
		}
	}

	char buf[128];
	sprintf_s(buf, "[ModelToggle] Switched to SkinnedModel [%d] (Root Motion: %s)\n",
		m_activeSkinnedModelIndex,
		models[m_activeSkinnedModelIndex]->IsRootMotionEnabled() ? "ENABLED" : "DISABLED");
	OutputDebugStringA(buf);
}

template <>
HEIN::SkinnedModelComponent* HEIN::Actor::GetComponent<HEIN::SkinnedModelComponent>()
{
	return GetActiveSkinnedModel();
}
