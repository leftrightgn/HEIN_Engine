#include "pch.h"
#include "TerrainCell.h"

HEIN::TerrainCell::TerrainCell()
	: m_vertexCount(0)
	, m_indexCount(0)
{
}

bool HEIN::TerrainCell::Initialize(ID3D11Device* device, void* vertexData, int vertexCount, void* indexData, int indexCount)
{
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;

	// Calculate the Bounding Box for the Culling
	DirectX::SimpleMath::Vector3 min(FLT_MAX, FLT_MAX, FLT_MAX);
	DirectX::SimpleMath::Vector3 max(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	TerrainVertexType* v = (TerrainVertexType*)vertexData;

	for (int i = 0; i < vertexCount; i++)
	{
		if (v[i].position.x < min.x) min.x = v[i].position.x;
		if (v[i].position.y < min.y) min.y = v[i].position.y;
		if (v[i].position.z < min.z) min.z = v[i].position.z;

		if (v[i].position.x > max.x) max.x = v[i].position.x;
		if (v[i].position.y > max.y) max.y = v[i].position.y;
		if (v[i].position.z > max.z) max.z = v[i].position.z;

	}

	m_boundingBox.Center = (min + max) * 0.5f;
	m_boundingBox.Extents = (max - min) * 0.5f;
	if (m_boundingBox.Extents.x < 1.0f) m_boundingBox.Extents.x = 1.0f;
	if (m_boundingBox.Extents.y < 2.0f) m_boundingBox.Extents.y = 2.0f;
	if (m_boundingBox.Extents.z < 1.0f) m_boundingBox.Extents.z = 1.0f;

	// Build DirectX Buffers
	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(TerrainVertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vData = {};
	vData.pSysMem = vertexData;
	DX::ThrowIfFailed(device->CreateBuffer(&vertexBufferDesc, &vData, m_vertexBuffer.ReleaseAndGetAddressOf()));

	D3D11_BUFFER_DESC indexBufferDesc = {};
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(uint32_t) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA iData = {};
	iData.pSysMem = indexData;
	DX::ThrowIfFailed(device->CreateBuffer(&indexBufferDesc, &iData, m_indexBuffer.ReleaseAndGetAddressOf()));

	return true;
}

void HEIN::TerrainCell::Draw(ID3D11DeviceContext* context)
{
	UINT stride = sizeof(TerrainVertexType);
	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	context->DrawIndexed(m_indexCount, 0, 0);
}
