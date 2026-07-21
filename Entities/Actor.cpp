#include "pch.h"
#include "Actor.h"
#include "Components/IComponent.h"
#include <Components/TransformComponent.h>


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
