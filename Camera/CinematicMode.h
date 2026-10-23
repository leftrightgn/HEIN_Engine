#pragma once
#include "ICameraMode.h"
#include "Entities/Actor.h"
#include "Common/json.hpp"
#include <string>
#include <vector>

namespace HEIN
{
	class ActorManager;

	struct CinematicWaypoint
	{
		DirectX::SimpleMath::Vector3 eyePosition;
		DirectX::SimpleMath::Vector3 lookAtPosition;
		float fovDegrees = 50.0f;
		float duration = 2.5f;
	};

	struct CinematicTrack
	{
		std::string name = "Default";
		std::vector<CinematicWaypoint> waypoints;
	};

	class CinematicMode : public ICameraMode
	{
	private:
		static constexpr float DEFAULT_RADIUS = 8.0f;
		static constexpr float DEFAULT_CURRENTANGLE = 0.0f;
		static constexpr float DEFAULT_ORBITSPEED = 0.5f;
		static constexpr float DEFAULT_CAMERAHEIGHT = 1.5f;

	private:
		HEIN::ActorManager* m_manager = nullptr;
		HEIN::ActorID m_targetID = HEIN::INVALID_ACTOR_ID;

		DirectX::SimpleMath::Vector3 m_centerofStage;
		float m_radius;
		float m_currentAngle;
		float m_orbitSpeed;
		float m_cameraHeight;

		// Multi-Track Sequence Library
		std::vector<CinematicTrack> m_tracks;
		size_t m_activeTrackIndex = 0;

		size_t m_currentWaypointIndex = 0;
		float m_waypointTimer = 0.0f;
		bool m_isFinished = false;

		// Slot-based interactive editing & playback state
		size_t m_selectedSlotIndex = 0;
		bool m_isPlaying = false;

		// Interactive free-look and fly camera state for editing slots
		bool m_isDragging = false;
		float m_lastMouseX = 0.0f;
		float m_lastMouseY = 0.0f;

	public:
		CinematicMode(
			HEIN::ActorManager* manager,
			HEIN::ActorID targetID,
			DirectX::SimpleMath::Vector3 centerPoint,
			float radius = DEFAULT_RADIUS,
			float currentAngle = DEFAULT_CURRENTANGLE,
			float orbitSpeed = DEFAULT_ORBITSPEED,
			float cameraHeight = DEFAULT_CAMERAHEIGHT
		);

		void OnEnter(CameraData& data) override;
		void ProcessInput(const CameraInputState& input) override;
		void Update(CameraData& outData, float deltaTime, ICameraController& controller) override;

		CameraType GetType() const override { return CameraType::Cinematic; }

		// Track Management API
		size_t GetTrackCount() const { return m_tracks.size(); }
		const std::vector<CinematicTrack>& GetTracks() const { return m_tracks; }
		std::vector<CinematicTrack>& GetTracks() { return m_tracks; }
		size_t GetActiveTrackIndex() const { return m_activeTrackIndex; }
		void SetActiveTrackIndex(size_t index);
		bool SetActiveTrackByName(const std::string& name);
		CinematicTrack* GetActiveTrack();
		const CinematicTrack* GetActiveTrack() const;

		void AddTrack(const std::string& name);
		void RemoveTrack(size_t index);
		void DuplicateTrack(size_t index);
		void RenameTrack(size_t index, const std::string& newName);

		// Waypoint Access (operates on the active track)
		void ClearWaypoints();
		void AddWaypoint(const CinematicWaypoint& waypoint);
		void RemoveWaypoint(size_t index);
		void SetWaypoints(const std::vector<CinematicWaypoint>& waypoints);
		const std::vector<CinematicWaypoint>& GetWaypoints() const;
		std::vector<CinematicWaypoint>& GetWaypoints();

		bool HasCustomWaypoints() const;
		bool IsFinished() const { return m_isFinished; }
		size_t GetCurrentWaypointIndex() const { return m_currentWaypointIndex; }
		float GetWaypointTimer() const { return m_waypointTimer; }
		void ResetSequence();
		void Skip();

		// Interactive Slot Authoring & Playback API
		void SetSelectedSlotIndex(size_t index);
		size_t GetSelectedSlotIndex() const { return m_selectedSlotIndex; }
		bool IsPlaying() const { return m_isPlaying; }
		void Play();
		bool PlayTrack(const std::string& name);
		void Pause();

		// Target-Agnostic Track Generators (Reusable for any Boss, Gate, Item, or Landmark)
		void GenerateActorRevealTrack(
			const std::string& trackName,
			const DirectX::SimpleMath::Vector3& targetPos,
			const DirectX::SimpleMath::Vector3& viewerPos
		);
		void GenerateOrbitTrack(
			const std::string& trackName,
			const DirectX::SimpleMath::Vector3& targetPos,
			float radius = 8.0f,
			float height = 3.0f,
			float totalDuration = 6.0f
		);

		// Backwards-compatible Boss preset wrapper
		void SetupBossIntroPreset(
			const DirectX::SimpleMath::Vector3& bossPos,
			const DirectX::SimpleMath::Vector3& playerPos
		);

		// Serialization
		nlohmann::json SerializeTracks() const;
		void DeserializeTracks(const nlohmann::json& data);
	};
}
