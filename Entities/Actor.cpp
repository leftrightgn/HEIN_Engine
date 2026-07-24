#include "pch.h"
#include "Actor.h"
#include "Components/IComponent.h"
#include <Components/TransformComponent.h>
#include <Factory/ComponentFactory.h>


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
	for (auto& comp : m_components)
	{
		comp->OnInspectorGUI();
	}
}

nlohmann::json HEIN::Actor::Serialize()
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
	return actorData;
}

void HEIN::Actor::Deserialize(const nlohmann::json& actorData)
{
	if (actorData.contains("Name"))
	{
		std::string loadedName = actorData["Name"];

		m_tag = std::wstring(loadedName.begin(), loadedName.end());
	}

	if (actorData.contains("Components"))
	{
		for (const auto& compData : actorData["Components"])
		{
			std::string compType = compData["Type"];

			HEIN::IComponent* newComp = ComponentFactory::CreateComponent(compType, this);

			if (newComp != nullptr)
			{
				newComp->Deserialize(compData["Data"]);
			}
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
