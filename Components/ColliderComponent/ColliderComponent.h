#pragma once
#include "Components/IComponent.h"
#include <string>
#include <cstdint>
#include <DebugingTools/IGizmoEditable.h>

namespace HEIN
{
	enum class ColliderShape
	{
		Sphere,
		AABB,
		OBB,
		Capsule,
		Mesh
	};

	// Collision Filtering Layers(using BitWise Shifts)
	namespace CollisionLayer 
	{
		constexpr uint32_t Layer_None = 0;
		constexpr uint32_t Layer_Default = 1 << 0;
		constexpr uint32_t Layer_Environment = 1 << 1;
		constexpr uint32_t Layer_Player = 1 << 2;
		constexpr uint32_t Layer_Enemy = 1 << 3;
		constexpr uint32_t Layer_PlayerWeapon = 1 << 4;
		constexpr uint32_t Layer_EnemyWeapon = 1 << 5;
		constexpr uint32_t Layer_All = ~0u; // Everything all Set to 1
	};

	class SkinnedModelComponent;

	class TransformComponent;

	class ColliderComponent : public IComponent, public IGizmoEditable
	{
	protected:
		
		DirectX::SimpleMath::Vector3 m_offset;
		DirectX::SimpleMath::Vector3 m_rotationEuler;
		DirectX::SimpleMath::Quaternion m_rotationOffset;
		ColliderShape m_shape;
		bool m_isTrigger;

		TransformComponent* m_transform = nullptr;
		
		DirectX::SimpleMath::Matrix m_manualMatrix;
		bool m_useManualMatrix = false;
		bool m_isCollidingThisFrame = false;

		std::wstring m_colliderTag = L"";

		uint32_t m_layer = CollisionLayer::Layer_Default;
		uint32_t m_mask = CollisionLayer::Layer_All;

	public:
		std::string GetComponentName() const override { return "ColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;


		ColliderComponent(Actor* owner, ColliderShape shape);
		virtual ~ColliderComponent() = default;

		void Start() override;
		void Update(float /*deltaTime*/) override {}

		void OnInspectorGUI(GameContext& gameContext) override;
		void DrawGizmo(
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			int operation,
			int mode
		) override;

		virtual void SyncColliderState() = 0;

		virtual void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override = 0;

		void SetManualMatrix(const DirectX::SimpleMath::Matrix& matirx)
		{
			m_manualMatrix = matirx;
			m_useManualMatrix = true;
		}

		ColliderShape GetShape() { return m_shape; }
		void SetTrigger(bool active) { m_isTrigger = active; }
		bool IsTrigger() const { return m_isTrigger; }
		void SetOffset(const DirectX::SimpleMath::Vector3& offset) { m_offset = offset; }
		DirectX::SimpleMath::Vector3 GetOffset() const { return m_offset; }
		void SetRotationOffset(const DirectX::SimpleMath::Vector3& rotation)
		{
			m_rotationEuler = rotation;
			m_rotationOffset = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
				rotation.y,
				rotation.x,
				rotation.z
			);
			m_rotationOffset.Normalize();
		}
		DirectX::SimpleMath::Vector3 GetRotationOffset() const { return m_rotationEuler; }

		std::wstring GetColliderTag() const { return m_colliderTag; }
		void SetColliderTag(std::wstring tag) { m_colliderTag = tag; }

		void SetCollidingThisFrame(bool isColliding) { m_isCollidingThisFrame = isColliding; }
		bool IsCollidingThisFrame() const { return m_isCollidingThisFrame; }

		void SetCollisionLayer(uint32_t layer) { m_layer = layer; }
		uint32_t GetCollisionLayer() const  { return m_layer; }

		void SetCollisionMask(uint32_t mask) { m_mask = mask; }
		uint32_t GetCollisionMask() const { return m_mask; }

		DirectX::SimpleMath::Matrix GetCalculateWorldMatrix() { return CalculateWorldMatrix(); }

	protected:

		DirectX::SimpleMath::Matrix CalculateWorldMatrix();
	};
}
