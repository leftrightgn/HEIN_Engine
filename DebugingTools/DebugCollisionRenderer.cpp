#include "pch.h"
#include "DebugCollisionRenderer.h"
#include "DirectXTK_Utilities/DebugDraw.h"


HEIN::DebugCollisionRenderer::DebugCollisionRenderer(
	bool modelActive,
	bool lineActive,
	uint32_t collisionMax
)
	: m_modelActive(modelActive)
	, m_lineActive(lineActive)
	, m_collisionMax(collisionMax)
{
	m_spheres.reserve(m_collisionMax);
	m_aabb.reserve(m_collisionMax);
	m_obb.reserve(m_collisionMax);
	m_meshes.reserve(m_collisionMax);
	m_lineSegments.reserve(m_collisionMax);
}

void HEIN::DebugCollisionRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
	m_modelSphere = DirectX::GeometricPrimitive::CreateSphere(context, 2.0f, 8);

	m_modelBox = DirectX::GeometricPrimitive::CreateCube(context);

	m_modelEffect = std::make_unique<DirectX::NormalMapEffect>(device);
	m_modelEffect->SetVertexColorEnabled(false);
	m_modelEffect->SetBiasedVertexNormals(false);
	m_modelEffect->SetInstancingEnabled(true);
	m_modelEffect->SetFogEnabled(false);
	m_modelEffect->SetTexture(nullptr);
	m_modelEffect->DisableSpecular();
	m_modelEffect->EnableDefaultLighting();
	m_modelEffect->SetWorld(DirectX::SimpleMath::Matrix::Identity);

	m_lineEffect = std::make_unique<DirectX::BasicEffect>(device);
	m_lineEffect->SetVertexColorEnabled(true);
	m_lineEffect->SetTextureEnabled(false);
	m_lineEffect->SetLightingEnabled(false);
	m_lineEffect->SetWorld(DirectX::SimpleMath::Matrix::Identity);

	m_meshEffect = std::make_unique<DirectX::BasicEffect>(device);
	m_meshEffect->SetVertexColorEnabled(false);
	m_meshEffect->SetTextureEnabled(false);
	m_meshEffect->SetLightingEnabled(false);

	const D3D11_INPUT_ELEMENT_DESC c_InputElements[] =
	{
		{ "SV_Position", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA,   0 },
		{ "InstMatrix",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "InstMatrix",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		{ "InstMatrix",  2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
	};
	DX::ThrowIfFailed(
		CreateInputLayoutFromEffect(device, m_modelEffect.get(),
			c_InputElements, std::size(c_InputElements),
			m_modelInputLayout.ReleaseAndGetAddressOf())
	);

	D3D11_BUFFER_DESC desc = CD3D11_BUFFER_DESC(
		static_cast<UINT>(DISPLAY_COLLISION_MAX * sizeof(DirectX::XMFLOAT3X4)),
		D3D11_BIND_VERTEX_BUFFER,
		D3D11_USAGE_DYNAMIC,
		D3D11_CPU_ACCESS_WRITE);
	DX::ThrowIfFailed(
		device->CreateBuffer(&desc, nullptr,
			m_instancedVB.ReleaseAndGetAddressOf())
	);

	m_meshBatch = std::make_unique<DirectX::PrimitiveBatch<DirectX::VertexPosition>>(context);

	DX::ThrowIfFailed(
		CreateInputLayoutFromEffect<DirectX::VertexPosition>(device, m_meshEffect.get(),
			m_meshInputLayout.ReleaseAndGetAddressOf())
	);

	m_primitiveBatch = std::make_unique<DirectX::PrimitiveBatch<DirectX::VertexPositionColor>>(context);

	DX::ThrowIfFailed(
		CreateInputLayoutFromEffect<DirectX::VertexPositionColor>(device, m_lineEffect.get(),
			m_lineInputLayout.ReleaseAndGetAddressOf())
	);


}

void HEIN::DebugCollisionRenderer::RenderAndFlush(
	ID3D11DeviceContext* context,
	const DirectX::CommonStates& states,
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj,
	DirectX::FXMVECTOR baseColor,
	DirectX::FXMVECTOR lineColor,
	float alpha
)
{
	DirectX::SimpleMath::Color color = baseColor;
	color.w = alpha;

	if (m_modelActive) DrawSolidShapes(context, states, view, proj, color);

	DirectX::SimpleMath::Color c = lineColor;
	if (c.w != 0.0f)
	{
		color = lineColor;
		color.w = alpha;
	}

	if (m_lineActive) DrawWireFrames(context, states, view, proj, color);

	m_spheres.clear();
	m_aabb.clear();
	m_obb.clear();
	m_meshes.clear();
	m_lineSegments.clear();
}
void HEIN::DebugCollisionRenderer::Clear()
{
	m_spheres.clear();
	m_aabb.clear();
	m_obb.clear();
	m_meshes.clear();
	m_lineSegments.clear();
}

void HEIN::DebugCollisionRenderer::DrawSolidShapes(
	ID3D11DeviceContext* context,
	const DirectX::CommonStates& states,
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj,
	DirectX::FXMVECTOR color
)
{
	m_modelEffect->SetColorAndAlpha(color);
	m_modelEffect->SetView(view);
	m_modelEffect->SetProjection(proj);
	m_modelEffect->Apply(context);

	// Draw Spheres
	size_t sphereDrawCount = m_spheres.size();
	if (sphereDrawCount > m_collisionMax) sphereDrawCount = m_collisionMax;

	if (sphereDrawCount > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		context->Map(m_instancedVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		DirectX::XMFLOAT3X4* p = static_cast<DirectX::XMFLOAT3X4*>(mappedResource.pData);

		for (size_t i = 0; i < sphereDrawCount; i++)
		{
			ZeroMemory(&p[i], sizeof(DirectX::XMFLOAT3X4));
			p[i]._11 = p[i]._22 = p[i]._33 = m_spheres[i].radius;
			p[i]._14 = m_spheres[i].center.x;
			p[i]._24 = m_spheres[i].center.y;
			p[i]._34 = m_spheres[i].center.z;
		}
		context->Unmap(m_instancedVB.Get(), 0);

		m_modelSphere->DrawInstanced(m_modelEffect.get(), m_modelInputLayout.Get(), static_cast<uint32_t>(sphereDrawCount), true, false, 0, [&]()
			{
				UINT stride = sizeof(DirectX::XMFLOAT3X4);
				UINT offset = 0;
				context->OMSetDepthStencilState(states.DepthRead(), 0);
				context->IASetVertexBuffers(1, 1, m_instancedVB.GetAddressOf(), &stride, &offset);
			});
	}

	// Draw AABBs and OBBs together 
	size_t totalBoxCount = m_aabb.size() + m_obb.size();
	size_t boxDrawCount = totalBoxCount;
	if (boxDrawCount > m_collisionMax) boxDrawCount = m_collisionMax;

	if (boxDrawCount > 0)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		context->Map(m_instancedVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		DirectX::XMFLOAT3X4* p = static_cast<DirectX::XMFLOAT3X4*>(mappedResource.pData);

		size_t writeIndex = 0;
	
		// Write AABBs (No rotation)
		for (size_t i = 0; i < m_aabb.size() && writeIndex < boxDrawCount; i++, writeIndex++)
		{
			DirectX::XMVECTOR scale = m_aabb[i].extents * 2.0f;
			DirectX::XMVECTOR translation = m_aabb[i].center;
			DirectX::XMVECTOR rotation = DirectX::XMQuaternionIdentity();
			DirectX::XMVECTOR zeroOrigin = DirectX::XMVectorZero();

			DirectX::SimpleMath::Matrix m = DirectX::XMMatrixAffineTransformation(scale, zeroOrigin, rotation, translation);

			p[writeIndex]._11 = m._11; p[writeIndex]._12 = m._21; p[writeIndex]._13 = m._31; p[writeIndex]._14 = m._41;
			p[writeIndex]._21 = m._12; p[writeIndex]._22 = m._22; p[writeIndex]._23 = m._32; p[writeIndex]._24 = m._42;
			p[writeIndex]._31 = m._13; p[writeIndex]._32 = m._23; p[writeIndex]._33 = m._33; p[writeIndex]._34 = m._43;
		}

		// Write OBBs (With rotation)
		for (size_t i = 0; i < m_obb.size() && writeIndex < boxDrawCount; i++, writeIndex++)
		{
			DirectX::XMVECTOR scale = m_obb[i].extents * 2.0f;
			DirectX::XMVECTOR translation = m_obb[i].center;
			DirectX::XMVECTOR rotation = m_obb[i].rotate;
			DirectX::XMVECTOR zeroOrigin = DirectX::XMVectorZero();

			DirectX::SimpleMath::Matrix m = DirectX::XMMatrixAffineTransformation(scale, zeroOrigin, rotation, translation);

			p[writeIndex]._11 = m._11; p[writeIndex]._12 = m._21; p[writeIndex]._13 = m._31; p[writeIndex]._14 = m._41;
			p[writeIndex]._21 = m._12; p[writeIndex]._22 = m._22; p[writeIndex]._23 = m._32; p[writeIndex]._24 = m._42;
			p[writeIndex]._31 = m._13; p[writeIndex]._32 = m._23; p[writeIndex]._33 = m._33; p[writeIndex]._34 = m._43;
		}

		context->Unmap(m_instancedVB.Get(), 0);

		m_modelBox->DrawInstanced(m_modelEffect.get(), m_modelInputLayout.Get(), static_cast<uint32_t>(boxDrawCount), true, false, 0, [&]()
			{
				UINT stride = sizeof(DirectX::XMFLOAT3X4);
				UINT offset = 0;
				context->OMSetDepthStencilState(states.DepthRead(), 0);
				context->IASetVertexBuffers(1, 1, m_instancedVB.GetAddressOf(), &stride, &offset);
			});
	}
}

void HEIN::DebugCollisionRenderer::DrawWireFrames(
	ID3D11DeviceContext* context,
	const DirectX::CommonStates& states,
	const DirectX::SimpleMath::Matrix& view,
	const DirectX::SimpleMath::Matrix& proj,
	DirectX::FXMVECTOR color
)
{
	context->OMSetBlendState(states.Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(states.DepthRead(), 0);
	context->RSSetState(states.CullNone());

	m_lineEffect->SetView(view);
	m_lineEffect->SetProjection(proj);
	m_lineEffect->Apply(context);

	context->IASetInputLayout(m_lineInputLayout.Get());
	m_primitiveBatch->Begin();

	// Draw Spheres
	for (size_t i = 0; i < m_spheres.size(); i++)
	{
		const SphereData& s = m_spheres[i];
		DirectX::BoundingSphere sphere(s.center, s.radius);

		DirectX::SimpleMath::Color lineColor = color;
		if (s.lineColor.w != 0.0f)
		{
			lineColor = s.lineColor;
		}

		DX::Draw(m_primitiveBatch.get(), sphere, lineColor);
	}

	// Draw AABBs
	for (size_t i = 0; i < m_aabb.size(); i++)
	{
		const AABBData& a = m_aabb[i];
		DirectX::BoundingBox box(a.center, a.extents);

		DirectX::SimpleMath::Color lineColor = color;
		if (a.lineColor.w != 0.0f)
		{
			lineColor = a.lineColor;
		}
		DX::Draw(m_primitiveBatch.get(), box, lineColor);
	}

	// Draw OBBs
	for (size_t i = 0; i < m_obb.size(); i++)
	{
		const OBBData& o = m_obb[i];
		DirectX::BoundingOrientedBox box(o.center, o.extents, o.rotate);
		DirectX::SimpleMath::Color lineColor = color;
		if (o.lineColor.w != 0.0f)
		{
			lineColor = o.lineColor;
		}
		DX::Draw(m_primitiveBatch.get(), box, lineColor);
	}

	// Draw Line Segments
	for (size_t i = 0; i < m_lineSegments.size(); i++)
	{
		const LineSegmentData& l = m_lineSegments[i];
		DirectX::SimpleMath::Color lineColor = color;
		if (l.lineColor.w != 0.0f)
		{
			lineColor = l.lineColor;
		}

		DirectX::VertexPositionColor verts[2];
		DirectX::XMStoreFloat3(&verts[0].position, DirectX::XMLoadFloat3(&l.a));
		DirectX::XMStoreFloat3(&verts[1].position, DirectX::XMLoadFloat3(&l.b));
		DirectX::XMStoreFloat4(&verts[0].color, lineColor);
		DirectX::XMStoreFloat4(&verts[1].color, lineColor);
		m_primitiveBatch->Draw(D3D_PRIMITIVE_TOPOLOGY_LINELIST, verts, 2);
	}

	m_primitiveBatch->End();

	// Draw Meshes
	context->OMSetBlendState(states.AlphaBlend(), nullptr, 0xFFFFFFFF);
	context->RSSetState(states.Wireframe());
	context->IASetInputLayout(m_meshInputLayout.Get());

	m_meshEffect->SetView(view);
	m_meshEffect->SetProjection(proj);

	m_meshBatch->Begin();

	for (size_t i = 0; i < m_meshes.size(); i++)
	{
		const MeshData& m = m_meshes[i];
		DirectX::SimpleMath::Color lineColor = color;
		if (m.lineColor.w != 0.0f)
		{
			lineColor = m.lineColor;
		}

		m_meshEffect->SetColorAndAlpha(lineColor);
		DirectX::SimpleMath::Matrix world = 
			DirectX::SimpleMath::Matrix::CreateScale(m.scale) *
			DirectX::SimpleMath::Matrix::CreateFromQuaternion(m.rotate) *
			DirectX::SimpleMath::Matrix::CreateTranslation(m.position);

		m_meshEffect->SetWorld(world);
		m_meshEffect->Apply(context);

		m_meshBatch->DrawIndexed(
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
			m.indexes.data(), m.indexes.size(),
			m.vertexes.data(), m.vertexes.size()
		);
	}

	m_meshBatch->End();
}