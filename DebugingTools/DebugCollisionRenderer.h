#pragma once
#include <vector>
#include "SimpleMath.h"
#include "CommonStates.h"
#include "Effects.h"
#include "GeometricPrimitive.h"
#include "PrimitiveBatch.h"
#include "VertexTypes.h"
namespace HEIN
{
	class DebugCollisionRenderer
	{
	private:
		static const uint32_t DISPLAY_COLLISION_MAX = 200;

		bool m_modelActive;

		bool m_lineActive;

		uint32_t m_collisionMax;

		struct SphereData
		{
			DirectX::SimpleMath::Vector3 center;

			float radius;

			DirectX::SimpleMath::Color lineColor;

			constexpr SphereData(
				const DirectX::SimpleMath::Vector3& center,
				float radius,
				DirectX::SimpleMath::Color lineColor) noexcept
				: center(center), radius(radius), lineColor(lineColor) {
			}
		};

		struct AABBData
		{
			DirectX::SimpleMath::Vector3 center;

			DirectX::SimpleMath::Vector3 extents;

			DirectX::SimpleMath::Color lineColor;

			constexpr AABBData(
				const DirectX::SimpleMath::Vector3& center,
				const DirectX::SimpleMath::Vector3& extents,
				DirectX::SimpleMath::Color lineColor)noexcept
				: center(center), extents(extents), lineColor(lineColor) {
			}
		};
		struct OBBData
		{
			DirectX::SimpleMath::Vector3 center;

			DirectX::SimpleMath::Vector3 extents;

			DirectX::SimpleMath::Quaternion rotate;

			DirectX::SimpleMath::Color lineColor;

			constexpr OBBData(
				const DirectX::SimpleMath::Vector3& center,
				const DirectX::SimpleMath::Vector3& extents,
				const DirectX::SimpleMath::Quaternion& rotate,
				DirectX::SimpleMath::Color lineColor)noexcept
				: center(center), extents(extents), rotate(rotate), lineColor(lineColor) {}
		};

		struct MeshData
		{
			ID3D11Buffer* vertexBuffer;
			ID3D11Buffer* indexBuffer;
			UINT indexCount;
			DirectX::SimpleMath::Matrix world;
			DirectX::SimpleMath::Color lineColor;

			constexpr MeshData(
				ID3D11Buffer* vb,
				ID3D11Buffer* ib,
				UINT indexCount,
				const DirectX::SimpleMath::Matrix& world,
				DirectX::SimpleMath::Color lineColor) noexcept
				: vertexBuffer(vb), indexBuffer(ib), indexCount(indexCount), world(world), lineColor(lineColor) {}
		};

		struct LineSegmentData
		{
			DirectX::SimpleMath::Vector3 a;
			DirectX::SimpleMath::Vector3 b;
			DirectX::SimpleMath::Color lineColor;

			constexpr LineSegmentData(
				const DirectX::SimpleMath::Vector3& a,
				const DirectX::SimpleMath::Vector3& b,
				DirectX::SimpleMath::Color lineColor) noexcept
				: a(a), b(b), lineColor(lineColor) {}
		};

		std::vector<SphereData> m_spheres;

		std::vector<AABBData> m_aabb;

		std::vector<OBBData> m_obb;

		std::vector<MeshData> m_meshes;

		std::vector<LineSegmentData> m_lineSegments;
		
	    std::unique_ptr<DirectX::GeometricPrimitive> m_modelSphere;

		std::unique_ptr<DirectX::GeometricPrimitive> m_modelBox;

		std::unique_ptr<DirectX::NormalMapEffect> m_modelEffect;

		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_modelInputLayout;

		std::unique_ptr<DirectX::BasicEffect> m_meshEffect;

		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_meshInputLayout;

		std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPosition>> m_meshBatch;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_instancedVB;

		std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;

		std::unique_ptr<DirectX::BasicEffect> m_lineEffect;

		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_lineInputLayout;

		ID3D11Device* m_device = nullptr;
		ID3D11DeviceContext* m_context = nullptr;
		bool m_gpuInitialized = false;

    private:
		void EnsureResourcesCreated();

		void DrawSolidShapes(
			ID3D11DeviceContext* context,
			const DirectX::CommonStates& states,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			DirectX::FXMVECTOR color
		);

		void DrawWireFrames(
			ID3D11DeviceContext* context,
			const DirectX::CommonStates& states,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			DirectX::FXMVECTOR color
		);

    public:

		DebugCollisionRenderer(
			bool modelActive = false,
			bool lineActive = true,
			uint32_t collisionMax = DISPLAY_COLLISION_MAX
		);

		void Initialize(ID3D11Device* device,
			ID3D11DeviceContext* context);

		void RenderAndFlush(
			ID3D11DeviceContext* context,
			const DirectX::CommonStates& states,
			const DirectX::SimpleMath::Matrix& view,
			const DirectX::SimpleMath::Matrix& proj,
			DirectX::FXMVECTOR baseColor = DirectX::Colors::White,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f },
			float alpha = 0.5f
		);

		void Clear();
		void ReleaseDebugMemory();


		void QueueSphere(
			DirectX::BoundingSphere sphere,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			DirectX::XMFLOAT3 center = sphere.Center;
			m_spheres.push_back(SphereData(center, sphere.Radius, lineColor));
		}

		void QueueAABB(
			DirectX::BoundingBox box,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			m_aabb.push_back(AABBData(box.Center, box.Extents, lineColor));
		}

		void QueueOBB(
			DirectX::BoundingOrientedBox obb,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			m_obb.push_back(OBBData(obb.Center, obb.Extents, DirectX::SimpleMath::Quaternion(obb.Orientation), lineColor));
		}

		void QueueMesh(
			ID3D11Buffer* vertexBuffer,
			ID3D11Buffer* indexBuffer,
			UINT indexCount,
			const DirectX::SimpleMath::Matrix& world,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			if (!vertexBuffer || !indexBuffer || indexCount == 0) return;
			m_meshes.push_back(MeshData(vertexBuffer, indexBuffer, indexCount, world, lineColor));
		}

		void QueueMesh(
			ID3D11Buffer* vertexBuffer,
			ID3D11Buffer* indexBuffer,
			UINT indexCount,
			DirectX::SimpleMath::Vector3 scale,
			DirectX::SimpleMath::Vector3 position,
			DirectX::SimpleMath::Quaternion rotate,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			if (!vertexBuffer || !indexBuffer || indexCount == 0) return;
			DirectX::SimpleMath::Matrix world =
				DirectX::SimpleMath::Matrix::CreateScale(scale) *
				DirectX::SimpleMath::Matrix::CreateFromQuaternion(rotate) *
				DirectX::SimpleMath::Matrix::CreateTranslation(position);
			m_meshes.push_back(MeshData(vertexBuffer, indexBuffer, indexCount, world, lineColor));
		}

		void QueueLine(
			DirectX::SimpleMath::Vector3 a,
			DirectX::SimpleMath::Vector3 b,
			DirectX::FXMVECTOR lineColor = DirectX::XMVECTORF32{ 0.0f, 0.0f, 0.0f, 0.0f }
		)
		{
			m_lineSegments.push_back(LineSegmentData(a, b, lineColor));
		}

		bool HasQueuedData() const { return !m_lineSegments.empty() || !m_spheres.empty() || !m_aabb.empty() || !m_obb.empty() || !m_meshes.empty(); }

		void SetSolidRenderingEnable(bool active) { m_modelActive = active; }

		void SetWireFrameRenderingEnable(bool active) { m_lineActive = active; }
	};

}