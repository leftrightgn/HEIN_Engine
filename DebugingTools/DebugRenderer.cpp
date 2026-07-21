#include "pch.h"
#include "DebugRenderer.h"
#include "DirectXTK_Utilities/DebugDraw.h"

namespace HEIN
{
	void HEIN::DebugRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		m_primitiveBatch = std::make_unique<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(context);
		m_basicEffect = std::make_unique<DirectX::BasicEffect>(device);
		m_context = context;

		m_basicEffect->SetVertexColorEnabled(true);

		void const* shaderByteCode;
		size_t byteCodeLength;

		m_basicEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

		device->CreateInputLayout(
			DirectX::VertexPositionColor::InputElements,
			DirectX::VertexPositionColor::InputElementCount,
			shaderByteCode,
			byteCodeLength,
			m_inputLayout.ReleaseAndGetAddressOf()
		);
	}
	void DebugRenderer::Begin(const DirectX::SimpleMath::Matrix& view, const DirectX::SimpleMath::Matrix& projection)
	{
		m_basicEffect->SetView(view);

		m_basicEffect->SetProjection(projection);

		m_basicEffect->SetWorld(DirectX::SimpleMath::Matrix::Identity);

		m_basicEffect->Apply(m_context.Get());

		m_context->IASetInputLayout(m_inputLayout.Get());

		m_primitiveBatch->Begin();
	}
	void DebugRenderer::End()
	{
		m_primitiveBatch->End();
	}
	void DebugRenderer::DrawLine(
		const DirectX::SimpleMath::Vector3& startPos,
		const DirectX::SimpleMath::Vector3& endPos, 
		DirectX::FXMVECTOR color
	)
	{

		DirectX::VertexPositionColor vertex[2] =
		{
			{DirectX::SimpleMath::Vector3(startPos.x, startPos.y , startPos.z), color},
			{DirectX::SimpleMath::Vector3(startPos.x + endPos.x, startPos.y + endPos.y, startPos.z + endPos.z), color}
		};

		m_primitiveBatch->DrawLine(vertex[0], vertex[1]);
	}
	void DebugRenderer::DrawVector(
		const DirectX::SimpleMath::Vector3& position, 
		const DirectX::SimpleMath::Vector3& vector,
		DirectX::FXMVECTOR color
	)
	{
		const float cosTheta = cosf(DirectX::XMConvertToRadians(20.0f));
		const float sinTheta = sinf(DirectX::XMConvertToRadians(20.0f));

		DirectX::SimpleMath::Vector3 arrow = -vector;

		arrow.Normalize();

		arrow *= 3.0f;

		DirectX::SimpleMath::Vector3 arrowR =
			DirectX::SimpleMath::Vector3(
				arrow.x * cosTheta - arrow.z * sinTheta,
				arrow.y,
				arrow.x * sinTheta + arrow.z * cosTheta
			);

		DirectX::SimpleMath::Vector3 arrowL =
			DirectX::SimpleMath::Vector3(
				arrow.x * cosTheta + arrow.z * sinTheta,
				arrow.y,
			   -arrow.x * sinTheta + arrow.z * cosTheta
			);

		DrawLine(position, vector, color);
		DrawLine(position + vector, arrowR, color);
		DrawLine(position + vector, arrowL, color);
	     
	}
	void DebugRenderer::DrawSphere(const DirectX::BoundingSphere& sphere, DirectX::FXMVECTOR color)
	{
		DX::Draw(m_primitiveBatch.get(), sphere, color);
	}
	void DebugRenderer::DrawBox(const DirectX::BoundingBox& box, DirectX::FXMVECTOR color)
	{
		DX::Draw(m_primitiveBatch.get(), box, color);
	}
	void DebugRenderer::DrawOrientedBox(const DirectX::BoundingOrientedBox& obb, DirectX::FXMVECTOR color)
	{
		DX::Draw(m_primitiveBatch.get(), obb, color);
	}
	void DebugRenderer::DrawFrustum(const DirectX::BoundingFrustum& frustum, DirectX::FXMVECTOR color)
	{
		DX::Draw(m_primitiveBatch.get(), frustum, color);
	}
	void DebugRenderer::DrawGrid(
		const DirectX::SimpleMath::Vector3& xAxis,
		const DirectX::SimpleMath::Vector3& yAxis, 
		const DirectX::SimpleMath::Vector3& origin, 
		size_t xdivs, size_t ydivs, 
		DirectX::FXMVECTOR color)
	{
		DX::DrawGrid(m_primitiveBatch.get(), xAxis, yAxis, origin, xdivs, ydivs, color);
	}
	void DebugRenderer::DrawRing(
		const DirectX::SimpleMath::Vector3& origin, 
		const DirectX::SimpleMath::Vector3& majorAxis,
		const DirectX::SimpleMath::Vector3& minorAxis, 
		DirectX::FXMVECTOR color
	)
	{
		DX::DrawRing(m_primitiveBatch.get(), origin, majorAxis, minorAxis, color);
	}
	void DebugRenderer::DrawQuad(
		const DirectX::SimpleMath::Vector3& pointA, 
		const DirectX::SimpleMath::Vector3& pointB,
		const DirectX::SimpleMath::Vector3& pointC, 
		const DirectX::SimpleMath::Vector3& pointD,
		DirectX::FXMVECTOR color
	)
	{
		DX::DrawQuad(m_primitiveBatch.get(), pointA, pointB, pointC, pointD, color);
	}
	void DebugRenderer::DrawRay(
		const DirectX::SimpleMath::Vector3& origin, 
		const DirectX::SimpleMath::Vector3& direction, 
		bool normalized, 
		DirectX::FXMVECTOR color
	)
	{
		DX::DrawRay(m_primitiveBatch.get(), origin, direction, normalized, color);
	}
}
