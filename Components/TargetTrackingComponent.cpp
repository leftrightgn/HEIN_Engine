#include "pch.h"
#include "TargetTrackingComponent.h"
#include "Components/HealthComponent.h"
#include "Components/TransformComponent.h"
#include "Entities/ActorManager.h"
#if HEIN_WITH_EDITOR
#include "ImGui/imgui.h"
#endif


namespace HEIN
{
HEIN::ActorID HEIN::TargetTrackingComponent::FindBestTarget() const
{
	if (m_actorManager == nullptr || m_transform == nullptr) return HEIN::INVALID_ACTOR_ID;

	DirectX::SimpleMath::Vector3 myPos = m_transform->GetPosition();
	HEIN::ActorID closestTargetID = HEIN::INVALID_ACTOR_ID;
	float minDistanceSq = FLT_MAX;

	const std::unordered_map<HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& allActors = m_actorManager->GetAllActors();
	for (const std::pair<const HEIN::ActorID, std::unique_ptr<HEIN::Actor>>& actorPair : allActors)
	{
		HEIN::Actor* actor = actorPair.second.get();

		if (actor == nullptr || actor == m_owner) continue;

		if (actor->GetActorType() == m_targetTypeToFind)
		{
			auto* health = actor->GetComponent<HealthComponent>();
			if (health && health->isDead()) continue;

			HEIN::TransformComponent* targetTrans = actor->GetComponent<HEIN::TransformComponent>();

			if (targetTrans != nullptr)
			{
				DirectX::SimpleMath::Vector3 targetPos = targetTrans->GetPosition();

				DirectX::SimpleMath::Vector3 dist = targetPos - myPos;
				float distSq = dist.LengthSquared();

				// Support arena-wide targeting range (600.0f units, matching MAX_LOCK_RANGE)
				if (distSq < 360000.0f && distSq < minDistanceSq)
				{
					minDistanceSq = distSq;
					closestTargetID = actor->GetID();
				}
			}
		}
	}

	return closestTargetID;
}

HEIN::TargetTrackingComponent::TargetTrackingComponent(
	Actor* owner,
	ActorManager* manager,
	HEIN::ActorType targetType
)
	: IComponent(owner)
	, m_actorManager(manager)
	, m_targetTypeToFind(targetType)
{
}

void HEIN::TargetTrackingComponent::Start()
{
	m_transform = m_owner->GetComponent<HEIN::TransformComponent>();
}

void HEIN::TargetTrackingComponent::Update(float /*deltaTime*/)
{
	if (!m_transform || !m_actorManager) return;

	if (m_isLockedOn && m_targetID == HEIN::INVALID_ACTOR_ID)
	{
		m_targetID = FindBestTarget();

		// If there is no valid enemy nearby, force the lock-on state off
		if (m_targetID == HEIN::INVALID_ACTOR_ID)
		{
			m_isLockedOn = false;
		}
	}
	else if (!m_isLockedOn)
	{
		m_targetID = HEIN::INVALID_ACTOR_ID;
		m_dirToTarget = DirectX::SimpleMath::Vector3::Zero;
		m_distanceToTarget = 0.0f;
	}

	if (m_targetID != HEIN::INVALID_ACTOR_ID)
	{
		HEIN::Actor* target = m_actorManager->GetActor(m_targetID);

		auto* health = target ? target->GetComponent<HealthComponent>() : nullptr;
		if (target != nullptr && (!health || !health->isDead()))
		{
			HEIN::TransformComponent* targetTrans = target->GetComponent<HEIN::TransformComponent>();

			if (targetTrans != nullptr)
			{
				DirectX::SimpleMath::Vector3 myPos = m_transform->GetPosition();
				DirectX::SimpleMath::Vector3 targetPos = targetTrans->GetPosition();

				DirectX::SimpleMath::Vector3 dir = targetPos - myPos;

				m_distanceToTarget = dir.Length();
				dir.y = 0.0f;

				if (dir.LengthSquared() > 0.001f)
				{
					dir.Normalize();
					m_dirToTarget = dir;
				}
			}
		}
		else
		{
			// Target is dead or missing; drop the lock-on cleanly
			m_targetID = HEIN::INVALID_ACTOR_ID;
			m_isLockedOn = false;
		}
	}
}

nlohmann::json HEIN::TargetTrackingComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    data["TargetType"] = static_cast<int>(m_targetTypeToFind);
    return data;
}

void HEIN::TargetTrackingComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
    if (data.contains("TargetType")) m_targetTypeToFind = static_cast<ActorType>(data["TargetType"]);
}

#if HEIN_WITH_EDITOR
void HEIN::TargetTrackingComponent::OnInspectorGUI(GameContext& /*gameContext*/)
{
    if (ImGui::CollapsingHeader("TargetTrackingComponent", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int type = static_cast<int>(m_targetTypeToFind);
        if (ImGui::Combo("Target Type", &type, "None\0Player\0Enemy\0Environment\0\0"))
        {
            m_targetTypeToFind = static_cast<ActorType>(type);
        }
        ImGui::Text("Is Locked On: %s", m_isLockedOn ? "True" : "False");
        ImGui::Text("Distance: %.2f", m_distanceToTarget);
    }
}
#else
void HEIN::TargetTrackingComponent::OnInspectorGUI(GameContext& /*gameContext*/) {}
#endif


} // namespace HEIN
