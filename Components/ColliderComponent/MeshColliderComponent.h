#pragma once
#include "ColliderComponent.h"
#include <Common/CollisionMath.h>
#include <vector> 
#include <VertexTypes.h>

namespace HEIN
{
	class MeshColliderComponent : public ColliderComponent
	{
	private:

		std::vector<Triangle> m_localTriangles;
		std::vector<Triangle> m_worldTriangles;
		std::wstring m_objPath;

		std::vector<DirectX::VertexPosition> m_debugVertices;
		std::vector<uint16_t> m_debugIndices;

	public:
		std::string GetComponentName() const override { return "MeshColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI() override;


		MeshColliderComponent(Actor* owner);

		void LoadFromObj(const wchar_t* filePath);

		void Update(float /*deltaTime*/) override {}

		void SyncColliderState() override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		const std::vector<Triangle>& GetWorldTriangles() const{ return m_worldTriangles; }
	};
}


