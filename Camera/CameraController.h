#pragma once
#include "ICameraMode.h"
#include "Components/IComponent.h"
#include "DebugingTools/IGizmoEditable.h"
#include "Entities/Actor.h"
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace HEIN
{
	class CameraController final : public ICameraController, public IComponent, public IGizmoEditable
	{
	private:
		enum class Command { None, Switch, Push, Pop };

		CameraData m_data;

		using CameraFactory = std::function<std::unique_ptr<ICameraMode>()>;
		std::unordered_map<CameraType, CameraFactory> m_factories;
		std::vector<std::unique_ptr<ICameraMode>> m_cameraStack;

		Command m_nextCommand = Command::None;
		std::optional<CameraType> m_nextCameraKey = std::nullopt;

		CameraData m_previousCameraData;
		bool m_isBlending = false;
		float m_blendDuration = 1.0f;
		float m_blendTimer = 0.0f;
		bool m_autoLockOn = true;
		nlohmann::json m_cinematicTracksJson;
		std::optional<CameraType> m_savedCameraType = std::nullopt;

	public:
		CameraController(Actor* owner);

		void Start() override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;
		std::string GetComponentName() const override { return "CameraController"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;
		void DrawGizmo(
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			int operation,
			int mode
		) override;

		void RegisterCamera(CameraType key, CameraFactory factory);
		void ClearRegisteredCameras() { m_factories.clear(); }

		// Sets the initial camera instantly
		void SetFirstCamera(CameraType key);

		void Update(float deltaTime) override;
		void ProcessInput(const CameraInputState& input);

		// Getters for GameScene.cpp
		DirectX::SimpleMath::Matrix GetView() const { return m_data.viewMatrix; }
		DirectX::SimpleMath::Quaternion GetRotation() const { return m_data.rotation; }
		DirectX::SimpleMath::Vector3 GetPosition() const { return m_data.position; }
		float GetFov() const { return m_data.fov; }

		void SetPosition(const DirectX::SimpleMath::Vector3& pos) { m_data.position = pos; }
		void SetRotation(const DirectX::SimpleMath::Quaternion& rot) { m_data.rotation = rot; }
		void SetFov(float fov) { m_data.fov = fov; }
		float GetBlendDuration() const { return m_blendDuration; }
		void SetBlendDuration(float duration) { m_blendDuration = duration; }
		bool IsBlending() const { return m_isBlending; }
		const CameraData& GetCameraData() const { return m_data; }
		bool IsAutoLockOnEnabled() const { return m_autoLockOn; }
		void SetAutoLockOnEnabled(bool enabled) { m_autoLockOn = enabled; }
		CameraData& GetCameraDataRef() { return m_data; }

		bool LocksPlayerRotation() const;

		void RequestSwitch(CameraType type) override;
		void RequestPush(CameraType type) override;
		void RequestPop(CameraType type) override;

		void UpdateMouseMode();

		std::optional<CameraType> GetCurrentCameraType() const
		{
			if (m_cameraStack.empty()) return std::nullopt;
			return m_cameraStack.back()->GetType();
		}

		ICameraMode* GetCurrentCameraMode() const
		{
			if (m_cameraStack.empty()) return nullptr;
			return m_cameraStack.back().get();
		}

	public:
		bool HasSavedCinematicTracks() const
		{
			if (m_cinematicTracksJson.is_array()) return !m_cinematicTracksJson.empty();
			if (m_cinematicTracksJson.is_object())
			{
				if (m_cinematicTracksJson.contains("tracks") && m_cinematicTracksJson["tracks"].is_array())
				{
					return !m_cinematicTracksJson["tracks"].empty();
				}
			}
			return false;
		}

		const nlohmann::json& GetSavedCinematicTracks() const { return m_cinematicTracksJson; }
		void SetSavedCinematicTracks(const nlohmann::json& tracks) { m_cinematicTracksJson = tracks; }

		std::optional<CameraType> GetSavedCameraType() const { return m_savedCameraType; }
		void SetSavedCameraType(CameraType type) { m_savedCameraType = type; }

		void ApplyRequest();
	};
}
