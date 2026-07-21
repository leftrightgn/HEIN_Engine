#pragma once
#include <d3d11.h>  
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>


namespace HEIN
{
	class DebugRenderer
	{
	private:

		std::unique_ptr<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>> m_primitiveBatch;
		std::unique_ptr<DirectX::BasicEffect> m_basicEffect;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;

	public:

		DebugRenderer() = default;
		~DebugRenderer() = default;

		void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

		void Begin(const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection);
		void End();

		void DrawLine(
			const DirectX::SimpleMath::Vector3& startPos,
			const DirectX::SimpleMath::Vector3& endPos,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawVector(
			const DirectX::SimpleMath::Vector3& position,
			const DirectX::SimpleMath::Vector3& vector,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawSphere(
			const DirectX::BoundingSphere& sphere,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawBox(
			const DirectX::BoundingBox& box,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawOrientedBox(
			const DirectX::BoundingOrientedBox& obb,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawFrustum(
			const DirectX::BoundingFrustum& frustum,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);
		
		void DrawGrid(
			const DirectX::SimpleMath::Vector3& xAxis,
			const DirectX::SimpleMath::Vector3& yAxis,
			const DirectX::SimpleMath::Vector3& origin,
			size_t xdivs, size_t ydivs,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);
		
		void DrawRing(
			const DirectX::SimpleMath::Vector3& origin,
			const DirectX::SimpleMath::Vector3& majorAxis,
			const DirectX::SimpleMath::Vector3& minorAxis,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawQuad(
			const DirectX::SimpleMath::Vector3& pointA,
			const DirectX::SimpleMath::Vector3& pointB,
			const DirectX::SimpleMath::Vector3& pointC,
			const DirectX::SimpleMath::Vector3& pointD,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);

		void DrawRay(
			const DirectX::SimpleMath::Vector3& origin,
			const DirectX::SimpleMath::Vector3& direction,
			bool normalized,
			DirectX::FXMVECTOR color = DirectX::Colors::White
		);
	};

}