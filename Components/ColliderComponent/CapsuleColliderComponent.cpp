#include "pch.h"
#include "Framework/GameContext.h"
#include "CapsuleColliderComponent.h"
#include <DirectXColors.h>

HEIN::CapsuleColliderComponent::CapsuleColliderComponent(Actor* owner)
	: ColliderComponent(owner, ColliderShape::Capsule)
	, m_radius(0.5f)
	, m_height(1.0f)
{
}

void HEIN::CapsuleColliderComponent::Initialize(float radius, float height)
{
	m_radius = radius;
	m_height = height;
}

void HEIN::CapsuleColliderComponent::SyncColliderState()
{
	DirectX::SimpleMath::Matrix worldMatrix = CalculateWorldMatrix();

	DirectX::SimpleMath::Vector3 center = worldMatrix.Translation();

	m_worldupDir = DirectX::SimpleMath::Vector3(worldMatrix._21, worldMatrix._22, worldMatrix._23);
	float upLengthSq = m_worldupDir.LengthSquared();
	if (upLengthSq > 0.0001f)
	{
		float upLen = std::sqrt(upLengthSq);
		m_worldupDir /= upLen;
	}

	m_worldrightDir = DirectX::SimpleMath::Vector3(worldMatrix._11, worldMatrix._12, worldMatrix._13);
	float rightLengthSq = m_worldrightDir.LengthSquared();
	if (rightLengthSq > 0.0001f)
	{
		float rightLen = std::sqrt(rightLengthSq);
		m_worldrightDir /= rightLen;
	}

	m_worldforwardDir = DirectX::SimpleMath::Vector3(worldMatrix._31, worldMatrix._32, worldMatrix._33);
	float forwardLengthSq = m_worldforwardDir.LengthSquared();
	if (forwardLengthSq > 0.0001f)
	{
		float forwardLen = std::sqrt(forwardLengthSq);
		m_worldforwardDir /= forwardLen;
	}

	m_worldTopCenter = center + (m_worldupDir * (m_height * 0.5f));

	m_worldBottomCenter = center - (m_worldupDir * (m_height * 0.5f));
}

void HEIN::CapsuleColliderComponent::Draw(
	GameContext& gameContext, 
	const DirectX::SimpleMath::Matrix& world,
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj
)
{
	if (gameContext.debugCollisionRenderer == nullptr) return;


	DirectX::SimpleMath::Color debugColor = DirectX::SimpleMath::Color(DirectX::Colors::Red);
	if (m_isCollidingThisFrame)
	{
		debugColor = DirectX::Colors::Yellow;
	}

	gameContext.debugCollisionRenderer->QueueLine(
		m_worldTopCenter + (m_worldrightDir * m_radius),
		m_worldBottomCenter + (m_worldrightDir * m_radius), 
		debugColor
	);
	gameContext.debugCollisionRenderer->QueueLine(
		m_worldTopCenter - (m_worldrightDir * m_radius), 
		m_worldBottomCenter - (m_worldrightDir * m_radius),
		debugColor
	);
	gameContext.debugCollisionRenderer->QueueLine(
		m_worldTopCenter + (m_worldforwardDir * m_radius),
		m_worldBottomCenter + (m_worldforwardDir * m_radius), 
		debugColor
	);
	gameContext.debugCollisionRenderer->QueueLine(
		m_worldTopCenter - (m_worldforwardDir * m_radius), 
		m_worldBottomCenter - (m_worldforwardDir * m_radius), 
		debugColor
	);

	// DRAW THE DOMES AND RINGS USING SINE WAVES ---
	const int segments = 16;

	// Starting points for the arcs
	DirectX::SimpleMath::Vector3 prevTopRight = m_worldTopCenter + (m_worldrightDir * m_radius);
	DirectX::SimpleMath::Vector3 prevTopForward = m_worldTopCenter + (m_worldforwardDir * m_radius);

	DirectX::SimpleMath::Vector3 prevBottomRight = m_worldBottomCenter + (m_worldrightDir * m_radius);
	DirectX::SimpleMath::Vector3 prevBottomForward = m_worldBottomCenter + (m_worldforwardDir * m_radius);

	DirectX::SimpleMath::Vector3 prevTopRing = m_worldTopCenter + (m_worldrightDir * m_radius);
	DirectX::SimpleMath::Vector3 prevBottomRing = m_worldBottomCenter + (m_worldforwardDir * m_radius);

	for (int i = 1; i <= segments; i++)
	{
		// Calculate the angles 
		float arcAngle = (DirectX::XM_PI) * ((float)i / segments); // 0 to 180 degrees (for the over-the-top arcs)
		float ringAngle = (DirectX::XM_2PI) * ((float)i / segments); // 0 to 360 degrees (for the flat rings)

		float cosArc = cos(arcAngle);
		float sinArc = sin(arcAngle);

		float cosRing = cos(ringAngle);
		float sinRing = sin(ringAngle);

		// TOP DOME (Uses +upDir)
		DirectX::SimpleMath::Vector3 nextTopRight =
			m_worldTopCenter + (m_worldrightDir * cosArc * m_radius) + (m_worldupDir * sinArc * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevTopRight, nextTopRight, debugColor);
		prevTopRight = nextTopRight;

		DirectX::SimpleMath::Vector3 nextTopForward = 
			m_worldTopCenter + (m_worldforwardDir * cosArc * m_radius) + (m_worldupDir * sinArc * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevTopForward, nextTopForward, debugColor);
		prevTopForward = nextTopForward;

		// BOTTOM BOWL (Uses -upDir) 
		DirectX::SimpleMath::Vector3 nextBottomRight = 
			m_worldBottomCenter + (m_worldrightDir * cosArc * m_radius) - (m_worldupDir * sinArc * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevBottomRight, nextBottomRight, debugColor);
		prevBottomRight = nextBottomRight;

		DirectX::SimpleMath::Vector3 nextBottomForward =
			m_worldBottomCenter + (m_worldforwardDir * cosArc * m_radius) - (m_worldupDir * sinArc * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevBottomForward, nextBottomForward, debugColor);
		prevBottomForward = nextBottomForward;

		// HORIZONTAL RINGS (The "Seams" connecting the domes to the cylinder)
		DirectX::SimpleMath::Vector3 nextTopRing = 
			m_worldTopCenter + (m_worldrightDir * cosRing * m_radius) + (m_worldforwardDir * sinRing * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevTopRing, nextTopRing, debugColor);
		prevTopRing = nextTopRing;

		DirectX::SimpleMath::Vector3 nextBottomRing =
			m_worldBottomCenter + (m_worldrightDir * cosRing * m_radius) + (m_worldforwardDir * sinRing * m_radius);
		gameContext.debugCollisionRenderer->QueueLine(prevBottomRing, nextBottomRing, debugColor);
		prevBottomRing = nextBottomRing;

		float ringSpacing = 2.0f;

		int numExtraRings = static_cast<int>(m_height / ringSpacing);

		if (numExtraRings > 0)
		{
			// Calculate the exact distance between rings
			float stepSize = m_height / (numExtraRings + 1);

			for (int r = 1; r <= numExtraRings; r++)
			{
				DirectX::SimpleMath::Vector3 ringCenter =
					m_worldBottomCenter + (m_worldupDir * (stepSize * r));

				DirectX::SimpleMath::Vector3 prevRingPoint =
					ringCenter + (m_worldrightDir * m_radius);

				for (int i = 1; i <= segments; i++)
				{
					float ringAngle = (DirectX::XM_2PI) * ((float)i / segments);

					float cosRing = cos(ringAngle);
					float sinRing = sin(ringAngle);

					DirectX::SimpleMath::Vector3 nextRingPoint = ringCenter +
						(m_worldrightDir * cosRing * m_radius) +
						(m_worldforwardDir * sinRing * m_radius);

					gameContext.debugCollisionRenderer->QueueLine(prevRingPoint, nextRingPoint, debugColor);
					prevRingPoint = nextRingPoint;
				}
			}
		}
	}
}

nlohmann::json HEIN::CapsuleColliderComponent::Serialize()
{
    nlohmann::json data = IComponent::Serialize();
    return data;
}

void HEIN::CapsuleColliderComponent::Deserialize(const nlohmann::json& data)
{
    IComponent::Deserialize(data);
}
