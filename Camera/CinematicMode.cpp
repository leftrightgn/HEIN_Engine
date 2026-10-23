#include "pch.h"
#include "CinematicMode.h"
#include "Entities/ActorManager.h"
#include <algorithm>

HEIN::CinematicMode::CinematicMode(
	HEIN::ActorManager* manager,
	HEIN::ActorID targetID,
	DirectX::SimpleMath::Vector3 centerPoint,
	float radius, float currentAngle,
	float orbitSpeed,
	float cameraHeight
)
	: m_manager(manager)
	, m_targetID(targetID)
	, m_centerofStage(centerPoint)
	, m_radius(radius)
	, m_currentAngle(currentAngle)
	, m_orbitSpeed(orbitSpeed)
	, m_cameraHeight(cameraHeight)
	, m_tracks{}
	, m_activeTrackIndex(0)
	, m_currentWaypointIndex(0)
	, m_waypointTimer(0.0f)
	, m_isFinished(false)
	, m_selectedSlotIndex(0)
	, m_isPlaying(false)
	, m_isDragging(false)
	, m_lastMouseX(0.0f)
	, m_lastMouseY(0.0f)
{
	CinematicTrack defaultTrack;
	defaultTrack.name = "Boss_Intro";
	m_tracks.push_back(defaultTrack);
}

void HEIN::CinematicMode::OnEnter(CameraData& /*data*/)
{
	ResetSequence();
	m_isPlaying = false;
}

HEIN::CinematicTrack* HEIN::CinematicMode::GetActiveTrack()
{
	if (m_tracks.empty())
	{
		CinematicTrack defaultTrack;
		defaultTrack.name = "Default";
		m_tracks.push_back(defaultTrack);
		m_activeTrackIndex = 0;
	}
	if (m_activeTrackIndex >= m_tracks.size())
	{
		m_activeTrackIndex = m_tracks.size() - 1;
	}
	return &m_tracks[m_activeTrackIndex];
}

const HEIN::CinematicTrack* HEIN::CinematicMode::GetActiveTrack() const
{
	if (m_tracks.empty()) return nullptr;
	size_t idx = (m_activeTrackIndex < m_tracks.size()) ? m_activeTrackIndex : (m_tracks.size() - 1);
	return &m_tracks[idx];
}

void HEIN::CinematicMode::SetActiveTrackIndex(size_t index)
{
	if (index < m_tracks.size())
	{
		m_activeTrackIndex = index;
		m_selectedSlotIndex = 0;
		m_currentWaypointIndex = 0;
		m_waypointTimer = 0.0f;
		m_isPlaying = false;
		m_isFinished = false;
	}
}

bool HEIN::CinematicMode::SetActiveTrackByName(const std::string& name)
{
	for (size_t i = 0; i < m_tracks.size(); ++i)
	{
		if (m_tracks[i].name == name)
		{
			SetActiveTrackIndex(i);
			return true;
		}
	}
	return false;
}

void HEIN::CinematicMode::AddTrack(const std::string& name)
{
	CinematicTrack newTrack;
	newTrack.name = name.empty() ? ("Track_" + std::to_string(m_tracks.size() + 1)) : name;
	m_tracks.push_back(newTrack);
	SetActiveTrackIndex(m_tracks.size() - 1);
}

void HEIN::CinematicMode::RemoveTrack(size_t index)
{
	if (m_tracks.size() > 1 && index < m_tracks.size())
	{
		m_tracks.erase(m_tracks.begin() + index);
		if (m_activeTrackIndex >= m_tracks.size())
		{
			m_activeTrackIndex = m_tracks.size() - 1;
		}
		SetActiveTrackIndex(m_activeTrackIndex);
	}
}

void HEIN::CinematicMode::DuplicateTrack(size_t index)
{
	if (index < m_tracks.size())
	{
		CinematicTrack copy = m_tracks[index];
		copy.name += "_Copy";
		m_tracks.push_back(copy);
		SetActiveTrackIndex(m_tracks.size() - 1);
	}
}

void HEIN::CinematicMode::RenameTrack(size_t index, const std::string& newName)
{
	if (index < m_tracks.size() && !newName.empty())
	{
		m_tracks[index].name = newName;
	}
}

void HEIN::CinematicMode::ClearWaypoints()
{
	if (auto* track = GetActiveTrack())
	{
		track->waypoints.clear();
	}
	m_currentWaypointIndex = 0;
	m_selectedSlotIndex = 0;
	m_waypointTimer = 0.0f;
	m_isFinished = false;
	m_isPlaying = false;
}

void HEIN::CinematicMode::AddWaypoint(const CinematicWaypoint& waypoint)
{
	if (auto* track = GetActiveTrack())
	{
		track->waypoints.push_back(waypoint);
		m_selectedSlotIndex = track->waypoints.size() - 1;
	}
	m_isFinished = false;
}

void HEIN::CinematicMode::RemoveWaypoint(size_t index)
{
	if (auto* track = GetActiveTrack())
	{
		if (index < track->waypoints.size())
		{
			track->waypoints.erase(track->waypoints.begin() + index);
			if (track->waypoints.empty())
			{
				m_selectedSlotIndex = 0;
				m_currentWaypointIndex = 0;
			}
			else if (m_selectedSlotIndex >= track->waypoints.size())
			{
				m_selectedSlotIndex = track->waypoints.size() - 1;
			}
		}
	}
}

void HEIN::CinematicMode::SetWaypoints(const std::vector<CinematicWaypoint>& waypoints)
{
	if (auto* track = GetActiveTrack())
	{
		track->waypoints = waypoints;
	}
	m_currentWaypointIndex = 0;
	m_selectedSlotIndex = 0;
	m_waypointTimer = 0.0f;
	m_isFinished = false;
}

const std::vector<HEIN::CinematicWaypoint>& HEIN::CinematicMode::GetWaypoints() const
{
	static const std::vector<CinematicWaypoint> kEmpty{};
	if (const auto* track = GetActiveTrack())
	{
		return track->waypoints;
	}
	return kEmpty;
}

std::vector<HEIN::CinematicWaypoint>& HEIN::CinematicMode::GetWaypoints()
{
	return GetActiveTrack()->waypoints;
}

bool HEIN::CinematicMode::HasCustomWaypoints() const
{
	if (const auto* track = GetActiveTrack())
	{
		return !track->waypoints.empty();
	}
	return false;
}

void HEIN::CinematicMode::ResetSequence()
{
	m_currentWaypointIndex = 0;
	m_waypointTimer = 0.0f;
	m_isFinished = false;
}

void HEIN::CinematicMode::Skip()
{
	auto& waypoints = GetWaypoints();
	if (!waypoints.empty())
	{
		m_currentWaypointIndex = waypoints.size() - 1;
		m_selectedSlotIndex = m_currentWaypointIndex;
	}
	m_isFinished = true;
	m_isPlaying = false;
}

void HEIN::CinematicMode::Play()
{
	m_currentWaypointIndex = 0;
	m_waypointTimer = 0.0f;
	m_isPlaying = true;
	m_isFinished = false;
}

bool HEIN::CinematicMode::PlayTrack(const std::string& name)
{
	if (SetActiveTrackByName(name))
	{
		Play();
		return true;
	}
	// Fallback to active track if name not found
	Play();
	return false;
}

void HEIN::CinematicMode::Pause()
{
	m_isPlaying = false;
	m_selectedSlotIndex = m_currentWaypointIndex;
}

void HEIN::CinematicMode::SetSelectedSlotIndex(size_t index)
{
	auto& waypoints = GetWaypoints();
	if (index < waypoints.size())
	{
		m_selectedSlotIndex = index;
		m_currentWaypointIndex = index;
		m_isPlaying = false;
		m_isFinished = false;
	}
}

void HEIN::CinematicMode::GenerateActorRevealTrack(
	const std::string& trackName,
	const DirectX::SimpleMath::Vector3& targetPos,
	const DirectX::SimpleMath::Vector3& viewerPos
)
{
	// Find existing track or add new one
	if (!SetActiveTrackByName(trackName))
	{
		AddTrack(trackName);
	}

	ClearWaypoints();

	DirectX::SimpleMath::Vector3 toViewer = viewerPos - targetPos;
	toViewer.y = 0.0f;
	if (toViewer.LengthSquared() < 0.01f)
	{
		toViewer = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		toViewer.Normalize();
	}

	DirectX::SimpleMath::Vector3 rightDir = DirectX::SimpleMath::Vector3::Up.Cross(toViewer);

	// Shot 1: Low-angle upward reveal tracking silhouette
	CinematicWaypoint shotA;
	shotA.eyePosition = targetPos + toViewer * 7.5f + rightDir * 2.0f + DirectX::SimpleMath::Vector3(0.0f, 1.2f, 0.0f);
	shotA.lookAtPosition = targetPos + DirectX::SimpleMath::Vector3(0.0f, 3.2f, 0.0f);
	shotA.fovDegrees = 45.0f;
	shotA.duration = 2.4f;
	AddWaypoint(shotA);

	// Shot 2: Intermediate upward crane tracking head and torso
	CinematicWaypoint shotB;
	shotB.eyePosition = targetPos + toViewer * 5.0f - rightDir * 1.5f + DirectX::SimpleMath::Vector3(0.0f, 2.8f, 0.0f);
	shotB.lookAtPosition = targetPos + DirectX::SimpleMath::Vector3(0.0f, 4.0f, 0.0f);
	shotB.fovDegrees = 40.0f;
	shotB.duration = 2.0f;
	AddWaypoint(shotB);

	// Shot 3: Wide over-the-shoulder dramatic reveal framing both entities
	CinematicWaypoint shotC;
	shotC.eyePosition = viewerPos - toViewer * 4.5f + DirectX::SimpleMath::Vector3(0.0f, 2.6f, 0.0f);
	shotC.lookAtPosition = targetPos + DirectX::SimpleMath::Vector3(0.0f, 2.5f, 0.0f);
	shotC.fovDegrees = 52.0f;
	shotC.duration = 2.8f;
	AddWaypoint(shotC);

	m_selectedSlotIndex = 0;
	m_isPlaying = false;
}

void HEIN::CinematicMode::GenerateOrbitTrack(
	const std::string& trackName,
	const DirectX::SimpleMath::Vector3& targetPos,
	float radius,
	float height,
	float totalDuration
)
{
	if (!SetActiveTrackByName(trackName))
	{
		AddTrack(trackName);
	}

	ClearWaypoints();

	float shotDur = totalDuration / 4.0f;
	DirectX::SimpleMath::Vector3 lookTarget = targetPos + DirectX::SimpleMath::Vector3(0.0f, height * 0.5f, 0.0f);

	// 4 cardinal orbital angles
	CinematicWaypoint s0;
	s0.eyePosition = targetPos + DirectX::SimpleMath::Vector3(0.0f, height, radius);
	s0.lookAtPosition = lookTarget;
	s0.fovDegrees = 50.0f;
	s0.duration = shotDur;
	AddWaypoint(s0);

	CinematicWaypoint s1;
	s1.eyePosition = targetPos + DirectX::SimpleMath::Vector3(radius, height, 0.0f);
	s1.lookAtPosition = lookTarget;
	s1.fovDegrees = 50.0f;
	s1.duration = shotDur;
	AddWaypoint(s1);

	CinematicWaypoint s2;
	s2.eyePosition = targetPos + DirectX::SimpleMath::Vector3(0.0f, height, -radius);
	s2.lookAtPosition = lookTarget;
	s2.fovDegrees = 50.0f;
	s2.duration = shotDur;
	AddWaypoint(s2);

	CinematicWaypoint s3;
	s3.eyePosition = targetPos + DirectX::SimpleMath::Vector3(-radius, height, 0.0f);
	s3.lookAtPosition = lookTarget;
	s3.fovDegrees = 50.0f;
	s3.duration = shotDur;
	AddWaypoint(s3);

	m_selectedSlotIndex = 0;
	m_isPlaying = false;
}

void HEIN::CinematicMode::SetupBossIntroPreset(
	const DirectX::SimpleMath::Vector3& bossPos,
	const DirectX::SimpleMath::Vector3& playerPos
)
{
	GenerateActorRevealTrack("Boss_Intro", bossPos, playerPos);
}

void HEIN::CinematicMode::ProcessInput(const CameraInputState& input)
{
	auto& waypoints = GetWaypoints();
	if (m_isPlaying || waypoints.empty() || m_selectedSlotIndex >= waypoints.size())
	{
		return;
	}

	auto& activeWp = waypoints[m_selectedSlotIndex];

	// Free look rotation with Left Mouse button drag
	if (input.isLeftMouseDown)
	{
		if (!m_isDragging)
		{
			m_lastMouseX = input.mouseX;
			m_lastMouseY = input.mouseY;
			m_isDragging = true;
		}
		else
		{
			float deltaX = input.mouseX - m_lastMouseX;
			float deltaY = input.mouseY - m_lastMouseY;
			m_lastMouseX = input.mouseX;
			m_lastMouseY = input.mouseY;

			DirectX::SimpleMath::Vector3 forward = activeWp.lookAtPosition - activeWp.eyePosition;
			float dist = forward.Length();
			if (dist < 0.1f) dist = 5.0f;
			forward.Normalize();

			DirectX::SimpleMath::Vector3 right = DirectX::SimpleMath::Vector3::Up.Cross(forward);
			right.Normalize();

			DirectX::SimpleMath::Matrix rotYaw = DirectX::SimpleMath::Matrix::CreateFromAxisAngle(DirectX::SimpleMath::Vector3::Up, -deltaX * 0.005f);
			DirectX::SimpleMath::Matrix rotPitch = DirectX::SimpleMath::Matrix::CreateFromAxisAngle(right, -deltaY * 0.005f);

			forward = DirectX::SimpleMath::Vector3::TransformNormal(forward, rotYaw * rotPitch);
			forward.Normalize();
			activeWp.lookAtPosition = activeWp.eyePosition + forward * dist;
		}
	}
	else
	{
		m_isDragging = false;
	}

	// WASD 3D flight translation
	if (input.movementIntent.LengthSquared() > 0.0f && !input.ignoreMovement)
	{
		DirectX::SimpleMath::Vector3 forward = activeWp.lookAtPosition - activeWp.eyePosition;
		forward.y = 0.0f;
		if (forward.LengthSquared() < 0.001f) forward = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);
		forward.Normalize();
		DirectX::SimpleMath::Vector3 right = DirectX::SimpleMath::Vector3::Up.Cross(forward);
		right.Normalize();

		DirectX::SimpleMath::Vector3 moveDir = forward * input.movementIntent.z + right * input.movementIntent.x + DirectX::SimpleMath::Vector3::Up * input.movementIntent.y;
		float speed = 0.25f;
		activeWp.eyePosition += moveDir * speed;
		activeWp.lookAtPosition += moveDir * speed;
	}
}

void HEIN::CinematicMode::Update(CameraData& outData, float deltaTime, ICameraController& /*controller*/)
{
	auto& waypoints = GetWaypoints();

	if (!HasCustomWaypoints() || waypoints.empty())
	{
		m_currentAngle += m_orbitSpeed * deltaTime;
		float x = m_centerofStage.x + (cosf(m_currentAngle) * m_radius);
		float z = m_centerofStage.z + (sinf(m_currentAngle) * m_radius);

		outData.position = DirectX::SimpleMath::Vector3(x, m_centerofStage.y + m_cameraHeight, z);
		outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
			outData.position,
			m_centerofStage,
			DirectX::SimpleMath::Vector3::Up
		);

		DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
		outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
		outData.fov = DirectX::XMConvertToRadians(50.0f);
		return;
	}

	// Editing / View Slot Mode: Camera viewport displays and stays on the selected slot
	if (!m_isPlaying)
	{
		if (m_selectedSlotIndex >= waypoints.size())
		{
			m_selectedSlotIndex = waypoints.size() - 1;
		}

		const auto& slotWp = waypoints[m_selectedSlotIndex];
		outData.position = slotWp.eyePosition;
		outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
			outData.position,
			slotWp.lookAtPosition,
			DirectX::SimpleMath::Vector3::Up
		);
		DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
		outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
		outData.fov = DirectX::XMConvertToRadians(slotWp.fovDegrees);
		return;
	}

	// Playback Mode: Sequence finished
	if (m_isFinished)
	{
		const auto& finalWp = waypoints.back();
		outData.position = finalWp.eyePosition;
		outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
			outData.position,
			finalWp.lookAtPosition,
			DirectX::SimpleMath::Vector3::Up
		);
		DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
		outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
		outData.fov = DirectX::XMConvertToRadians(finalWp.fovDegrees);
		m_isPlaying = false;
		m_selectedSlotIndex = waypoints.size() - 1;
		return;
	}

	const CinematicWaypoint& currentWp = waypoints[m_currentWaypointIndex];
	m_waypointTimer += deltaTime;

	// Last waypoint hold
	if (m_currentWaypointIndex + 1 >= waypoints.size())
	{
		outData.position = currentWp.eyePosition;
		outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
			outData.position,
			currentWp.lookAtPosition,
			DirectX::SimpleMath::Vector3::Up
		);
		DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
		outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
		outData.fov = DirectX::XMConvertToRadians(currentWp.fovDegrees);

		if (m_waypointTimer >= currentWp.duration)
		{
			m_isFinished = true;
			m_isPlaying = false;
			m_selectedSlotIndex = waypoints.size() - 1;
		}
		return;
	}

	const CinematicWaypoint& nextWp = waypoints[m_currentWaypointIndex + 1];

	if (currentWp.duration <= 0.0001f)
	{
		m_currentWaypointIndex++;
		m_waypointTimer = 0.0f;
		return;
	}

	float t = std::clamp(m_waypointTimer / currentWp.duration, 0.0f, 1.0f);
	float smoothT = t * t * (3.0f - 2.0f * t);

	outData.position = DirectX::SimpleMath::Vector3::Lerp(currentWp.eyePosition, nextWp.eyePosition, smoothT);
	DirectX::SimpleMath::Vector3 lookAt = DirectX::SimpleMath::Vector3::Lerp(currentWp.lookAtPosition, nextWp.lookAtPosition, smoothT);

	outData.viewMatrix = DirectX::SimpleMath::Matrix::CreateLookAt(
		outData.position,
		lookAt,
		DirectX::SimpleMath::Vector3::Up
	);

	DirectX::SimpleMath::Matrix inverseView = outData.viewMatrix.Invert();
	outData.rotation = DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(inverseView);
	outData.fov = DirectX::XMConvertToRadians(std::lerp(currentWp.fovDegrees, nextWp.fovDegrees, smoothT));

	if (t >= 1.0f)
	{
		m_currentWaypointIndex++;
		m_waypointTimer = 0.0f;
	}
}

nlohmann::json HEIN::CinematicMode::SerializeTracks() const
{
	nlohmann::json root;
	root["activeTrackIndex"] = m_activeTrackIndex;
	root["selectedSlotIndex"] = m_selectedSlotIndex;
	nlohmann::json arr = nlohmann::json::array();
	for (const auto& track : m_tracks)
	{
		nlohmann::json trackJson;
		trackJson["name"] = track.name;
		nlohmann::json wpArr = nlohmann::json::array();
		for (const auto& wp : track.waypoints)
		{
			nlohmann::json wpJson;
			wpJson["eye"] = { wp.eyePosition.x, wp.eyePosition.y, wp.eyePosition.z };
			wpJson["lookAt"] = { wp.lookAtPosition.x, wp.lookAtPosition.y, wp.lookAtPosition.z };
			wpJson["fov"] = wp.fovDegrees;
			wpJson["duration"] = wp.duration;
			wpArr.push_back(wpJson);
		}
		trackJson["waypoints"] = wpArr;
		arr.push_back(trackJson);
	}
	root["tracks"] = arr;
	return root;
}

void HEIN::CinematicMode::DeserializeTracks(const nlohmann::json& data)
{
	const nlohmann::json* tracksArrayPtr = nullptr;
	if (data.is_object())
	{
		if (data.contains("activeTrackIndex") && data["activeTrackIndex"].is_number())
		{
			m_activeTrackIndex = data["activeTrackIndex"].get<size_t>();
		}
		if (data.contains("selectedSlotIndex") && data["selectedSlotIndex"].is_number())
		{
			m_selectedSlotIndex = data["selectedSlotIndex"].get<size_t>();
		}
		if (data.contains("tracks") && data["tracks"].is_array())
		{
			tracksArrayPtr = &data["tracks"];
		}
	}
	else if (data.is_array())
	{
		tracksArrayPtr = &data;
	}

	if (!tracksArrayPtr) return;

	m_tracks.clear();

	for (const auto& trackJson : *tracksArrayPtr)
	{
		if (!trackJson.contains("name")) continue;
		CinematicTrack track;
		track.name = trackJson["name"].get<std::string>();

		if (trackJson.contains("waypoints") && trackJson["waypoints"].is_array())
		{
			for (const auto& wpJson : trackJson["waypoints"])
			{
				CinematicWaypoint wp;
				if (wpJson.contains("eye") && wpJson["eye"].is_array() && wpJson["eye"].size() >= 3)
				{
					wp.eyePosition = DirectX::SimpleMath::Vector3(
						wpJson["eye"][0].get<float>(),
						wpJson["eye"][1].get<float>(),
						wpJson["eye"][2].get<float>()
					);
				}
				if (wpJson.contains("lookAt") && wpJson["lookAt"].is_array() && wpJson["lookAt"].size() >= 3)
				{
					wp.lookAtPosition = DirectX::SimpleMath::Vector3(
						wpJson["lookAt"][0].get<float>(),
						wpJson["lookAt"][1].get<float>(),
						wpJson["lookAt"][2].get<float>()
					);
				}
				if (wpJson.contains("fov") && wpJson["fov"].is_number())
				{
					wp.fovDegrees = wpJson["fov"].get<float>();
				}
				if (wpJson.contains("duration") && wpJson["duration"].is_number())
				{
					wp.duration = wpJson["duration"].get<float>();
				}
				track.waypoints.push_back(wp);
			}
		}
		m_tracks.push_back(track);
	}

	if (m_tracks.empty())
	{
		CinematicTrack defaultTrack;
		defaultTrack.name = "Boss_Intro";
		m_tracks.push_back(defaultTrack);
	}

	if (m_activeTrackIndex >= m_tracks.size())
	{
		m_activeTrackIndex = 0;
	}
	auto& waypoints = GetWaypoints();
	if (m_selectedSlotIndex >= waypoints.size() && !waypoints.empty())
	{
		m_selectedSlotIndex = 0;
	}
}
