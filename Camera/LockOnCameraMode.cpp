#include "pch.h"
#include "LockOnCameraMode.h"
#include "CameraCollisionHelper.h"
#include "Components/HealthComponent.h"
#include "Components/SkinnedModelComponent.h"
#include "Components/TargetTrackingComponent.h"
#include "Components/TransformComponent.h"
#include "Entities/ActorManager.h"
#include <algorithm>
#include <cmath>

HEIN::LockOnCameraMode::LockOnCameraMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID playerID,
	HEIN::ActorID targetID,
	float freq
)
	: m_manager(manager)
	, m_playerID(playerID)
	, m_targetID(targetID)
	, m_currentPosition(DirectX::SimpleMath::Vector3::Zero)
	, m_positionVelocity(DirectX::SimpleMath::Vector3::Zero)
	, m_currentFollowDistance(BASE_FOLLOW_DISTANCE)
	, m_currentFov(50.0f)
	, m_switchCooldown(0.0f)
	, m_currentLookTarget(DirectX::SimpleMath::Vector3::Zero)
	, m_currentOrbitDir(DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f))
	, m_stiffness(0.0f)
	, m_damping(0.0f)
	, m_isInitialized(false)
	, m_isLookTargetInitialized(false)
	, m_invalidFrames(0)
	, m_autoRetargetOnDeath(true)
{
	SetFrequency(freq);
}

void HEIN::LockOnCameraMode::OnEnter(CameraData& data)
{
	m_currentPosition = data.position;
	m_positionVelocity = DirectX::SimpleMath::Vector3::Zero;
	m_currentFollowDistance = BASE_FOLLOW_DISTANCE;
	m_currentFov = 50.0f;
	m_switchCooldown = 0.0f;
	m_isInitialized = true;
	m_isLookTargetInitialized = false;
	m_invalidFrames = 0;
	m_manualPitchOffset = 0.0f;
	m_manualYawOffset = 0.0f;
	m_currentOrbitDir = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);

	if (m_manager != nullptr)
	{
		HEIN::Actor* activePlayer = m_manager->GetActor(m_playerID);
		if (!activePlayer) activePlayer = m_manager->GetFirstActorByType(HEIN::ActorType::Player);
		if (activePlayer && activePlayer->GetActorType() == HEIN::ActorType::Ghost)
		{
			return;
		}
		if (m_playerID == HEIN::INVALID_ACTOR_ID || m_manager->GetActor(m_playerID) == nullptr)
		{
			HEIN::Actor* player = m_manager->GetFirstActorByType(HEIN::ActorType::Player);
			if (player) m_playerID = player->GetID();
		}

		if (m_targetID == HEIN::INVALID_ACTOR_ID || m_manager->GetActor(m_targetID) == nullptr)
		{
			HEIN::Actor* enemy = m_manager->GetFirstActorByType(HEIN::ActorType::Enemy);
			if (enemy)
			{
				m_targetID = enemy->GetID();
			}
			else
			{
				m_targetID = FindNextAutoTarget();
			}
		}

		if (m_targetID != HEIN::INVALID_ACTOR_ID)
		{
			HEIN::Actor* player = m_manager->GetActor(m_playerID);
			HEIN::Actor* target = m_manager->GetActor(m_targetID);
			if (player != nullptr)
			{
				auto* tracking = player->GetComponent<TargetTrackingComponent>();
				if (tracking != nullptr)
				{
					tracking->SetLockedOn(true);
					tracking->SetTargetID(m_targetID);
				}
			}
			if (player != nullptr && target != nullptr)
			{
				auto* pTrans = player->GetComponent<TransformComponent>();
				auto* tTrans = target->GetComponent<TransformComponent>();
				if (pTrans != nullptr && tTrans != nullptr)
				{
					DirectX::SimpleMath::Vector3 initialDiff = pTrans->GetPosition() - tTrans->GetPosition();
					initialDiff.y = 0.0f;
					if (initialDiff.LengthSquared() > 0.001f)
					{
						initialDiff.Normalize();
						m_currentOrbitDir = initialDiff;
					}

					float separationDistance = (pTrans->GetPosition() - tTrans->GetPosition()).Length();
					float targetDistance = std::clamp(BASE_FOLLOW_DISTANCE + separationDistance * 0.35f, MIN_FOLLOW_DISTANCE, MAX_FOLLOW_DISTANCE);
					m_currentFollowDistance = targetDistance;
					float heightOffset = std::clamp(14.0f + separationDistance * 0.06f, 8.0f, 40.0f);
					DirectX::SimpleMath::Vector3 desiredPos = pTrans->GetPosition()
						+ (m_currentOrbitDir * m_currentFollowDistance)
						+ (DirectX::SimpleMath::Vector3::Up * heightOffset);

					DirectX::SimpleMath::Vector3 focusOrigin = pTrans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, 8.0f, 0.0f);
					m_currentPosition = CameraCollisionHelper::ResolveOcclusion(
						m_manager,
						m_playerID,
						focusOrigin,
						desiredPos,
						0.5f,
						1.0f
					);
					m_positionVelocity = DirectX::SimpleMath::Vector3::Zero;

					DirectX::SimpleMath::Vector3 playerAnchor = pTrans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, 8.0f, 0.0f);
					DirectX::SimpleMath::Vector3 targetAnchor = tTrans->GetPosition() + DirectX::SimpleMath::Vector3(0.0f, 10.0f, 0.0f);
					m_currentLookTarget = DirectX::SimpleMath::Vector3::Lerp(playerAnchor, targetAnchor, 0.65f);
					m_isLookTargetInitialized = true;
				}
			}
		}
	}
}

void HEIN::LockOnCameraMode::OnExit(CameraData& /*data*/)
{
	if (m_manager != nullptr)
	{
		HEIN::Actor* player = m_manager->GetActor(m_playerID);
		if (!player)
		{
			player = m_manager->GetFirstActorByType(HEIN::ActorType::Player);
			if (!player) player = m_manager->GetFirstActorByType(HEIN::ActorType::Ghost);
		}
		if (player != nullptr)
		{
			if (auto* tracking = player->GetComponent<TargetTrackingComponent>())
			{
				tracking->SetLockedOn(false);
				tracking->SetTargetID(HEIN::INVALID_ACTOR_ID);
			}
		}
	}
}

void HEIN::LockOnCameraMode::ProcessInput(const CameraInputState& input)
{
	// Subtle mouse look adjustment so mouse input feels alive and responsive
	m_manualPitchOffset += -input.mouseY * 0.04f;
	m_manualPitchOffset = std::clamp(m_manualPitchOffset, -8.0f, 8.0f);

	m_manualYawOffset += -input.mouseX * 0.002f;
	m_manualYawOffset = std::clamp(m_manualYawOffset, -0.45f, 0.45f);

	if (m_switchCooldown > 0.0f) return;

	if (std::abs(input.mouseX) >= FLICK_THRESHOLD)
	{
		float flickDir = (input.mouseX > 0.0f) ? 1.0f : -1.0f;
		HEIN::ActorID newTarget = FindSwitchTarget(flickDir);
		if (newTarget != HEIN::INVALID_ACTOR_ID && newTarget != m_targetID)
		{
			m_targetID = newTarget;
			m_switchCooldown = 0.35f;

			if (m_manager != nullptr)
			{
				HEIN::Actor* player = m_manager->GetActor(m_playerID);
				if (player != nullptr)
				{
					auto* tracking = player->GetComponent<TargetTrackingComponent>();
					if (tracking != nullptr)
					{
						tracking->SetTargetID(newTarget);
					}
				}
			}
		}
	}
}

void HEIN::LockOnCameraMode::Update(CameraData& outData, float deltaTime, ICameraController& controller)
{
	if (m_switchCooldown > 0.0f)
	{
		m_switchCooldown -= deltaTime;
		if (m_switchCooldown < 0.0f) m_switchCooldown = 0.0f;
	}

	HEIN::Actor* playerActor = m_manager ? m_manager->GetActor(m_playerID) : nullptr;
	if (playerActor == nullptr && m_manager != nullptr)
	{
		playerActor = m_manager->GetFirstActorByType(HEIN::ActorType::Player);
		if (playerActor) m_playerID = playerActor->GetID();
	}
	if (playerActor == nullptr || playerActor->GetActorType() == HEIN::ActorType::Ghost)
	{
		if (playerActor != nullptr)
		{
			if (auto* tracking = playerActor->GetComponent<TargetTrackingComponent>())
			{
				tracking->SetLockedOn(false);
			}
		}
		controller.RequestSwitch(CameraType::Spring);
		return;
	}

	HEIN::Actor* targetActor = m_manager ? m_manager->GetActor(m_targetID) : nullptr;
	if (targetActor == nullptr && m_manager != nullptr)
	{
		targetActor = m_manager->GetFirstActorByType(HEIN::ActorType::Enemy);
		if (targetActor)
		{
			m_targetID = targetActor->GetID();
		}
		else if (m_autoRetargetOnDeath)
		{
			HEIN::ActorID nextTarget = FindNextAutoTarget();
			if (nextTarget != HEIN::INVALID_ACTOR_ID)
			{
				m_targetID = nextTarget;
				targetActor = m_manager->GetActor(m_targetID);
			}
		}
	}

	if (targetActor == nullptr)
	{
		m_invalidFrames++;
		if (m_invalidFrames > 5)
		{
			controller.RequestSwitch(CameraType::Spring);
		}
		return;
	}

	auto* targetHealth = targetActor->GetComponent<HealthComponent>();
	if (targetHealth && targetHealth->isDead())
	{
		if (m_autoRetargetOnDeath)
		{
			HEIN::ActorID nextTarget = FindNextAutoTarget();
			if (nextTarget != HEIN::INVALID_ACTOR_ID && nextTarget != m_targetID)
			{
				m_targetID = nextTarget;
				targetActor = m_manager->GetActor(m_targetID);
				m_switchCooldown = 0.35f;
				if (playerActor != nullptr)
				{
					auto* tracking = playerActor->GetComponent<TargetTrackingComponent>();
					if (tracking != nullptr)
					{
						tracking->SetTargetID(nextTarget);
					}
				}
			}
			else
			{
				controller.RequestSwitch(CameraType::Spring);
				return;
			}
		}
		else
		{
			controller.RequestSwitch(CameraType::Spring);
			return;
		}
	}

	m_invalidFrames = 0;

	if (auto* tracking = playerActor->GetComponent<TargetTrackingComponent>())
	{
		tracking->SetLockedOn(true);
		tracking->SetTargetID(m_targetID);
	}

	HEIN::TransformComponent* playerTrans = playerActor->GetComponent<HEIN::TransformComponent>();
	HEIN::TransformComponent* targetTrans = targetActor->GetComponent<HEIN::TransformComponent>();
	if (!playerTrans || !targetTrans)
	{
		controller.RequestSwitch(CameraType::Spring);
		return;
	}

	DirectX::SimpleMath::Vector3 playerPos = playerTrans->GetPosition();
	DirectX::SimpleMath::Vector3 targetPos = targetTrans->GetPosition();

	DirectX::SimpleMath::Vector3 diff = playerPos - targetPos;
	float separationDistance = diff.Length();

	if (separationDistance > MAX_LOCK_RANGE)
	{
		controller.RequestSwitch(CameraType::Spring);
		return;
	}

	DirectX::SimpleMath::Vector3 horizDiff = diff;
	horizDiff.y = 0.0f;
	float horizDist = horizDiff.Length();

	// Smoothly interpolate orbit direction to prevent jarring 180-degree flips
	if (horizDist > 1.5f)
	{
		DirectX::SimpleMath::Vector3 targetOrbit = horizDiff / horizDist;
		float orbitAlpha = 1.0f - std::exp(-8.0f * deltaTime);
		m_currentOrbitDir = DirectX::SimpleMath::Vector3::Lerp(m_currentOrbitDir, targetOrbit, orbitAlpha);
		if (m_currentOrbitDir.LengthSquared() > 0.001f)
		{
			m_currentOrbitDir.Normalize();
		}
	}
	else if (m_currentOrbitDir.LengthSquared() < 0.001f)
	{
		m_currentOrbitDir = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);
	}

	// Smoothly decay manual mouse look offsets back towards neutral framing
	m_manualYawOffset = std::lerp(m_manualYawOffset, 0.0f, 1.0f - std::exp(-2.5f * deltaTime));
	m_manualPitchOffset = std::lerp(m_manualPitchOffset, 0.0f, 1.0f - std::exp(-2.5f * deltaTime));

	DirectX::SimpleMath::Vector3 dirFromEnemyToPlayer = m_currentOrbitDir;
	if (std::abs(m_manualYawOffset) > 0.0001f)
	{
		DirectX::SimpleMath::Matrix yawRot = DirectX::SimpleMath::Matrix::CreateRotationY(m_manualYawOffset);
		dirFromEnemyToPlayer = DirectX::SimpleMath::Vector3::TransformNormal(dirFromEnemyToPlayer, yawRot);
	}

	float targetDistance = std::clamp(BASE_FOLLOW_DISTANCE + separationDistance * 0.35f, MIN_FOLLOW_DISTANCE, MAX_FOLLOW_DISTANCE);
	float distAlpha = 1.0f - std::exp(-5.0f * deltaTime);
	m_currentFollowDistance = std::lerp(m_currentFollowDistance, targetDistance, distAlpha);

	float heightOffset = 14.0f + m_manualPitchOffset + separationDistance * 0.06f;
	heightOffset = std::clamp(heightOffset, 8.0f, 40.0f);

	DirectX::SimpleMath::Vector3 desiredPosition = playerPos
		+ (dirFromEnemyToPlayer * m_currentFollowDistance)
		+ (DirectX::SimpleMath::Vector3::Up * heightOffset);

	// Occlusion resolution against level geometry
	DirectX::SimpleMath::Vector3 focusOrigin = playerPos + DirectX::SimpleMath::Vector3(0.0f, 8.0f, 0.0f);
	desiredPosition = CameraCollisionHelper::ResolveOcclusion(
		m_manager,
		m_playerID,
		focusOrigin,
		desiredPosition,
		0.5f,
		1.0f
	);

	if (!m_isInitialized)
	{
		m_currentPosition = desiredPosition;
		m_isInitialized = true;
	}

	UpdateSpring(desiredPosition, m_currentPosition, m_positionVelocity, deltaTime);

	DirectX::SimpleMath::Vector3 safePosition = CameraCollisionHelper::ResolveOcclusion(
		m_manager,
		m_playerID,
		focusOrigin,
		m_currentPosition,
		0.5f,
		1.0f
	);

	outData.position = safePosition;

	// Generic framing anchor: look toward mid-upper body between player and target
	DirectX::SimpleMath::Vector3 playerAnchor = playerPos + DirectX::SimpleMath::Vector3(0.0f, 8.0f, 0.0f);
	DirectX::SimpleMath::Vector3 targetAnchor = targetPos + DirectX::SimpleMath::Vector3(0.0f, 10.0f, 0.0f);
	DirectX::SimpleMath::Vector3 targetLookPos = DirectX::SimpleMath::Vector3::Lerp(playerAnchor, targetAnchor, 0.65f);

	// Temporal low-pass filter on look-at target to eliminate animation jitter
	if (!m_isLookTargetInitialized)
	{
		m_currentLookTarget = targetLookPos;
		m_isLookTargetInitialized = true;
	}
	else
	{
		float lookAlpha = 1.0f - std::exp(-16.0f * deltaTime);
		m_currentLookTarget = DirectX::SimpleMath::Vector3::Lerp(m_currentLookTarget, targetLookPos, lookAlpha);
	}

	DirectX::SimpleMath::Vector3 lookDir = m_currentLookTarget - outData.position;
	if (lookDir.LengthSquared() < 0.1f)
	{
		outData.position -= dirFromEnemyToPlayer * 5.0f;
	}

	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		m_currentLookTarget,
		DirectX::SimpleMath::Vector3::Up
	);

	outData.fov = DirectX::XMConvertToRadians(50.0f);

	DirectX::SimpleMath::Matrix invView = outData.viewMatrix.Invert();
	outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(invView);
}

HEIN::ActorID HEIN::LockOnCameraMode::FindNextAutoTarget() const
{
	if (!m_manager) return HEIN::INVALID_ACTOR_ID;

	HEIN::Actor* player = m_manager->GetActor(m_playerID);
	if (!player)
	{
		player = m_manager->GetFirstActorByType(HEIN::ActorType::Player);
		if (!player) player = m_manager->GetFirstActorByType(HEIN::ActorType::Ghost);
	}
	if (!player) return HEIN::INVALID_ACTOR_ID;

	auto* playerTrans = player->GetComponent<TransformComponent>();
	if (!playerTrans) return HEIN::INVALID_ACTOR_ID;

	DirectX::SimpleMath::Vector3 playerPos = playerTrans->GetPosition();
	HEIN::ActorID bestCandidateID = HEIN::INVALID_ACTOR_ID;
	float minDistance = MAX_LOCK_RANGE;

	bool currentTargetAlive = false;
	if (m_targetID != HEIN::INVALID_ACTOR_ID)
	{
		if (HEIN::Actor* curr = m_manager->GetActor(m_targetID))
		{
			auto* h = curr->GetComponent<HealthComponent>();
			if (!h || !h->isDead()) currentTargetAlive = true;
		}
	}

	const auto& allActors = m_manager->GetAllActors();
	for (const auto& [id, actorPtr] : allActors)
	{
		if (!actorPtr || actorPtr.get() == player) continue;
		if (currentTargetAlive && id == m_targetID) continue;
		if (actorPtr->GetActorType() != HEIN::ActorType::Enemy) continue;

		auto* health = actorPtr->GetComponent<HealthComponent>();
		if (health && health->isDead()) continue;

		auto* candidateTrans = actorPtr->GetComponent<TransformComponent>();
		if (!candidateTrans) continue;

		DirectX::SimpleMath::Vector3 candidatePos = candidateTrans->GetPosition();
		float dist = (candidatePos - playerPos).Length();
		if (dist < minDistance)
		{
			minDistance = dist;
			bestCandidateID = id;
		}
	}

	return bestCandidateID;
}

HEIN::ActorID HEIN::LockOnCameraMode::FindSwitchTarget(float flickDirection) const
{
	if (!m_manager) return HEIN::INVALID_ACTOR_ID;

	HEIN::Actor* player = m_manager->GetActor(m_playerID);
	HEIN::Actor* currentTarget = m_manager->GetActor(m_targetID);
	if (!player || !currentTarget) return HEIN::INVALID_ACTOR_ID;

	auto* playerTrans = player->GetComponent<TransformComponent>();
	auto* currTargetTrans = currentTarget->GetComponent<TransformComponent>();
	if (!playerTrans || !currTargetTrans) return HEIN::INVALID_ACTOR_ID;

	DirectX::SimpleMath::Vector3 playerPos = playerTrans->GetPosition();
	DirectX::SimpleMath::Vector3 currTargetPos = currTargetTrans->GetPosition();

	DirectX::SimpleMath::Vector3 toCurrTarget = currTargetPos - playerPos;
	toCurrTarget.y = 0.0f;
	if (toCurrTarget.LengthSquared() < 0.001f) return HEIN::INVALID_ACTOR_ID;
	toCurrTarget.Normalize();

	DirectX::SimpleMath::Vector3 camRight = DirectX::SimpleMath::Vector3::Up.Cross(toCurrTarget);
	camRight.Normalize();

	HEIN::ActorID bestCandidateID = HEIN::INVALID_ACTOR_ID;
	float bestScore = FLT_MAX;

	const auto& allActors = m_manager->GetAllActors();
	for (const auto& [id, actorPtr] : allActors)
	{
		if (!actorPtr || actorPtr.get() == player || id == m_targetID) continue;
		if (actorPtr->GetActorType() != HEIN::ActorType::Enemy) continue;

		auto* health = actorPtr->GetComponent<HealthComponent>();
		if (health && health->isDead()) continue;

		auto* candidateTrans = actorPtr->GetComponent<TransformComponent>();
		if (!candidateTrans) continue;

		DirectX::SimpleMath::Vector3 candidatePos = candidateTrans->GetPosition();
		DirectX::SimpleMath::Vector3 toCandidate = candidatePos - playerPos;
		float dist = toCandidate.Length();
		if (dist > MAX_LOCK_RANGE) continue;

		toCandidate.y = 0.0f;
		if (toCandidate.LengthSquared() < 0.001f) continue;
		toCandidate.Normalize();

		float lateralDot = toCandidate.Dot(camRight);

		if (flickDirection > 0.0f && lateralDot <= 0.05f) continue;
		if (flickDirection < 0.0f && lateralDot >= -0.05f) continue;

		float score = dist - std::abs(lateralDot) * 15.0f;
		if (score < bestScore)
		{
			bestScore = score;
			bestCandidateID = id;
		}
	}

	return bestCandidateID;
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
	float safeDt = std::clamp(elapsedTime, 0.0001f, 0.033f);
	DirectX::SimpleMath::Vector3 delta = target - current;
	DirectX::SimpleMath::Vector3 accel = (m_stiffness * delta) - (m_damping * velocity);
	velocity += accel * safeDt;
	current += velocity * safeDt;
}

void HEIN::LockOnCameraMode::SetTargetID(HEIN::ActorID target)
{
	m_targetID = target;
	if (m_manager != nullptr && m_targetID != HEIN::INVALID_ACTOR_ID)
	{
		HEIN::Actor* player = m_manager->GetActor(m_playerID);
		HEIN::Actor* targetActor = m_manager->GetActor(m_targetID);
		if (player != nullptr)
		{
			if (auto* tracking = player->GetComponent<TargetTrackingComponent>())
			{
				tracking->SetLockedOn(true);
				tracking->SetTargetID(m_targetID);
			}
		}
		if (player != nullptr && targetActor != nullptr)
		{
			auto* pTrans = player->GetComponent<TransformComponent>();
			auto* tTrans = targetActor->GetComponent<TransformComponent>();
			if (pTrans != nullptr && tTrans != nullptr)
			{
				DirectX::SimpleMath::Vector3 initialDiff = pTrans->GetPosition() - tTrans->GetPosition();
				initialDiff.y = 0.0f;
				if (initialDiff.LengthSquared() > 0.001f)
				{
					initialDiff.Normalize();
					m_currentOrbitDir = initialDiff;
				}
			}
		}
	}
}
