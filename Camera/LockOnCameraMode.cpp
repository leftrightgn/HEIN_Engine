#include "pch.h"
#include "LockOnCameraMode.h"
#include "Components/TransformComponent.h"
#include <Entities/ActorManager.h>


HEIN::LockOnCameraMode::LockOnCameraMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID playerID,
	HEIN::ActorID targetID,
	float freq
)
	: m_manager(manager)
	, m_playerID(playerID)
	, m_targetID(targetID)
	, m_heightOffset(DEFAULT_HEIGHT_OFFSET)
	, m_currentYaw(0.0f)
	, m_invalidFrames(0)
{
	SetFrequency(freq);
}

void HEIN::LockOnCameraMode::OnEnter(CameraData& data)
{
	m_currentPosition = data.position;
	m_positionVelocity = DirectX::SimpleMath::Vector3::Zero;
	m_isInitialized = true;
	m_invalidFrames = 0;
}


void HEIN::LockOnCameraMode::Update(CameraData& outData, float deltaTime, ICameraController& controller)
{
	HEIN::Actor* playerActor = m_manager->GetActor(m_playerID);

	if (playerActor == nullptr) 
	{
		controller.RequestSwitch(CameraType::ThirdPerson);
		return;
	}

	HEIN::TransformComponent* playerTrans = playerActor->GetComponent<HEIN::TransformComponent>();

	if (m_targetID == HEIN::INVALID_ACTOR_ID)
	{
		m_invalidFrames++;
		if (m_invalidFrames > 2)
		{
			controller.RequestSwitch(CameraType::ThirdPerson);
		}
		return;
	}
	m_invalidFrames = 0;
	HEIN::Actor* targetActor = m_manager->GetActor(m_targetID);
	if (targetActor == nullptr) 
	{
		controller.RequestSwitch(CameraType::Spring);
		return;
	}
	HEIN::TransformComponent* targetTrans = targetActor->GetComponent<HEIN::TransformComponent>();
	DirectX::SimpleMath::Vector3 playerPos = playerTrans->GetPosition();
	DirectX::SimpleMath::Vector3 targetPos = targetTrans->GetPosition();

	// calculate the direction form enemy to player
	DirectX::SimpleMath::Vector3 dirFromEnemyToPlayer = playerPos - targetPos;
	dirFromEnemyToPlayer.y = 0.0f;
	dirFromEnemyToPlayer.Normalize();

	// calculate the right vector for the shoulder offset
	DirectX::SimpleMath::Vector3 rightDir = DirectX::SimpleMath::Vector3::Up.Cross(dirFromEnemyToPlayer);
	rightDir.Normalize();

	float followDistance = 40.0f;
	float shoulderOffset = -20.0f;
	float heightOffset = 20.0f;

	DirectX::SimpleMath::Vector3 desiredPosition = playerPos
		+ (dirFromEnemyToPlayer * followDistance)
		+ (rightDir * shoulderOffset)
		+ (DirectX::SimpleMath::Vector3::Up * heightOffset);

	if (!m_isInitialized)
	{
		m_currentPosition = desiredPosition;
		m_isInitialized = true;
	}

	UpdateSpring(desiredPosition, m_currentPosition, m_positionVelocity, deltaTime);
	outData.position = m_currentPosition;

	// find both of the chest pos
	DirectX::SimpleMath::Vector3 enemyChest = targetPos + DirectX::SimpleMath::Vector3(0.0f, 2.5f, 0.0f);
	DirectX::SimpleMath::Vector3 playerChest = playerPos + DirectX::SimpleMath::Vector3(0.0f, 2.5f, 0.0f);

	// and find the mid point between them
	DirectX::SimpleMath::Vector3 midPoint = (playerChest + enemyChest) / 2.0f;

	float focalHorizontalOffset = 5.0f;
	DirectX::SimpleMath::Vector3 finalLookTarget = midPoint + (rightDir * focalHorizontalOffset);

	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		finalLookTarget, 
		DirectX::SimpleMath::Vector3::Up
	);

	outData.fov = DirectX::XMConvertToRadians(50.0f);
}

void HEIN::LockOnCameraMode::SetFrequency(float freq)
{
	m_stiffness = freq * freq;
	m_damping = DAMPING * freq;
}

void HEIN::LockOnCameraMode::UpdateSpring(
	const DirectX::SimpleMath::Vector3& target,
	DirectX::SimpleMath::Vector3& current,
	DirectX::SimpleMath::Vector3& velocity,
	float elapsedTime
) const
{
	DirectX::SimpleMath::Vector3 delta = target - current;
	DirectX::SimpleMath::Vector3 accel = (m_stiffness * delta) - (m_damping * velocity);
	velocity += accel * elapsedTime;
	current += velocity * elapsedTime;
}

