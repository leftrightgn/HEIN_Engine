#pragma once
#include "ColliderComponent.h"
#include <Common/CollisionMath.h>
#include <vector> 
#include <VertexTypes.h>
#include <wrl/client.h>
#include <d3d11.h>

namespace HEIN
{
	class MeshColliderComponent : public ColliderComponent
	{
	private:

		std::vector<Triangle> m_localTriangles;
		std::vector<Triangle> m_worldTriangles;
		std::wstring m_objPath;
		DirectX::BoundingBox m_worldAABB;

		std::vector<DirectX::VertexPosition> m_debugVertices;
		std::vector<uint32_t> m_debugIndices;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
		UINT m_indexCount = 0;

		void CreateGpuBuffers(ID3D11Device* device);

	public:
	
		MeshColliderComponent(Actor* owner);

		void LoadFromObj(const wchar_t* filePath, ID3D11Device* device = nullptr);

		void Start() override;
		void Update(float /*deltaTime*/) override {}

		void SyncColliderState() override;

		void Draw(
			GameContext& gameContext,
			const DirectX::SimpleMath::Matrix& world,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj
		) override;

		std::string GetComponentName() const override { return "MeshColliderComponent"; }
		nlohmann::json Serialize() override;
		void Deserialize(const nlohmann::json& data) override;
		void OnInspectorGUI(GameContext& gameContext) override;


		const std::vector<Triangle>& GetWorldTriangles() const{ return m_worldTriangles; }
		bool IntersectsFrustum(const DirectX::BoundingFrustum& frustum) const override;
	};
}


