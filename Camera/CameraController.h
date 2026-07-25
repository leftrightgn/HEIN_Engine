#pragma once
#include "ICameraMode.h"
#include <Components/IComponent.h>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include <Entities/Actor.h>

namespace HEIN
{
	class CameraController final : public ICameraController, public IComponent
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

	public:

		CameraController(Actor* owner);

		void Start() override;
		void InitializeAfterDeserialize(GameContext& gameContext) override;
		std::string GetComponentName() const override { return "CameraController"; }
		nlohmann::json Serialize() override { return nlohmann::json::object(); }
		void Deserialize(const nlohmann::json& data) override { IComponent::Deserialize(data); }

		void RegisterCamera(CameraType key, CameraFactory factory);
		

		// Sets the initial camera instantly
		void SetFirstCamera(CameraType key);
		

		void Update(float deltaTime) override;
		

		void ProcessInput(const CameraInputState& input);
		

		// Getters for GameScene.cpp
		DirectX::SimpleMath::Matrix GetView() const { return m_data.viewMatrix; }
		DirectX::SimpleMath::Quaternion GetRotation() const { return m_data.rotation; }
		DirectX::SimpleMath::Vector3 GetPosition() const { return m_data.position; }
		float GetFov() const { return m_data.fov; }

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

	private:

		void ApplyRequest();
		
		
		
	};
}